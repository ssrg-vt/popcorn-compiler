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
#include "func.h"
#include "query.h"
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
  __log = fopen(LOG_FILE, "a");
  ASSERT(__log, "could not open log file\n");
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
  if(__log) fclose(__log);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Initialization & teardown
///////////////////////////////////////////////////////////////////////////////

/*
 * Open the specified ELF file and initialize DWARF debugging information.
 * If at any point we fail, the function internally cleans up any previously
 * opened resources and returns NULL.
 */
st_handle st_init(const char* fn)
{
  int i, ret = 0;
  const char* id;
  Elf64_Ehdr* ehdr;
  Dwarf_Error err;
  st_handle handle;

  if(!fn) goto return_null;

  TIMER_START(st_init);
  ST_INFO("Initializing handle for '%s'\n", fn);

  if(!(handle = (st_handle)malloc(sizeof(struct _st_handle)))) goto return_null;
  handle->fn = fn;

  /* Initialize libelf data */
  if((handle->fd = open(fn, O_RDONLY, 0)) < 0) goto free_handle;
  if(!(handle->elf = elf_begin(handle->fd, ELF_C_READ, NULL))) goto close_file;

  if(!(ehdr = elf64_getehdr(handle->elf))) goto close_elf;
  handle->arch = ehdr->e_machine;
  if(!(id = elf_getident(handle->elf, NULL))) goto close_elf;
  handle->ptr_size = (id[EI_CLASS] == ELFCLASS64 ? 8 : 4);

  /* Read call site metadata */
  handle->sites_count = get_num_entries(handle->elf, SECTION_ST_ID);
  if(handle->sites_count > 0)
  {
    handle->sites_id = get_call_sites(handle->elf, SECTION_ST_ID);
    handle->sites_addr = get_call_sites(handle->elf, SECTION_ST_ADDR);
    if(!handle->sites_id || !handle->sites_addr) goto close_elf;
#if _LIVE_VALS == STACKMAP_LIVE_VALS
    handle->live_vals_count = get_num_entries(handle->elf, SECTION_ST_LIVE);
    handle->live_vals = get_call_site_values(handle->elf, SECTION_ST_LIVE);
    if(!handle->live_vals) goto close_elf;
#endif
    ST_INFO("Found %lu call sites\n", handle->sites_count);
  }
  else
  {
    ST_WARN("no call site information\n");
    goto close_elf;
  }

  /* Initialize libdwarf data */
  ret = DWARF_CHK(dwarf_elf_init(handle->elf,
                                 DW_DLC_READ,
                                 NULL, NULL,
                                 &handle->dbg, &err),
                  "dwarf_elf_init");
  if(ret == DW_DLV_NO_ENTRY)
  {
    ST_WARN("no debugging information\n");
    goto close_elf;
  }

  // Note: clang doesn't emit .debug_aranges by default
  ret = DWARF_CHK(dwarf_get_aranges(handle->dbg,
                                    &handle->aranges,
                                    (Dwarf_Signed*)&handle->arange_count,
                                    &err),
                  "dwarf_get_aranges");
  if(ret == DW_DLV_NO_ENTRY)
  {
    ST_WARN("no address range information\n");
    goto finish_dwarf;
  }
  ST_INFO("Found %llu address ranges\n", handle->arange_count);

  // Note: GCC and LLVM generate .debug_frame or .eh_frame (superset of
  // .debug_frame) depending on the architecture.  However, sometimes the other
  // or both are emitted.  The only safe thing to do is to grab both.
  ret = DWARF_CHK(dwarf_get_fde_list(handle->dbg,
                                     &handle->cies,
                                     &handle->cie_count,
                                     &handle->fdes,
                                     &handle->fde_count,
                                     &err),
                  "dwarf_get_fde_list");
  if(ret == DW_DLV_NO_ENTRY)
  {
    handle->cies = NULL;
    handle->cie_count = 0;
    handle->fdes = NULL;
    handle->fde_count = 0;
  }

  ret = DWARF_CHK(dwarf_get_fde_list_eh(handle->dbg,
                                        &handle->cies_eh,
                                        &handle->cie_count_eh,
                                        &handle->fdes_eh,
                                        &handle->fde_count_eh,
                                        &err),
                  "dwarf_get_fde_list_eh");
  if(ret == DW_DLV_NO_ENTRY)
  {
    handle->cies_eh = NULL;
    handle->cie_count_eh = 0;
    handle->fdes_eh = NULL;
    handle->fde_count_eh = 0;
  }

  if((handle->fde_count + handle->fde_count_eh) == 0)
  {
    ST_WARN("no frame description entry information\n");
    goto cleanup_arange;
  }
  ST_INFO("Found %lld frame description entries\n",
          handle->fde_count + handle->fde_count_eh);

  /*
   * Cache starting function information, which will be used to check for
   * halting conditions when rewriting.
   */
  // Note: threading start function may not be present
  handle->start_main = get_func_by_name(handle, START_MAIN_CU, START_MAIN);
  handle->start_thread = get_func_by_name(handle, START_THREAD_CU, START_THREAD);
  if(!handle->start_main) goto cleanup_fdes;

  /* Get register & stack properties, initialize unwinding. */
  if(!(handle->regops = get_regops(handle->arch))) goto cleanup_func_info;
  if(!(handle->props = get_properties(handle->arch))) goto cleanup_func_info;
  init_unwinding(handle);

  TIMER_STOP(st_init);

  return handle;

cleanup_func_info:
  free_func_info(handle, handle->start_main);
  if(handle->start_thread) free_func_info(handle, handle->start_thread);
cleanup_fdes:
  if(handle->fdes)
    dwarf_fde_cie_list_dealloc(handle->dbg,
                               handle->cies,
                               handle->cie_count,
                               handle->fdes,
                               handle->fde_count);
  if(handle->fdes_eh)
    dwarf_fde_cie_list_dealloc(handle->dbg,
                               handle->cies_eh,
                               handle->cie_count_eh,
                               handle->fdes_eh,
                               handle->fde_count_eh);
cleanup_arange:
  for(i = 0; i < handle->arange_count; i++)
    dwarf_dealloc(handle->dbg, handle->aranges[i], DW_DLA_ARANGE);
  dwarf_dealloc(handle->dbg, handle->aranges, DW_DLA_LIST);
finish_dwarf:
  dwarf_finish(handle->dbg, &err);
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
 * DWARF and ELF resources, so internally things may fail silently.
 */
void st_destroy(st_handle handle)
{
  Dwarf_Unsigned i;
  Dwarf_Error err;

  if(!handle) return;

  TIMER_START(st_destroy);
  ST_INFO("Cleaning up handle for '%s'\n", handle->fn);

  free_func_info(handle, handle->start_main);
  if(handle->start_thread) free_func_info(handle, handle->start_thread);
  if(handle->fdes)
    dwarf_fde_cie_list_dealloc(handle->dbg,
                               handle->cies,
                               handle->cie_count,
                               handle->fdes,
                               handle->fde_count);
  if(handle->fdes_eh)
    dwarf_fde_cie_list_dealloc(handle->dbg,
                               handle->cies_eh,
                               handle->cie_count_eh,
                               handle->fdes_eh,
                               handle->fde_count_eh);
  for(i = 0; i < handle->arange_count; i++)
    dwarf_dealloc(handle->dbg, handle->aranges[i], DW_DLA_ARANGE);
  dwarf_dealloc(handle->dbg, handle->aranges, DW_DLA_LIST);
  dwarf_finish(handle->dbg, &err);
  elf_end(handle->elf);
  close(handle->fd);
  free(handle);

  TIMER_STOP(st_destroy);
}

