/*
 @Author	: ouadimjamal@gmail.com
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


extern int page_size;

//TODO: use it in struct procmap_s
struct protection_s{
	char is_r:1;			//< rewrote of perm with short flags
	char is_w:1;
	char is_x:1;
	char is_p:1;
};

typedef struct page_s
{
	struct protection_s prot;//bit is_p not used
}page_t;

/**
 * procmap_t
 * @desc hold all the information about an area in the process's  VM
 */
typedef struct procmap_s{
	void* addr_start; 	//< start address of the area
	void* addr_end; 	//< end address
	unsigned long length; //< size of the range

	char perm[5];		//< permissions rwxp ( string format)
	struct protection_s prot;

	long offset;	//< offset
	char dev[12];	//< dev major:minor
	int inode;		//< inode of the file that backs the area

	char pathname[600];		//< the path of the file that backs the area
	//chained list
	struct procmap_s* next;		//<handler of the chinaed list
	
	struct page_s* pages;		// page descriptors of this region
} procmap_t;

/**
 * pmparser_parse
 * @param pid the process id whose memory map to be parser. the current process if pid<0
 * @return list of procmap_t structers
 */
int pmparser_parse(int pid);
void pmparser_init();//TODO: Merge with previous

/**
 * pmparser_next
 * @description move between areas
 */
procmap_t* pmparser_next();
/**
 * pmparser_free
 * @description should be called at the end to free the resources
 * @param maps_list the head of the list to be freed
 */
void pmparser_free();


/* return the map that lies within the addr argument */
int pmparser_get(void* addr, procmap_t **map, struct page_s **page);

int pmparser_alloc_pages(procmap_t *map);

/**
 * pmparser_print
 * @param map the head of the list
 * @order the order of the area to print, -1 to print everything
 */
void pmparser_print(procmap_t* map,int order);





#endif
