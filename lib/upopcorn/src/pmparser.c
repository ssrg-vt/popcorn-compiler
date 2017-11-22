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
#include <stdlib.h>
#include <stdio.h>

/**
 * gobal variables
 */
procmap_t* pmp_head =NULL;
procmap_t* pmp_curr =NULL;

static int pmparser_parse();

int pmparser_init()
{
	return pmparser_parse(0);
}


procmap_t* pmparser_new()
{
	void* ret;
	ret = (procmap_t*)pmalloc(sizeof(procmap_t));
	if(!ret)
		printf("%s: error!!!\n", __func__);
	return ret;
}

void pmparser_insert(procmap_t* node)
{
	node->next = pmp_head;
	pmp_head=node;	
}


#define BUF_SIZE 512
static int pmparser_parse(int update)
{
	FILE* file;
	size_t linesz=BUF_SIZE;
	char *lineptr;
	int fields;
	struct procmap_s* tmp;

	file=fopen("/proc/self/maps","r");
	if(!file){
		fprintf(stderr,"pmparser : cannot open the memory maps, %s\n",strerror(errno));
		return -1;
	}

	if(!(lineptr = (char*)pmalloc(BUF_SIZE * sizeof(char)))) 
			return -1;

	while(getline(&lineptr, &linesz, file) >= 0)
        {
		//printf("line read: %s", lineptr);
		tmp=(struct procmap_s*)pmalloc(sizeof(struct procmap_s));
		if(!tmp)
			perror(__func__);

                fields = sscanf(lineptr, "%lx-%lx %s %lx %s %lu %s",
                       (unsigned long*)&tmp->addr_start,  (unsigned long*)&tmp->addr_end, 
			tmp->perm, &tmp->offset, tmp->dev, &tmp->inode, tmp->pathname);

                if(fields < 6)
			printf("maps: less fields (%d) than expected (6 or 7)", fields);

		tmp->pathname[PMPARSER_PATHNAME_MAX-1] = '\0';

		tmp->length = (unsigned long)tmp->addr_end - 
				(unsigned long)tmp->addr_start;
		tmp->prot.is_r=(tmp->perm[0]=='r');
		tmp->prot.is_w=(tmp->perm[1]=='w');
		tmp->prot.is_x=(tmp->perm[2]=='x');
		tmp->prot.is_p=(tmp->perm[3]=='p');
		tmp->next=NULL;

#if 1
                printf("%p = %lx-%lx %s %lx %s %lu %s;\n", tmp,
                       (unsigned long)tmp->addr_start,  (unsigned long)tmp->addr_end, 
			tmp->perm, tmp->offset, tmp->dev, tmp->inode, tmp->pathname);
#endif
		//attach the node
		if(update && (pmparser_get(tmp->addr_start, NULL, NULL)==0))
		{
			pfree(tmp);//updating requested and the node already exist
			continue;
		}

		pmparser_insert(tmp);
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
	printf("updating pmparser...\n");
	return pmparser_parse(1);
}


int pmparser_get(void* addr, procmap_t **map, struct page_s **page){
	int found;
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
			printf("Node address :\t%p\n", tmp);
			printf("Backed by:\t%s\n",strlen(tmp->pathname)==0?"[anonym*]":tmp->pathname);
			printf("Range:\t\t%p-%p\n",tmp->addr_start,tmp->addr_end);
			printf("Length:\t\t%ld\n",tmp->length);
			printf("Offset:\t\t%ld\n",tmp->offset);
			printf("Permissions:\t%s\n",tmp->perm);
			printf("Inode:\t\t%lu\n",tmp->inode);
			printf("Device:\t\t%s\n",tmp->dev);
		}
		if(order!=-1 && id>order)
			tmp=NULL;
		else if(order==-1){
			printf("#################################\n");
			tmp=tmp->next;
		}else tmp=tmp->next;

		id++;
	}
}
