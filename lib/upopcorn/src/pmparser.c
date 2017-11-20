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
procmap_t* g_last_head =NULL;
procmap_t* g_current =NULL;



void pmparser_init()
{
}


procmap_t* pmparser_new()
{
	void* ret;
	ret = (procmap_t*)pmalloc(sizeof(procmap_t));
	if(!ret)
		printf("%s: error!!!\n", __func__);
	return ret;
}

#define BUF_SIZE 512
int pmparser_parse(int pid){
	char maps_path[500];

	if(pid>=0 ){
		sprintf(maps_path,"/proc/%d/maps",pid);
	}else{
		sprintf(maps_path,"/proc/self/maps");
	}

	FILE* file=fopen(maps_path,"r");
	if(!file){
		fprintf(stderr,"pmparser : cannot open the memory maps, %s\n",strerror(errno));
		return -1;
	}
	int ind=0;
	size_t linesz=BUF_SIZE;
	char *lineptr;
	int fields;
	char c;
	struct procmap_s* list_maps=NULL;
	struct procmap_s* tmp;
	struct procmap_s* current_node=list_maps;

	if(!(lineptr = (char*)pmalloc(BUF_SIZE * sizeof(char)))) 
			return -1;

	while(getline(&lineptr, &linesz, file) >= 0)
        {
		//printf("line read: %s", lineptr);
		tmp=(struct procmap_s*)pmalloc(sizeof(struct procmap_s));
                fields = sscanf(lineptr, "%lx-%lx %s %lx %s %lu %s",
                       (unsigned long*)&tmp->addr_start,  (unsigned long*)&tmp->addr_end, 
			tmp->perm, &tmp->offset, tmp->dev, &tmp->inode, tmp->pathname);

                if(fields < 6)
			printf("maps: less fields (%d) than expected (6 or 7)", fields);

		tmp->length = (unsigned long)tmp->addr_end - 
				(unsigned long)tmp->addr_start;
		tmp->prot.is_r=(tmp->perm[0]=='r');
		tmp->prot.is_w=(tmp->perm[1]=='w');
		tmp->prot.is_x=(tmp->perm[2]=='x');
		tmp->prot.is_p=(tmp->perm[3]=='p');
		tmp->next=NULL;

#if 0
                printf("%lx-%lx %s %lx %s %lu %s;\n",
                       (unsigned long)tmp->addr_start,  (unsigned long)tmp->addr_end, 
			tmp->perm, tmp->offset, tmp->dev, tmp->inode, tmp->pathname);
#endif
		//attach the node
		if(ind==0){
			list_maps=tmp;
			list_maps->next=NULL;
			current_node=list_maps;
		}
		current_node->next=tmp;
		current_node=tmp;
		ind++;
	}


	pfree(lineptr);
	fclose(file);

	g_last_head=list_maps;
	g_current=NULL;
	return 0;
}


procmap_t* pmparser_next(){
	if(g_last_head==NULL) return NULL;
	if(g_current==NULL){
		g_current=g_last_head;
	}else
		g_current=g_current->next;

	return g_current;
}

void pmparser_insert(procmap_t* tmp)
{
	tmp->next=g_last_head;
	g_last_head=tmp;
}

int pmparser_get(void* addr, procmap_t **map, struct page_s **page){
	//int pg_num;
	procmap_t *iter;

	*map = NULL;
	//if(page) = *page = NULL;
	g_current = NULL;
	while((iter = pmparser_next()))
	{
		/* TODO: Quicker search: ordered list/ hashtable ?	*
		 * and modify insertion accordingly			*/
		if(addr < iter->addr_end && addr >= iter->addr_start)
		{
			*map = iter;
			/*
			assert(iter->page);
			if(page){
			pg_num = addr-map->addr_start/page_size;
			page = &iter->page[pg_num];}
			*/
			break;
		}
	}
	if(*map)
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
	/*
	procmap_t* maps_list = g_last_head;
	if(maps_list==NULL) return ;
	procmap_t* act=maps_list;
	procmap_t* nxt=act->next;
	while(act!=NULL){
		pfree(act);
		act=nxt;
		if(nxt!=NULL)
			nxt=nxt->next;
	}
	*/

}

void pmparser_print(procmap_t* map, int order){

	procmap_t* tmp=map;
	int id=0;
	if(order<0) order=-1;
	while(tmp!=NULL){
		//(unsigned long) tmp->addr_start;
		if(order==id || order==-1){
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
