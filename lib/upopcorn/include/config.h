#pragma once

#define POPCORN_CONFIG_FILE ".popcorn" /* file should be in HOME directory */
#define POPCORN_NODE_MAX 16
#define PATH_MAX 4096

/* Supported architectures */
enum arch {
  AARCH64 = 0,
  X86_64,
  NUM_ARCHES
};


#define IP_FIELD 16
#define ARCH_FIELD 12
extern char arch_nodes[POPCORN_NODE_MAX][IP_FIELD]; //= {"127.0.0.1", "127.0.0.1"};
extern int arch_type[POPCORN_NODE_MAX]; //= { X86_64, X86_64, AARCH64, AARCH64};
