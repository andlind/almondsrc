#define _GNU_SOURCE
#include <stdbool.h>
#include <string.h>
#include <strings.h>     // strcasecmp
#include <stdlib.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <jwt.h>
#include <json-c/json.h>
#include "jwt_validate.h"
#include "logger.h"      // writeLog()
#include "configuration.h"   

// simple base64url -> base64 in-place
static void b64url_to_b64(char *s) {
    for (char *p = s; *p; p++) {
        if (*p == '-') *p = '+';
        else if (*p == '_') *p = '/';
    }
    int len = strlen(s);
    int pad = len % 4;
    if (pad == 2) strcat(s, "==");
    else if (pad == 3) strcat(s, "=");
}

// decode base64 (no newlines) using OpenSSL BIO
static int b64_decode(const char *in, unsigned char *out, size_t out_size) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO *bio = BIO_new_mem_buf(in, -1);
    bio = BIO_push(b64, bio);

    int len = BIO_read(bio, out, out_size);
    BIO_free_all(bio);
    return len;
}

int jwt_check_roles(struct json_object *payload,
                    char **acceptedRoles,
                    int acceptedCount)
{
    struct json_object *resource_access = NULL;
    if (!json_object_object_get_ex(payload, "resource_access", &resource_access))
        return 0;

    struct json_object *client_obj = NULL;
    if (!json_object_object_get_ex(resource_access, "almond-api", &client_obj))
        return 0;

    struct json_object *roles = NULL;
    if (!json_object_object_get_ex(client_obj, "roles", &roles))
        return 0;

    if (!json_object_is_type(roles, json_type_array))
        return 0;

    int roleCount = json_object_array_length(roles);

    for (int i = 0; i < roleCount; i++) {
        struct json_object *roleObj = json_object_array_get_idx(roles, i);
        if (!json_object_is_type(roleObj, json_type_string))
            continue;

        const char *role = json_object_get_string(roleObj);

        for (int j = 0; j < acceptedCount; j++) {
            if (strcmp(role, acceptedRoles[j]) == 0) {
                return 1; // match found
            }
        }
    }

    return 0; // no accepted role found
}

