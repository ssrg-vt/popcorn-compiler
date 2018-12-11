#include "region.h"
#include "uthash.h"
#include "config.h"
#include <stdlib.h>
#include <stdio.h>

region_t* region_new()
{
	region_t* ret;
	ret = (region_t*)pmalloc(sizeof(region_t));
	if(!ret)
		up_log("%s: error!!!\n", __func__);
	ret->region_pages = NULL;
	ret->region_nb_pages = 0;
	return ret;
}

void region_delete(region_t* map){/*TODO*/}

int region_register_page(region_t* map, void* addr, uint64_t size)
{
	page_t* page = page_new();
	if(page==NULL)
		return -1;
	page->page_start= (uintptr_t) addr;
	page->page_size=size;
	page->page_prot=map->prot;

	map->region_nb_pages+=1;

	HASH_ADD_PTR((map->region_pages), page_start, page);
	return 0;
}

page_t* region_find_page(region_t* map, void* addr)
{
	page_t* page;
	HASH_FIND_PTR((map->region_pages), &addr, page);
	return page;
}

void region_print(region_t* map)
{
	region_t* tmp=map;
	up_log("Range:\t\t%p-%p\n",tmp->addr_start,tmp->addr_end);
	up_log("Backed by:\t%s\n",strlen(tmp->pathname)==0?"[anonym*]":tmp->pathname);
	up_log("Length:\t\t%ld\n",tmp->length);
	up_log("Offset:\t\t%ld\n",tmp->offset);
	up_log("Permissions:\t%s\n",tmp->perm);
	up_log("Inode:\t\t%lu\n",tmp->inode);
	up_log("Device:\t\t%s\n",tmp->dev);
	up_log("Node address :\t%p\n", tmp);
}
