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
#include "pmparser.h"
#include "config.h"
#include <stdlib.h>
#include <stdio.h>

/**
 * gobal variables
 */
procmap_t* pmp_head =NULL;
procmap_t* pmp_curr =NULL;

enum PARSE_MODE
{
	NORMAL=0,
	UPDATE,
	JUST_PRINT
};

static int pmparser_parse(int update);

int pmparser_init()
{
	return pmparser_parse(NORMAL);
}


procmap_t* pmparser_new()
{
	void* ret;
	ret = (procmap_t*)pmalloc(sizeof(procmap_t));
	if(!ret)
		up_log("%s: error!!!\n", __func__);
	return ret;
}

static void pmp_update(procmap_t* dest, procmap_t* src)
{
	dest->addr_start=src->addr_start;
	dest->addr_end=src->addr_end;
	dest->length=src->length;
	strncpy(dest->perm, src->perm, 5);
	dest->prot=src->prot;
	dest->offset=src->offset;
	//dest->dev=src->dev;
	dest->inode=src->inode;
	//strcpydest->pathname=src->pathname;
}

void pmparser_insert(procmap_t* node, int nid)
{
	node->next = pmp_head;
	node->nid = nid;
	pmp_head=node;	
}


#define BUF_SIZE 512
static int pmparser_parse(int mode)
{
	FILE* file;
	size_t linesz=BUF_SIZE;
	char *lineptr;
	int fields;
	struct procmap_s *tmp, *tmp2;

	printf("parsing /proc/self/smaps\n"); 
	file=fopen("/proc/self/smaps","r");
	if(!file){
		fprintf(stderr,"pmparser : cannot open the memory maps, %s\n",strerror(errno));
		return -1;
	}

	if(!(lineptr = (char*)pmalloc(BUF_SIZE * sizeof(char)))) 
			return -1;

	while(getline(&lineptr, &linesz, file) >= 0)
        {
		up_log("line read: %s", lineptr);

		if(mode != JUST_PRINT)
		{
			/* allocate node and fill min fields */
			tmp=(struct procmap_s*)pmalloc(sizeof(struct procmap_s));
			if(!tmp)
				perror(__func__);

			fields = sscanf(lineptr, "%lx-%lx %s %lx %s %lu %s",
			       (unsigned long*)&tmp->addr_start,  (unsigned long*)&tmp->addr_end, 
				tmp->perm, &tmp->offset, tmp->dev, &tmp->inode, tmp->pathname);

			if(fields < 6)
				up_log("maps: less fields (%d) than expected (6 or 7)", fields);

			tmp->pathname[PMPARSER_PATHNAME_MAX-1] = '\0';

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
		if(mode==UPDATE && (pmparser_get(tmp->addr_start, &tmp2, NULL)==0))
		{
			up_log("region exist: updating content\n");
			/*revising the updating of regions since its complexe: local/remote regions, 
			 * extended regions, new/deleted!!! regions, what else? */
			pmp_update(tmp2, tmp);//In case the region has beed extended (like for malloced ones), this a temp fix
			pmparser_print(tmp,0);
			pfree(tmp);//updating requested and the node already exist
			continue;
		}

		//attach the node
		pmparser_insert(tmp, -1);
	}

	pfree(lineptr);
	fclose(file);

	pmp_curr=NULL;

	return 0;
}

procmap_t* pmparser_next()
{
	if(pmp_head==NULL) 
		return NULL;

	if(pmp_curr==NULL)
		pmp_curr=pmp_head;
	else
		pmp_curr=pmp_curr->next;

	return pmp_curr;
}

int pmparser_update()
{
	//should simply ask for more info and create a region and insert it?!
	up_log("updating pmparser...\n");
	return pmparser_parse(UPDATE);
}


int pmparser_parse_print()
{
	//should simply ask for more info and create a region and insert it?!
	up_log("pinting smaps...\n");
	return pmparser_parse(JUST_PRINT);
}


int pmparser_get(void* addr, procmap_t **map, struct page_s **page){
	int found;
	//static procmap_t *cached_map = 
	//int pg_num;
	procmap_t *iter;

	if(map) *map = NULL;
	//if(page) = *page = NULL;

	found = 0;
	pmp_curr = NULL;//rei-init the walk 
	while((iter = pmparser_next()))
	{
		/* TODO: Quicker search: ordered list/ hashtable ?	*
		 * and modify insertion accordingly			*/
		if(addr < iter->addr_end && addr >= iter->addr_start)
		{
			found = 1;
			if(map) *map = iter;
			/*
			assert(iter->page);
			if(page){
				pg_num = addr-map->addr_start/page_size;
				page = &iter->page[pg_num];
			}
			*/
			break;
		}
	}
	if(found)
		return 0;
	else
		return -1;
}

int pmparser_alloc_pages(procmap_t *map)
{
	//map->pages = calloc(map->length/page_size, sizeof(struct protection_s));	
	return -1;
}


void pmparser_free(){
	if(pmp_head==NULL) 
		return;

	procmap_t* act=pmp_head;
	procmap_t* nxt=act->next;

	while(act!=NULL){
		pfree(act);
		act=nxt;
		if(nxt!=NULL)
			nxt=nxt->next;
	}

	pmp_head = NULL;
	pmp_curr = NULL;
}

void pmparser_print(procmap_t* map, int order){

	procmap_t* tmp=map;
	int id=0;
	if(order<0) order=-1;
	while(tmp!=NULL){
		//(unsigned long) tmp->addr_start;
		if(order==id || order==-1){
			up_log("Node address :\t%p\n", tmp);
			up_log("Backed by:\t%s\n",strlen(tmp->pathname)==0?"[anonym*]":tmp->pathname);
			up_log("Range:\t\t%p-%p\n",tmp->addr_start,tmp->addr_end);
			up_log("Length:\t\t%ld\n",tmp->length);
			up_log("Offset:\t\t%ld\n",tmp->offset);
			up_log("Permissions:\t%s\n",tmp->perm);
			up_log("Inode:\t\t%lu\n",tmp->inode);
			up_log("Device:\t\t%s\n",tmp->dev);
		}
		if(order!=-1 && id>order)
			tmp=NULL;
		else if(order==-1){
			up_log("#################################\n");
			tmp=tmp->next;
		}else tmp=tmp->next;

		id++;
	}
}
