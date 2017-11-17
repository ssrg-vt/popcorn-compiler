/*
 * Main stack transformation functions.  In general these functions are used
 * to drive stack transformation.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 10/23/2015
 */

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>

#include "stack_transform.h"
#include "unwind.h"
#include "util.h"

#ifdef _LOG
/* Log file descriptor */
FILE* __log = NULL;
#endif

/* Userspace constructors & destructors */
extern void __st_userspace_ctor(void);
extern void __st_userspace_dtor(void);

static void __attribute__((constructor))
__st_ctor(void)
{
#ifdef _LOG
#ifndef _PER_LOG_OPEN
  __log = fopen(LOG_FILE, "a");
  ASSERT(__log, "could not open log file\n");
#endif
  ST_RAW_INFO("\n");
  ST_INFO("--> New execution started <--\n");
  ST_INFO("PID: %u\n", getpid());
#endif

  __st_userspace_ctor();
}

void /*__attribute__((destructor))*/
__st_dtor(void)
{
  __st_userspace_dtor();

#ifdef _LOG
  ST_INFO("--> Finished execution <--\n");
#ifndef _PER_LOG_OPEN
  if(__log) fclose(__log);
#endif
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Initialization & teardown
///////////////////////////////////////////////////////////////////////////////

/*
 * Open the specified ELF file and initialize rewriting metadata.  If at any
 * point we fail, the function internally cleans up any previously opened
 * resources and returns NULL.
 */
st_handle st_init(const char* fn)
{
  const char* id;
  Elf64_Ehdr* ehdr;
  st_handle handle;

  if(!fn) goto return_null;

  TIMER_START(st_init);
  ST_INFO("Initializing handle for '%s'\n", fn);

  if(!(handle = (st_handle)pmalloc(sizeof(struct _st_handle)))) goto return_null;
  handle->fn = fn;

  /* Initialize libelf data */
  if((handle->fd = open(fn, O_RDONLY, 0)) < 0) goto free_handle;
  if(!(handle->elf = elf_begin(handle->fd, ELF_C_READ, NULL))) goto close_file;

  /* Get architecture-specific information */
  if(!(ehdr = elf64_getehdr(handle->elf))) goto close_elf;
  handle->arch = ehdr->e_machine;
  if(!(id = elf_getident(handle->elf, NULL))) goto close_elf;
  handle->ptr_size = (id[EI_CLASS] == ELFCLASS64 ? 8 : 4);

  /* Read unwinding addresses */
  handle->unwind_addr_count = get_num_entries(handle->elf,
                                              SECTION_ST_UNWIND_ADDR);
  if(handle->unwind_addr_count > 0)
  {
    handle->unwind_addrs = get_section_data(handle->elf,
                                            SECTION_ST_UNWIND_ADDR);
    if(!handle->unwind_addrs) goto close_elf;
    ST_INFO("Found %lu per-function unwinding metadata entries\n",
            handle->unwind_addr_count);
  }
  else
  {
    ST_WARN("no per-function unwinding metadata\n");
    goto close_elf;
  }

  /* Read unwinding information */
  handle->unwind_count = get_num_entries(handle->elf, SECTION_ST_UNWIND);
  if(handle->unwind_count > 0)
  {
    handle->unwind_locs = get_section_data(handle->elf, SECTION_ST_UNWIND);
    if(!handle->unwind_locs ) goto close_elf;
    ST_INFO("Found %lu callee-saved frame unwinding entries\n",
            handle->unwind_count);
  }
  else
  {
    ST_WARN("no frame unwinding information\n");
    goto close_elf;
  }

  /* Read call site metadata */
  handle->sites_count = get_num_entries(handle->elf, SECTION_ST_ID);
  if(handle->sites_count > 0)
  {
    handle->sites_id = get_section_data(handle->elf, SECTION_ST_ID);
    handle->sites_addr = get_section_data(handle->elf, SECTION_ST_ADDR);
    if(!handle->sites_id || !handle->sites_addr) goto close_elf;
    ST_INFO("Found %lu call sites\n", handle->sites_count);
  }
  else
  {
    ST_WARN("no call site information\n");
    goto close_elf;
  }

  /* Read live value location records */
  handle->live_vals_count = get_num_entries(handle->elf, SECTION_ST_LIVE);
  if(handle->live_vals_count > 0)
  {
    handle->live_vals = get_section_data(handle->elf, SECTION_ST_LIVE);
    if(!handle->live_vals) goto close_elf;
    ST_INFO("Found %lu live value location records\n",
            handle->live_vals_count);
  }
  else
  {
    ST_WARN("no live value location records\n");
    goto close_elf;
  }

  /* Read architecture-specific live value location records */
  // Note: unlike other sections, we may not have any architecture-specific
  // live value records
  handle->arch_live_vals_count = get_num_entries(handle->elf,
                                                 SECTION_ST_ARCH_LIVE);
  if(handle->arch_live_vals_count > 0)
  {
    handle->arch_live_vals = get_section_data(handle->elf,
                                              SECTION_ST_ARCH_LIVE);
    if(!handle->arch_live_vals) goto close_elf;
    ST_INFO("Found %lu architecture-specific live value location records\n",
            handle->arch_live_vals_count);
  }
  else
    ST_INFO("no architecture-specific live value location records\n");

  /* Get architecture-specific register operations & stack properties. */
  if(!(handle->regops = get_regops(handle->arch))) goto close_elf;
  if(!(handle->props = get_properties(handle->arch))) goto close_elf;

  TIMER_STOP(st_init);

  return handle;

close_elf:
  elf_end(handle->elf);
close_file:
  close(handle->fd);
free_handle:
  free(handle);
return_null:
  return NULL;
}

/*
 * Destroy a previously opened handle.  No bugs are checked when cleaning up
 * ELF resources, so internally things may fail silently.
 */
void st_destroy(st_handle handle)
{
  if(!handle) return;

  TIMER_START(st_destroy);
  ST_INFO("Cleaning up handle for '%s'\n", handle->fn);

  elf_end(handle->elf);
  close(handle->fd);
  free(handle);

  TIMER_STOP(st_destroy);
}

