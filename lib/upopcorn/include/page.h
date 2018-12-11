#pragma once

#include "protection.h"
#include "uthash.h"
#include <malloc.h>

typedef struct page_s
{
	uintptr_t page_start;
	uintptr_t page_size;
	struct protection_s page_prot;
	UT_hash_handle hh;
}page_t;

static inline page_t* page_new()
{
	page_t* page = pmalloc(sizeof(*page));
	return page;
}
