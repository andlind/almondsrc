#ifndef JWT_VALIDATE_H
#define JWT_VALIDATE_H

#include <stddef.h>
#include <stdbool.h>

extern char *iam_issuer;
extern char *iam_aud;
extern char **iam_roles_accepted;
extern bool enableIamAud;
extern bool enableIamRoles;

int validate_jwt(const char *token,
                 const char *pubkey_pem,
                 char *out_username,
                 size_t username_len,
                 char *out_fullname,
                 size_t fullname_len);
#endif

