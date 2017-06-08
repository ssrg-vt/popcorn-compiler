/*
 * Support for logging HTM result statuses.  Uses a simple vector purpose built
 * for logging HTM statistics entries (similar to C++'s vector, but doesn't
 * require C++ standard library).  Also provides support for rate-limiting
 * logging of entries & streaming of log entries to disk rather than
 * continuously eating memory.
 *
 * Note: these APIs are *not* thread safe!
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 6/6/2017
 */

// TODO add fine-grained concurrency control for adding log entries
// TODO better error handling for file I/O issues

#include <stdbool.h>
#include "transaction.h"
#include "statistics.h"
#include "tsx_assert.h"

/* Last time an entry was added to the log.  Used to limit the rate at which
 * entries can be added to the log. */
static unsigned long last_entry_time = 0;

/* Initialize an empty htm_log. */
void htm_log_init(htm_log *log, const char *fn)
{
  tsx_assert(log);
  tsx_assert(fn);

  log->capacity = DEFAULT_CAPACITY;
  log->size = 0;
  log->entries = malloc(sizeof(htm_log_entry) * DEFAULT_CAPACITY);
  log->file = fopen(fn, "w");
  tsx_assert(log->file);
  fprintf(log->file, ";Thread ID,Start time (ns),End time (ns),Status,"
                     "Function,Call Site\n");
}

/* Free the resources used by an htm_log. */
void htm_log_free(htm_log *log)
{
  tsx_assert(log);
  tsx_assert(log->entries);
  tsx_assert(log->file);

  log->capacity = 0;
  log->size = 0;
  free(log->entries);
  log->entries = NULL;
  fclose(log->file);
  log->file = NULL;
}

/* Determine whether or not to filter out an entry based on minimum sampling
 * frequency.  Entry recording application's makespan is always added. */
static inline bool add_entry(const htm_log_entry *entry)
{
  tsx_assert(entry);

  if(entry->status == APP_MAKESPAN ||
     entry->end - last_entry_time > MIN_PERIOD)
    return true;
  else return false;
}

/* Add an element to the back of the htm_log. */
void htm_log_push_back(htm_log *log, htm_log_entry *entry)
{
  tsx_assert(log);
  tsx_assert(log->entries);
  tsx_assert(entry);

  // Rate limit adding entries to the log
  if(!add_entry(entry)) return;

  if(log->size >= log->capacity)
  {
    if(log->capacity >= MAX_CAPACITY)
    {
      // The log is too large to keep expanding, write entries out to disk
      // TODO start a separate thread to do this continually?
      htm_log_write_entries(log);
      log->size = 0;
    }
    else
    {
      // Double the log's size & keep going
      log->capacity *= 2;
      log->entries = realloc(log->entries,
                             sizeof(htm_log_entry) * log->capacity);
    }
  }

  log->entries[log->size] = *entry;
  log->size++;
  last_entry_time = entry->end;
}

/* Get an entry stored in the htm_log. */
const htm_log_entry *htm_log_get(htm_log *log, size_t elem)
{
  tsx_assert(log);
  tsx_assert(elem < log->size);
  return &log->entries[elem];
}

void htm_log_write_entries(htm_log *log)
{
  size_t i;

  tsx_assert(log);
  tsx_assert(log->entries);

  for(i = 0; i < log->size; i++)
  {
    const htm_log_entry *entry = htm_log_get(log, i);
    fprintf(log->file, "%lu,%lu,%lu,%s,%p,%p\n",
                        entry->tid, entry->start, entry->end,
                        status_name(entry->status),
                        entry->fn, entry->pc);
  }
}
