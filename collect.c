#define _GNU_SOURCE
#include <json-c/json.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#define DEVICE_LENGTH 64 
#define PATH_LENGTH 200
// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static char *ltrim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

void detect_memory_type(char *out, size_t out_size) {
    strncpy(out, "unknown", out_size);

    // --- 1. Try sysfs SMBIOS (bare metal + some VMs) ---
    FILE *fp = fopen("/sys/firmware/dmi/tables/DMI", "rb");
    if (fp) {
        char buf[4096];
        size_t n = fread(buf, 1, sizeof(buf), fp);
        fclose(fp);

        if (n > 0) {
            if (memmem(buf, n, "DDR5", 4)) { strncpy(out, "DDR5", out_size); return; }
            if (memmem(buf, n, "DDR4", 4)) { strncpy(out, "DDR4", out_size); return; }
            if (memmem(buf, n, "DDR3", 4)) { strncpy(out, "DDR3", out_size); return; }
            if (memmem(buf, n, "LPDDR5", 6)) { strncpy(out, "LPDDR5", out_size); return; }
            if (memmem(buf, n, "LPDDR4", 6)) { strncpy(out, "LPDDR4", out_size); return; }
        }
    }

    // --- 2. Try dmidecode (bare metal + most VMs, not containers) ---
    fp = popen("dmidecode -t memory 2>/dev/null | grep -m1 'Type:' | grep -v Unknown", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp)) {
            char *p = strstr(line, "Type:");
            if (p) {
                p += 5;
                while (*p == ' ' || *p == '\t') p++;
                strncpy(out, p, out_size);
                out[strcspn(out, "\n")] = 0;
                pclose(fp);
                return;
            }
        }
        pclose(fp);
    }

    // --- 3. Fallback: /proc/meminfo (works everywhere, including containers) ---
    fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
            	char *p = line + 9;      // skip "MemTotal:"
            	p = ltrim(p);            // remove leading spaces
            	/*snprintf(out, out_size, "physical (%s)", p);
            	out[strcspn(out, "\n")] = 0;*/
		p[strcspn(p, "\n")] = 0;      // remove newline BEFORE formatting
		snprintf(out, out_size, "physical (%s)", p);
            	fclose(fp);
            	return;
            }
        }
        fclose(fp);
    }

    // --- 4. Final fallback ---
    strncpy(out, "unknown", out_size);
}

static int read_first_line(const char *path, char *out, size_t max) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    if (!fgets(out, max, fp)) {
        fclose(fp);
        return 0;
    }
    out[strcspn(out, "\n")] = 0;
    fclose(fp);
    return 1;
}

static void add_if_known(struct json_object *obj,
                         const char *key,
                         const char *value)
{
    if (value && strcmp(value, "unknown") != 0 && strlen(value) > 0)
        json_object_object_add(obj, key, json_object_new_string(value));
}

static void add_if_positive(struct json_object *obj,
                            const char *key,
                            int value)
{
    if (value > 0)
        json_object_object_add(obj, key, json_object_new_int(value));
}

static int object_is_empty(struct json_object *obj) {
    return json_object_object_length(obj) == 0;
}

struct json_object* create_labels_json() {
        struct json_object *root = json_object_new_object();

        // Array under "labels"
        struct json_object *labels_array = json_object_new_array();

        // Object inside the array
        struct json_object *label_obj = json_object_new_object();
        json_object_object_add(label_obj, "environment", json_object_new_string("test"));
        json_object_object_add(label_obj, "product", json_object_new_string("microservice"));

        // Add object to array
        json_object_array_add(labels_array, label_obj);

        // return label onlys
        //return labels_array;

        // Add array to root
        json_object_object_add(root, "labels", labels_array);

        return root;
}

