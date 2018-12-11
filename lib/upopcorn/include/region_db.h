/*
 Mohamed Karaoui
 Modified from: ouadimjamal@gmail.com
 @date		: December 2015

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.  No representations are made about the suitability of this
software for any purpose.  It is provided "as is" without express or
implied warranty.

 */

#ifndef H_PMPARSER
#define H_PMPARSER
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "region.h"
#include "page.h"



/* initialize the DB from */
int region_db_init();

/* update: refresh database */
int region_db_update();


/* add a region_t to the DB: used to insert remote regions */
void region_db_insert(region_t* tmp, int nid);

/**
 * region_db_next
 * @description move between areas
 */
region_t* region_db_next();

/**
 * region_db_destroy
 * @description should be called at the end to free the resources
 * @param maps_list the head of the list to be freed
 */
void region_db_destroy();


/* return the map that lies within the addr argument */
int region_db_get(void* addr, region_t **map, struct page_s **page);

int region_db_parse_print();



/****** configurations parameter (private) ******/

#define LINE_MAX_SIZE 512
#define PROC_MAPS_FILE "/proc/self/smaps"


#endif
