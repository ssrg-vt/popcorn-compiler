#pragma once

static char* arch_nodes[] = {"127.0.0.1", "127.0.0.1"};

/* Supported architectures */
enum arch {
  AARCH64 = 0,
  X86_64,
  NUM_ARCHES
};

static int arch_type[] = { X86_64, X86_64, AARCH64, AARCH64};