struct json_object* read_labels(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return NULL;

    struct json_object *array = json_object_new_array();
    char buffer[2048];
    int id = 0;

    while (fgets(buffer, sizeof(buffer), fp)) {

        // Detect truncated line (too long for buffer)
        bool truncated = (strchr(buffer, '\n') == NULL);

        // Trim leading whitespace
        char *line = buffer;
        while (*line == ' ' || *line == '\t' || *line == '\n')
            line++;

        // Skip comments and empty lines
        if (*line == '#' || *line == '\0')
            continue;

        // Create wrapper object
        struct json_object *entry = json_object_new_object();
        json_object_object_add(entry, "id", json_object_new_int(id++));

        //
        // Extract service name [name]
        //
        char *lb = strchr(line, '[');
        char *rb = strchr(line, ']');

        if (lb && rb && rb > lb + 1) {
            char service[256];
            size_t len = rb - lb - 1;
            if (len >= sizeof(service)) len = sizeof(service) - 1;

            strncpy(service, lb + 1, len);
            service[len] = '\0';

            json_object_object_add(entry, "service",
                json_object_new_string(service));
        } else {
            json_object_object_add(entry, "service",
                json_object_new_string("unknown"));
        }

        //
        // If line was too long → mark as unparsable
        //
        if (truncated) {
            json_object_object_add(entry, "labels",
                json_object_new_string("unparsable"));
            json_object_array_add(array, entry);
            continue;
        }

        //
        // Extract JSON labels if present
        //
        char *json_start = strchr(line, '{');

        if (json_start) {
            struct json_object *parsed = json_tokener_parse(json_start);
            if (parsed) {
                json_object_object_add(entry, "labels", parsed);
                json_object_array_add(array, entry);
                continue;
            }
        }

        // No JSON found → labels: "none"
        json_object_object_add(entry, "labels",
            json_object_new_string("none"));

        json_object_array_add(array, entry);
    }

    fclose(fp);
    return array;
}


// ------------------------------------------------------------
// Virtualization detection
// ------------------------------------------------------------

static const char* detect_virtualization() {
    static char product[256];

    if (read_first_line("/sys/class/dmi/id/product_name", product, sizeof(product))) {
        if (strstr(product, "KVM")) return "KVM";
        if (strstr(product, "VirtualBox")) return "VirtualBox";
        if (strstr(product, "VMware")) return "VMware";
        if (strstr(product, "Microsoft")) return "Hyper-V";
        if (strstr(product, "Apple")) return "Apple Virtualization";
        if (strstr(product, "QEMU")) return "QEMU";
    }

    return "BareMetal/Unknown";
}

// ------------------------------------------------------------
// Main system info collector
// ------------------------------------------------------------

struct json_object* get_system_info(bool verbose) {
    struct json_object *root = json_object_new_object();

    // ---------------- OS INFO ----------------
    struct utsname uts;
    uname(&uts);

    struct json_object *os = json_object_new_object();
    add_if_known(os, "sysname", uts.sysname);
    add_if_known(os, "release", uts.release);
    add_if_known(os, "version", uts.version);
    add_if_known(os, "machine", uts.machine);

    if (!object_is_empty(os))
        json_object_object_add(root, "os", os);
    else
        json_object_put(os);

