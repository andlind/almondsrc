#ifndef ALMOND_CONFIG_STRUCTURES_HEADER
#define ALMOND_CONFIG_STRUCTURES_HEADER

typedef struct {
	int intval;
	char* strval;
} ConfValUnion;

typedef struct ConfigEntry {
	const char* name;
	void (*process)(ConfValUnion value);
} ConfigEntry;

#endif

