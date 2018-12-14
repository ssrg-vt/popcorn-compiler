#pragma once

#include "protection.h"
#include "page.h"
//#include "utarray.h"

/**
 * region_t
 * @desc hold all the information about an area in the process's  VM
 */
typedef struct region_s{
	void* addr_start; 	//< start address of the area
	void* addr_end; 	//< end address
	unsigned long length; //< size of the range
	unsigned long referenced; //< size of the range

	char perm[5];		//< permissions rwxp ( string format)
	struct protection_s prot;

	long offset;	//< offset
	char dev[12];	//< dev major:minor
	unsigned long inode;		//< inode of the file that backs the area

	#define REGION_PATHNAME_MAX	512
	char pathname[REGION_PATHNAME_MAX];		//< the path of the file that backs the area

	/* private data */
	struct region_s* next;		//<handler of the chained list
	int nid;			// nid of manager
	int remote;			//has the region been fetched/requested remotely: shared region
	/* use bitmap ? */
	uint32_t region_nb_pages;
	//struct page_s* region_pages;		// page descriptors of this region
	//UT_array *region_pages;
	char *region_pages;
} region_t;

/* allocate a new region_t */
region_t* region_new(int remote);
void region_init_pages(region_t* map, int present);
void region_delete(region_t* map);
void region_print(region_t* map);
//page_t* region_find_page(region_t* map, void* addr);
int region_set_page(region_t* map, void* addr, uint64_t size, int present);
void region_extend_pages(region_t* map, int present);
int region_is_page_present(region_t* map, void* addr, uint64_t size);