    // ---------------- CPU INFO ----------------
    FILE *fp = fopen("/proc/cpuinfo", "r");
    char line[512];
    char model[256] = "unknown";
    char vendor[128] = "unknown";
    int cores = 0;

    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "model name", 10) == 0) {
                char *colon = strchr(line, ':');
                if (colon) {
                    strncpy(model, colon + 2, sizeof(model)-1);
                    model[strcspn(model, "\n")] = 0;
                }
            }
            if (strncmp(line, "vendor_id", 9) == 0) {
                char *colon = strchr(line, ':');
                if (colon) {
                    strncpy(vendor, colon + 2, sizeof(vendor)-1);
                    vendor[strcspn(vendor, "\n")] = 0;
                }
            }
            if (strncmp(line, "processor", 9) == 0) {
                cores++;
            }
        }
        fclose(fp);
    }

    struct json_object *cpu = json_object_new_object();
    add_if_known(cpu, "model", model);
    add_if_known(cpu, "vendor", vendor);
    add_if_positive(cpu, "cores", cores);

    if ((!object_is_empty(cpu)) && verbose)
        json_object_object_add(root, "cpu", cpu);
    else
        json_object_put(cpu);

    // ---------------- MEMORY TYPE ----------------
    /*char memtype[128] = "unknown";
    read_first_line("/sys/devices/system/memory/memory0/uevent", memtype, sizeof(memtype));

    struct json_object *memory = json_object_new_object();
    add_if_known(memory, "type", memtype);

    if ((!object_is_empty(memory)) && verbose)
        json_object_object_add(root, "memory", memory);
    else
        json_object_put(memory);*/
    // ---------------- MEMORY TYPE ----------------
    char memtype[128];
    detect_memory_type(memtype, sizeof(memtype));

    struct json_object *memory = json_object_new_object();
    add_if_known(memory, "type", memtype);

    if ((!object_is_empty(memory)) && verbose)
        json_object_object_add(root, "memory", memory);
    else
        json_object_put(memory);


    // ---------------- DISKS ----------------
    struct json_object *disks = json_object_new_array();
    DIR *dir = opendir("/sys/block");

    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir))) {
            //if ((entry->d_name[0] == '.') || (strncmp(entry->d_name, "loop", 4) == 0)) continue;
            if (entry->d_name[0] == '.') continue;
            if (strncmp(entry->d_name, "loop", 4) == 0) continue;
            if (strncmp(entry->d_name, "ram", 3) == 0) continue;
            if (strncmp(entry->d_name, "vd", 2) == 0) continue;   // optional
            if (strcmp(model, "unknown") == 0) continue;          // optional

            char model_path[256];
            char rot_path[256];
            char model[256] = "unknown";
            char rot[8] = "1";

            snprintf(model_path, sizeof(model_path),
                     "/sys/block/%.*s/device/model", DEVICE_LENGTH, entry->d_name);
            snprintf(rot_path, sizeof(rot_path),
                     "/sys/block/%.*s/queue/rotational", DEVICE_LENGTH, entry->d_name);
            read_first_line(model_path, model, sizeof(model));
            read_first_line(rot_path, rot, sizeof(rot));

            const char *type = strcmp(rot, "0") == 0 ? "SSD/NVMe" : "HDD";

            struct json_object *disk = json_object_new_object();
            add_if_known(disk, "name", entry->d_name);
            add_if_known(disk, "model", model);
            add_if_known(disk, "type", type);

            if (!object_is_empty(disk))
                json_object_array_add(disks, disk);
            else
                json_object_put(disk);
        }
        closedir(dir);
    }

    if ((json_object_array_length(disks) > 0) && verbose)
        json_object_object_add(root, "disks", disks);
    else
        json_object_put(disks);

    // ---------------- BIOS ----------------
    char bios_vendor[128] = "unknown";
    char bios_version[128] = "unknown";

    read_first_line("/sys/class/dmi/id/bios_vendor", bios_vendor, sizeof(bios_vendor));
    read_first_line("/sys/class/dmi/id/bios_version", bios_version, sizeof(bios_version));

    struct json_object *bios = json_object_new_object();
    add_if_known(bios, "vendor", bios_vendor);
    add_if_known(bios, "version", bios_version);

    if ((!object_is_empty(bios)) && verbose)
        json_object_object_add(root, "bios", bios);
    else
        json_object_put(bios);

    // ---------------- BOARD ----------------
    char board_vendor[128] = "unknown";
    char board_name[128] = "unknown";

    read_first_line("/sys/class/dmi/id/board_vendor", board_vendor, sizeof(board_vendor));
    read_first_line("/sys/class/dmi/id/board_name", board_name, sizeof(board_name));

    struct json_object *board = json_object_new_object();
    add_if_known(board, "vendor", board_vendor);
    add_if_known(board, "name", board_name);

    if ((!object_is_empty(board)) && verbose)
        json_object_object_add(root, "board", board);
    else
        json_object_put(board);

    // ---------------- CHASSIS ----------------
    char chassis_type[DEVICE_LENGTH] = "unknown";
    read_first_line("/sys/class/dmi/id/chassis_type", chassis_type, sizeof(chassis_type));

    struct json_object *chassis = json_object_new_object();
    add_if_known(chassis, "type", chassis_type);

    if ((!object_is_empty(chassis)) && verbose)
        json_object_object_add(root, "chassis", chassis);
    else
        json_object_put(chassis);

    // ---------------- VIRTUALIZATION ----------------
    const char *virt = detect_virtualization();
    if ((strcmp(virt, "BareMetal/Unknown") != 0) && verbose)
        json_object_object_add(root, "virtualization", json_object_new_string(virt));

    // ---------------- GPU ----------------
    DIR *drm = opendir("/sys/class/drm");
    if (drm) {
        struct dirent *entry;
        while ((entry = readdir(drm))) {
            if (strncmp(entry->d_name, "card", 4) != 0) continue;

            char vendor_path[256];
            char device_path[256];
            char driver_link[256];
            char vendor[32] = "unknown";
            char device[32] = "unknown";
            char driver[128] = "unknown";

            snprintf(vendor_path, sizeof(vendor_path),
                 "/sys/class/drm/%.*s/device/vendor", DEVICE_LENGTH, entry->d_name);
            snprintf(device_path, sizeof(device_path),
                 "/sys/class/drm/%.*s/device/device", DEVICE_LENGTH, entry->d_name);

            read_first_line(vendor_path, vendor, sizeof(vendor));
            read_first_line(device_path, device, sizeof(device));

            // driver is a symlink: /sys/class/drm/cardX/device/driver -> .../DRIVER
            snprintf(driver_link, sizeof(driver_link),
                 "/sys/class/drm/%.*s/device/driver", DEVICE_LENGTH, entry->d_name);
            ssize_t len = readlink(driver_link, driver, sizeof(driver) - 1);
            if (len > 0) {
                driver[len] = '\0';
                // keep only basename
                char *slash = strrchr(driver, '/');
                if (slash) memmove(driver, slash + 1, strlen(slash));
            } else {
                strcpy(driver, "unknown");
            }

            struct json_object *gpu = json_object_new_object();
            add_if_known(gpu, "vendor", vendor);
            add_if_known(gpu, "device", device);
            add_if_known(gpu, "driver", driver);

            if ((!object_is_empty(gpu)) && verbose)
                json_object_object_add(root, "gpu", gpu);
            else
                json_object_put(gpu);

            break; // only first GPU
        }
        closedir(drm);
    }

    // ---------------- PCI DEVICES ----------------
    struct json_object *pci_array = json_object_new_array();
    DIR *pcidir = opendir("/sys/bus/pci/devices");

    if (pcidir) {
        struct dirent *entry;
        while ((entry = readdir(pcidir))) {
            if (entry->d_name[0] == '.') continue;

            char base[256];
            snprintf(base, sizeof(base), "/sys/bus/pci/devices/%.*s", DEVICE_LENGTH, entry->d_name);

            char vendor_path[256];
            char device_path[256];
            char class_path[256];
            char vendor[32] = "unknown";
            char device[32] = "unknown";
            char classcode[32] = "unknown";

            snprintf(vendor_path, sizeof(vendor_path), "%.*s/vendor",PATH_LENGTH, base);
            snprintf(device_path, sizeof(device_path), "%.*s/device", PATH_LENGTH, base);
            snprintf(class_path, sizeof(class_path), "%.*s/class", PATH_LENGTH, base);

            read_first_line(vendor_path, vendor, sizeof(vendor));
            read_first_line(device_path, device, sizeof(device));
            read_first_line(class_path, classcode, sizeof(classcode));

            struct json_object *pci = json_object_new_object();
            add_if_known(pci, "address", entry->d_name);
            add_if_known(pci, "vendor", vendor);
            add_if_known(pci, "device", device);
            add_if_known(pci, "class", classcode);

            if (!object_is_empty(pci))
                json_object_array_add(pci_array, pci);
            else
                json_object_put(pci);
        }
        closedir(pcidir);
    }

    if ((json_object_array_length(pci_array) > 0) && verbose)
        json_object_object_add(root, "pci", pci_array);
    else
        json_object_put(pci_array);

    // ---------------- USB CONTROLLERS ----------------
    struct json_object *usb_array = json_object_new_array();
    DIR *usbdir = opendir("/sys/bus/usb/devices");

    if (usbdir) {
        struct dirent *entry;
        while ((entry = readdir(usbdir))) {
            if (entry->d_name[0] == '.') continue;

            char base[256];
            snprintf(base, sizeof(base), "/sys/bus/usb/devices/%.*s", DEVICE_LENGTH, entry->d_name);

            char vendor_path[256];
            char product_path[256];
            char class_path[256];
            char vendor[32] = "unknown";
            char product[32] = "unknown";
            char classcode[32] = "unknown";

            snprintf(vendor_path, sizeof(vendor_path), "%.*s/idVendor", PATH_LENGTH, base);
            snprintf(product_path, sizeof(product_path), "%.*s/idProduct", PATH_LENGTH, base);
            snprintf(class_path, sizeof(class_path), "%.*s/bDeviceClass", PATH_LENGTH, base);

            if (!read_first_line(class_path, classcode, sizeof(classcode)))
                continue;

            read_first_line(vendor_path, vendor, sizeof(vendor));
            read_first_line(product_path, product, sizeof(product));

            struct json_object *usb = json_object_new_object();
            add_if_known(usb, "path", entry->d_name);
            add_if_known(usb, "vendor", vendor);
            add_if_known(usb, "product", product);
            add_if_known(usb, "class", classcode);

            if (!object_is_empty(usb))
                json_object_array_add(usb_array, usb);
            else
                json_object_put(usb);
        }
        closedir(usbdir);
    }

    if ((json_object_array_length(usb_array) > 0) && verbose)
        json_object_object_add(root, "usb", usb_array);
    else
        json_object_put(usb_array);

    // ---------------- NETWORK ----------------
    struct json_object *net_array = json_object_new_array();
    DIR *netdir = opendir("/sys/class/net");

    if (netdir) {
        struct dirent *entry;
        while ((entry = readdir(netdir))) {
            if (entry->d_name[0] == '.') continue;
            if (strcmp(entry->d_name, "lo") == 0) continue;

            char base[256];
            snprintf(base, sizeof(base), "/sys/class/net/%.*s", DEVICE_LENGTH, entry->d_name);

            char vendor_path[256];
            char device_path[256];
            char uevent_path[256];
            char vendor[32] = "unknown";
            char device[32] = "unknown";
            char iftype[DEVICE_LENGTH] = "unknown";

            snprintf(vendor_path, sizeof(vendor_path), "%.*s/device/vendor", PATH_LENGTH, base);
            snprintf(device_path, sizeof(device_path), "%.*s/device/device", PATH_LENGTH, base);
            snprintf(uevent_path, sizeof(uevent_path), "%.*s/uevent", PATH_LENGTH, base);

            read_first_line(vendor_path, vendor, sizeof(vendor));
            read_first_line(device_path, device, sizeof(device));

            FILE *uf = fopen(uevent_path, "r");
            if (uf) {
                char line[256];
                while (fgets(line, sizeof(line), uf)) {
                    if (strncmp(line, "DEVTYPE=", 8) == 0) {
                        char *val = line + 8;
                        val[strcspn(val, "\n")] = 0;
                        //strncpy(iftype, val, sizeof(iftype) - 1);
                        snprintf(iftype, sizeof(iftype), "%.*s", (int)sizeof(iftype) - 1, val);
                        break;
                    }
                }
                fclose(uf);
            }

            struct json_object *iface = json_object_new_object();
            add_if_known(iface, "name", entry->d_name);
            add_if_known(iface, "vendor", vendor);
            add_if_known(iface, "device", device);
            add_if_known(iface, "type", iftype);

           if (!object_is_empty(iface))
                json_object_array_add(net_array, iface);
           else
                json_object_put(iface);
        }
        closedir(netdir);
    }

    if ((json_object_array_length(net_array) > 0) && verbose)
        json_object_object_add(root, "network", net_array);
    else
        json_object_put(net_array);

    return root;
}