int validate_jwt(const char *token,
                 const char *pubkey_pem,
                 char *out_username,
                 size_t username_len,
                 char *out_fullname,
                 size_t fullname_len)
{
    jwt_t *jwt = NULL;

    // 1) Verify signature using libjwt
    if (jwt_decode(&jwt,
                   token,
                   (const unsigned char *)pubkey_pem,
                   strlen(pubkey_pem)) != 0) {
        writeLog("JWT signature verification failed", 1, 0);
        return 0;
    }

    // 2) Extract payload (middle part)
    const char *dot1 = strchr(token, '.');
    const char *dot2 = dot1 ? strchr(dot1 + 1, '.') : NULL;
    if (!dot1 || !dot2) {
        writeLog("Invalid JWT format", 1, 0);
        jwt_free(jwt);
        return 0;
    }

    size_t payload_len = (size_t)(dot2 - (dot1 + 1));
    char *payload_b64 = strndup(dot1 + 1, payload_len);
    if (!payload_b64) {
        jwt_free(jwt);
        return 0;
    }

    b64url_to_b64(payload_b64);

    unsigned char decoded[4096];
    int decoded_len = b64_decode(payload_b64, decoded, sizeof(decoded) - 1);
    free(payload_b64);

    if (decoded_len <= 0) {
        writeLog("Failed to decode JWT payload", 1, 0);
        jwt_free(jwt);
        return 0;
    }
    decoded[decoded_len] = '\0';

    // 3) Parse JSON payload
    struct json_object *payload = json_tokener_parse((char *)decoded);
    if (!payload) {
        writeLog("Invalid JWT payload JSON", 1, 0);
        jwt_free(jwt);
        return 0;
    }

    time_t now = time(NULL);

    // 4) Validate exp
    struct json_object *exp_obj = NULL;
    if (json_object_object_get_ex(payload, "exp", &exp_obj)) {
        time_t exp = json_object_get_int64(exp_obj);
        if (now > exp) {
            writeLog("JWT expired", 1, 0);
            json_object_put(payload);
            jwt_free(jwt);
            return 0;
        }
    }

    // 5) Validate nbf
    struct json_object *nbf_obj = NULL;
    if (json_object_object_get_ex(payload, "nbf", &nbf_obj)) {
        time_t nbf = json_object_get_int64(nbf_obj);
        if (now < nbf) {
            writeLog("JWT not yet valid (nbf)", 1, 0);
            json_object_put(payload);
            jwt_free(jwt);
            return 0;
        }
    }

    // 6) Validate iat
    struct json_object *iat_obj = NULL;
    if (json_object_object_get_ex(payload, "iat", &iat_obj)) {
        time_t iat = json_object_get_int64(iat_obj);
        if (iat > now + 60) {  // allow small clock skew
            writeLog("JWT issued in the future (iat)", 1, 0);
            json_object_put(payload);
            jwt_free(jwt);
            return 0;
        }
    }

    // 7) Validate issuer (iss)
    if (iam_issuer && strcmp(iam_issuer, "") != 0) {
        struct json_object *iss_obj = NULL;
        if (!json_object_object_get_ex(payload, "iss", &iss_obj) ||
            !json_object_is_type(iss_obj, json_type_string)) {
            writeLog("JWT missing or invalid 'iss'", 1, 0);
            json_object_put(payload);
            jwt_free(jwt);
            return 0;
        }
        const char *iss = json_object_get_string(iss_obj);
        if (strcmp(iss, iam_issuer) != 0) {
            writeLog("JWT issuer mismatch", 1, 0);
            json_object_put(payload);
            jwt_free(jwt);
            return 0;
        }
    }

    /* // 8) Validate audience (aud)
    int aud_ok = 1;
    if (enableIamAud && iam_aud && strcmp(iam_aud, "") != 0 &&
        strcasecmp(iam_aud, "none") != 0) {

        struct json_object *aud_obj = NULL;
        if (!json_object_object_get_ex(payload, "aud", &aud_obj)) {
            writeLog("JWT missing 'aud'", 1, 0);
            aud_ok = 0;
        } else if (json_object_is_type(aud_obj, json_type_string)) {
            const char *aud_str = json_object_get_string(aud_obj);
            aud_ok = (strcmp(aud_str, iam_aud) == 0);
        } else if (json_object_is_type(aud_obj, json_type_array)) {
            aud_ok = 0;
            int len = json_object_array_length(aud_obj);
            for (int i = 0; i < len; i++) {
                struct json_object *elem = json_object_array_get_idx(aud_obj, i);
                if (json_object_is_type(elem, json_type_string)) {
                    const char *val = json_object_get_string(elem);
                    if (strcmp(val, iam_aud) == 0) {
                        aud_ok = 1;
                        break;
                    }
                }
            }
        } else {
            writeLog("JWT 'aud' has unsupported type", 1, 0);
            aud_ok = 0;
        }
    }

    if (!aud_ok) {
        writeLog("JWT audience/azp validation failed", 1, 0);
        json_object_put(payload);
        jwt_free(jwt);
        return 0;
    }*/

    // 8) Validate audience (aud or azp)
    int aud_ok = 1;

    if (enableIamAud && iam_aud && strcmp(iam_aud, "") != 0 && strcasecmp(iam_aud, "none") != 0) {
	struct json_object *aud_obj = NULL;

	// First try: "aud"
	if (json_object_object_get_ex(payload, "aud", &aud_obj)) {
		if (json_object_is_type(aud_obj, json_type_string)) {
			const char *aud_str = json_object_get_string(aud_obj);
			aud_ok = (strcmp(aud_str, iam_aud) == 0);
		} else if (json_object_is_type(aud_obj, json_type_array)) {
			aud_ok = 0;
			int len = json_object_array_length(aud_obj);
			for (int i = 0; i < len; i++) {
				struct json_object *elem = json_object_array_get_idx(aud_obj, i);
				if (json_object_is_type(elem, json_type_string)) {
    					const char *val = json_object_get_string(elem);
    					if (strcmp(val, iam_aud) == 0) {
						aud_ok = 1;
						break;
    					}
				}
			}
		} else {
			writeLog("JWT 'aud' has unsupported type", 1, 0);
			aud_ok = 0;
		}
	} else {
		// No "aud" claim — fallback to "azp"
		struct json_object *azp_obj = NULL;
		if (json_object_object_get_ex(payload, "azp", &azp_obj) && json_object_is_type(azp_obj, json_type_string)) {
			const char *azp = json_object_get_string(azp_obj);
			aud_ok = (strcmp(azp, iam_aud) == 0);
			if (!aud_ok) {
				writeLog("JWT azp mismatch", 1, 0);
			}
		} else {
			writeLog("JWT missing both 'aud' and 'azp'", 1, 0);
			aud_ok = 0;
		}
	}
    }

    if (!aud_ok) {
	writeLog("JWT audience/azp validation failed", 1, 0);
	json_object_put(payload);
	jwt_free(jwt);
	return 0;
    }

    if (enableIamRoles) {
	if (!jwt_check_roles(payload, iam_roles_accepted, iam_roles_count)) {
        	writeLog("IAM role check failed — user lacks required role", 1, 0);
        	json_object_put(payload);
        	jwt_free(jwt);
       		 return 0;
    	}
	writeLog("IAM role check passed — user has required role", 0, 0);
    }


    // 9) Extract username (sub)
    struct json_object *sub_obj = NULL;
    if (json_object_object_get_ex(payload, "sub", &sub_obj) &&
        json_object_is_type(sub_obj, json_type_string)) {

        const char *username = json_object_get_string(sub_obj);

        snprintf(out_username, username_len, "%s", username);

        char logbuf[256];
        snprintf(logbuf, sizeof(logbuf), "JWT validated for user: %s", username);
        writeLog(logbuf, 0, 0);

    } else {
        writeLog("JWT missing 'sub' (username)", 1, 0);
        json_object_put(payload);
        jwt_free(jwt);
        return 0;
    }

    // fullname optional
    if (out_fullname && fullname_len > 0) {
        struct json_object *name_obj = NULL;
        if (json_object_object_get_ex(payload, "name", &name_obj) &&
            json_object_is_type(name_obj, json_type_string)) {
            snprintf(out_fullname, fullname_len,
                     "%s", json_object_get_string(name_obj));
        } else {
            out_fullname[0] = '\0';
        }
    }

    json_object_put(payload);
    jwt_free(jwt);
    return 1;
}
