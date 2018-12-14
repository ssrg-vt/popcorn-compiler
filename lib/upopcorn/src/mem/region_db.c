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

//#include "config.h"
#include "region_db.h"
#include "region.h"
#include "config.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

/**
 * gobal variables
 */
struct regions_db_s
{
	region_t* head;
	region_t* curr;
};

static struct regions_db_s region_db = {NULL, NULL};

enum PARSE_MODE
{
	NORMAL=0,
	UPDATE,
	JUST_PRINT
};

static int region_db_parse(int update);

static int __region_db_init=0;
int region_db_init()
{
	int ret;
	if(__region_db_init)
		return 0;
	__region_db_init=1;
	ret=region_db_parse(NORMAL);
	if(ret)
		printf("[map]: cannot parse the memory map of %d\n", getpid());//TODO:exit?
	return ret;
}



static void pmp_update(region_t* dest, region_t* src)
{
	region_print(src);
	region_print(dest);
	assert(dest->addr_start==src->addr_start);
	dest->addr_start=src->addr_start;
	dest->addr_end=src->addr_end;
	dest->length=src->length;
	strncpy(dest->perm, src->perm, 5);
	dest->prot=src->prot;
	dest->offset=src->offset;
	//dest->dev=src->dev;
	dest->inode=src->inode;
	//strcpydest->pathname=src->pathname;
	
	if(dest->addr_end!=src->addr_end)
		region_extend_pages(dest, 1);
}

void region_db_insert(region_t* node, int nid)
{
	node->next = region_db.head;
	node->nid = nid;
	region_db.head=node;	
}


static int region_db_parse(int mode)
{
	FILE* file;
	size_t linesz=LINE_MAX_SIZE;
	char *lineptr;
	int fields;
	struct region_s *tmp, *tmp2;

	up_log("parsing %s\n", PROC_MAPS_FILE); 
	file=fopen(PROC_MAPS_FILE , "r");
	if(!file){
		fprintf(stderr,"region_db : cannot open the memory maps, %s\n",strerror(errno));
		return -1;
	}

	if(!(lineptr = (char*)pmalloc(LINE_MAX_SIZE * sizeof(char)))) 
			return -1;

	while(getline(&lineptr, &linesz, file) >= 0)
        {
		up_log("line read: %s", lineptr);

		if(mode != JUST_PRINT)
		{
			/* allocate node and fill min fields */
			tmp=(struct region_s*)region_new(0);//pmalloc(sizeof(struct region_s));
			if(!tmp)
				perror(__func__);

			fields = sscanf(lineptr, "%lx-%lx %s %lx %s %lu %s",
			       (unsigned long*)&tmp->addr_start,  (unsigned long*)&tmp->addr_end, 
				tmp->perm, &tmp->offset, tmp->dev, &tmp->inode, tmp->pathname);

			if(fields < 6)
				up_log("maps: less fields (%d) than expected (6 or 7)", fields);


			tmp->pathname[REGION_PATHNAME_MAX-1] = '\0';

			tmp->length = (unsigned long)tmp->addr_end - 
					(unsigned long)tmp->addr_start;
			tmp->prot.is_r=(tmp->perm[0]=='r');
			tmp->prot.is_w=(tmp->perm[1]=='w');
			tmp->prot.is_x=(tmp->perm[2]=='x');
			tmp->prot.is_p=(tmp->perm[3]=='p');
			tmp->next=NULL;
		}

		//read reference field and skip other fields
		for(;;)
		{
			char substr[32];
        		int n;
			if(getline(&lineptr, &linesz, file)<0)
				perror("reading # referenced");

			//printf("line read: %s", lineptr);
			if (sscanf(lineptr, "%31[^:]%n", substr, &n) == 1)
			{
				//printf("subtr read: %32s\n", lineptr);
				if (strcmp(substr, "Referenced") == 0)     
				{
					if(mode != JUST_PRINT)
					{
						tmp->referenced = n; 
						//printf("referenced %ld\n", tmp->referenced);
					}
				}

				//VmFlags is the end marker (should be the last line)
				if (strcmp(substr, "VmFlags") == 0)     
				{
					//printf("VmFlags found\n");
					break;
				}
			}
		}
		
		if(mode == JUST_PRINT)
			continue;

#if 0
		if(tmp)
			up_log("%p = %lx-%lx %s %lx %s %lu %s;\n", tmp,
			       (unsigned long)tmp->addr_start,  (unsigned long)tmp->addr_end, 
				tmp->perm, tmp->offset, tmp->dev, tmp->inode, tmp->referenced, tmp->pathname);
#endif
		if(mode==UPDATE && (region_db_get(tmp->addr_start, &tmp2)==0))
		{
			/*TODO add/remove pages if the region becomes bigger/smaller */
			up_log("region exist: updating content\n");
			/*revising the updating of regions since its complexe: local/remote regions, 
			 * extended regions, new/deleted!!! regions, what else? */
			pmp_update(tmp2, tmp);//In case the region has beed extended (like for malloced ones), this a temp fix
			region_print(tmp);
			pfree(tmp);//updating requested and the node already exist
			continue;
		}

		//attach the node
		region_db_insert(tmp, -1);
	}

	pfree(lineptr);
	fclose(file);

	region_db.curr=NULL;

	return 0;
}

region_t* region_db_next()
{
	if(region_db.head==NULL) 
		return NULL;

	if(region_db.curr==NULL)
		region_db.curr=region_db.head;
	else
		region_db.curr=region_db.curr->next;

	return region_db.curr;
}

int region_db_update()
{
	//should simply ask for more info and create a region and insert it?!
	up_log("updating region_db...\n");
	return region_db_parse(UPDATE);
}


int region_db_parse_print()
{
	//should simply ask for more info and create a region and insert it?!
	up_log("pinting smaps...\n");
	return region_db_parse(JUST_PRINT);
}

static inline int addr_is_in_region(region_t *map, void* addr)
{
	if(addr < map->addr_end && addr >= map->addr_start)
		return 1;
	else
		return 0;
}


int region_db_get(void* addr, region_t **map){
	int found;
	/* FIXME: region can be freed!!! */
	static region_t *cached_map = NULL;
	region_t *iter;

	if(!map) 
		return -1;

	*map = NULL;

	if(cached_map && addr_is_in_region(cached_map, addr))
	{
		*map = cached_map;
		goto region_found;
	}
		

	region_db.curr = NULL;//rei-init the walk 
	while((iter = region_db_next()))
	{
		/* TODO: Quicker search: ordered list/ hashtable ?	*
		 * and modify insertion accordingly			*/
		if(addr_is_in_region(iter, addr))
		{
			assert(map);
			*map = iter;
			cached_map = iter;
			goto region_found;
		}
	}

	return -1;

region_found:
	return 0;
}

int region_db_alloc_pages(region_t *map)
{
	//map->pages = calloc(map->length/page_size, sizeof(struct protection_s));	
	return -1;
}


void region_db_destroy(){
	if(region_db.head==NULL) 
		return;

	region_t* act=region_db.head;
	region_t* nxt=act->next;

	while(act!=NULL){
		pfree(act);
		act=nxt;
		if(nxt!=NULL)
			nxt=nxt->next;
	}

	region_db.head = NULL;
	region_db.curr = NULL;
}