static double round2(double x) {
    return floor(x * 100.0 + 0.5) / 100.0;
}

struct json_object* parse_perfdata(const char *plugin_output)
{
    const char *sep = strchr(plugin_output, '|');
    if (!sep) return NULL;

    sep++; // move past '|'

    char *data = strdup(sep);

    // Replace commas with spaces
    for (char *p = data; *p; p++) {
        if (*p == ',') *p = ' ';
    }

    struct json_object *metrics = json_object_new_object();

    char *token = strtok(data, " ");
    while (token) {

        char *eq = strchr(token, '=');
        if (!eq) {
            token = strtok(NULL, " ");
            continue;
        }

        *eq = '\0';
        const char *name = token;
        char *value = eq + 1;

        // Split thresholds: value;warn;crit;min;max
        char *warn = NULL;
        char *crit = NULL;
        char *min = NULL;
        char *max = NULL;

        char *p = strchr(value, ';');
        if (p) {
            *p = '\0';
            warn = p + 1;

            p = strchr(warn, ';');
            if (p) {
                *p = '\0';
                crit = p + 1;

                p = strchr(crit, ';');
                if (p) {
                    *p = '\0';
                    min = p + 1;

                    p = strchr(min, ';');
                    if (p) {
                        *p = '\0';
                        max = p + 1;
                    }
                }
            }
        }

        // Remove trailing %
        char *unit = NULL;
        size_t len = strlen(value);
        if (len > 0 && value[len - 1] == '%') {
            value[len - 1] = '\0';
            unit = "percent";
        }

        // Build metric object
        struct json_object *obj = json_object_new_object();

        char buf[32];
	snprintf(buf, sizeof(buf), "%.2f", round2(atof(value)));
	json_object_object_add(obj, "value", json_object_new_string(buf));
        if (unit)
            json_object_object_add(obj, "unit",
                json_object_new_string(unit));

        if (warn && strlen(warn) > 0) {
		snprintf(buf, sizeof(buf), "%.2f", round2(atof(warn)));
   	 	json_object_object_add(obj, "warn", json_object_new_string(buf));
        }

        if (crit && strlen(crit) > 0) {
		snprintf(buf, sizeof(buf), "%.2f", round2(atof(crit)));
                json_object_object_add(obj, "crit", json_object_new_string(buf));

	}

        if (min && strlen(min) > 0) {
		snprintf(buf, sizeof(buf), "%.2f", round2(atof(min)));
                json_object_object_add(obj, "min", json_object_new_string(buf));
	}

        if (max && strlen(max) > 0) {
		snprintf(buf, sizeof(buf), "%.2f", round2(atof(max)));
                json_object_object_add(obj, "max", json_object_new_string(buf));
	}

        json_object_object_add(metrics, name, obj);

        token = strtok(NULL, " ");
    }

    free(data);
    return metrics;
}

