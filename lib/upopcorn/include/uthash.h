#pragma once
#define uthash_malloc(sz) pmalloc(sz)
#define uthash_free(ptr,sz) pfree(ptr)
#include "uthash/uthash.h"
