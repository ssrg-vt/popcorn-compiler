#ifndef _DEBUG_LOG_H
#define _DEBUG_LOG_H

/*
 * Open a per-thread log & log a statement.  Valid regardless of migration.
 * @param format a message/format descriptor
 * @param ... arguments to format descriptor
 * @return the number of characters printed (excluding ending null byte) or -1
 *         if there was an error
 */
int popcorn_log(const char *format, ...);

#endif

