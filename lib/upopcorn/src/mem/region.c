#include "region.h"
#include "uthash.h"
#include "config.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

region_t* region_new(int remote)
{
	region_t* ret;
	ret = (region_t*)pmalloc(sizeof(region_t));
	if(!ret)
		up_log("%s: error!!!\n", __func__);
	ret->remote = remote;
	ret->region_nb_pages = 0;
	ret->region_pages = NULL;
	return ret;
}

void region_delete(region_t* map){/*TODO*/}

void region_init_pages(region_t* map, int present)
{
	assert(!map->region_pages); /* To avoid re-allocation */
	map->region_nb_pages = map->length/DSM_PAGE_SIZE;
	map->region_pages = pcalloc(map->region_nb_pages, sizeof(char));
	memset(map->region_pages, present, map->region_nb_pages);//we can avoid memset
}

void region_extend_pages(region_t* map, int present)
{
	struct page_s *old = map->region_pages;
	uint64_t old_nb = map->region_nb_pages;
	assert(map->region_pages);

	map->region_nb_pages = map->length/DSM_PAGE_SIZE;
	map->region_pages = pcalloc(map->region_nb_pages, sizeof(char));
	memset(map->region_pages+old_nb, present, map->region_nb_pages);//we can avoid memset?
	memcpy(map->region_pages, old, old_nb);
}


static int region_get_page_idx(region_t* map, void* addr)
{
	assert(map->region_pages); /* To avoid re-allocation */
	return ((uintptr_t)addr - (uintptr_t)map->addr_start)/DSM_PAGE_SIZE;
}

int region_is_page_present(region_t* map, void* addr, uint64_t size)
{
	assert(map->region_pages); /* To avoid re-allocation */

	uint32_t idx = region_get_page_idx(map, addr);
	return map->region_pages[idx];

}

int region_set_page(region_t* map, void* addr, uint64_t size, int present)
{
	assert(map->region_pages);
	uint32_t idx = region_get_page_idx(map, addr);
	map->region_pages[idx]=present;
	return 0;
}

/*
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
*/
/*
assert(iter->page);
if(page){
	pg_num = addr-map->addr_start/page_size;
	page = &iter->page[pg_num];
}
*/

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
