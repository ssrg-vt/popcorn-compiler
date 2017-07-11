/*
 * Support for logging HTM result statuses.  Provides support for rate-limiting
 * logging of entries & streaming of log entries to disk rather than
 * continuously eating memory.
 *
 * Note: these APIs are *not* thread safe!
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: 6/6/2017
 */

// TODO add fine-grained concurrency control for adding log entries

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

  // Setup data structures
  log->capacity = DEFAULT_CAPACITY;
  log->size = 0;
  log->entries = malloc(sizeof(htm_log_entry) * DEFAULT_CAPACITY);
  if(!(log->file = fopen(fn, "w")))
    perror("WARNING: could not open HTM statistics log file");

  // Print log header
  if(fprintf(log->file, ";Thread ID,Start time (ns),End time (ns),Status,"
                        "Function,Call Site\n") < 0)
  {
    perror("WARNING: could not write header to HTM statistics log file");
    if(fclose(log->file))
      perror("WARNING: could not close HTM statistics log file");
  }
}

/* Free the resources used by an htm_log. */
void htm_log_free(htm_log *log)
{
  tsx_assert(log);
  tsx_assert(log->entries);

  log->capacity = 0;
  log->size = 0;
  free(log->entries);
  log->entries = NULL;

  if(log->file && fclose(log->file))
    perror("WARNING: could not close HTM statistics log file");
  log->file = NULL;
}

#ifdef RATE_LIMIT

/* Determine whether or not to filter out an entry based on minimum sampling
 * frequency.  Entry recording application's makespan is always added. */
static inline bool should_add_entry(const htm_log_entry *entry)
{
  tsx_assert(entry);

  if(entry->status == APP_MAKESPAN ||
     entry->end - last_entry_time > MIN_PERIOD)
    return true;
  else return false;
}

#endif /* RATE_LIMIT */

/* Add an element to the back of the htm_log. */
void htm_log_push_back(htm_log *log, htm_log_entry *entry)
{
  tsx_assert(log);
  tsx_assert(log->entries);
  tsx_assert(entry);

#ifdef RATE_LIMIT
  // Rate limit adding entries to the log
  if(!should_add_entry(entry)) return;
#endif /* RATE_LIMIT */

  // Write entries out to disk if we're out of space
  if(log->size >= log->capacity)
  {
    // TODO start a separate thread to do this continuously?
    htm_log_write_entries(log);
    log->size = 0;
  }

  log->entries[log->size] = *entry;
  log->size++;
  last_entry_time = entry->end;
}

/* Get an entry stored in the htm_log. */
const htm_log_entry *htm_log_get(htm_log *log, size_t elem)
{
  tsx_assert(log);
  tsx_assert(log->entries);
  tsx_assert(elem < log->size);
  return &log->entries[elem];
}

/* Write current log entries to disk. */
void htm_log_write_entries(htm_log *log)
{
  size_t i;

  tsx_assert(log);
  tsx_assert(log->entries);

  if(!log->file) return;

  for(i = 0; i < log->size; i++)
  {
    const htm_log_entry *entry = htm_log_get(log, i);
    if(fprintf(log->file, "%d,%lu,%lu,%s,%p,%p\n",
                           entry->tid, entry->start, entry->end,
                           status_name(entry->status),
                           entry->fn, entry->pc) < 0)
    {
      perror("WARNING: could not write to HTM statistics file");
      break;
    }
  }
}
