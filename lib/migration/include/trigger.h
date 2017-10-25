/*
 * Signal-based triggering APIs.
 */

#ifndef _TRIGGER_H
#define _TRIGGER_H

/*
 * If triggering a migration via signals, clear flags used to communicate
 * with threads that they should migrate.
 */
void clear_migrate_flag();

#endif /* _TRIGGER_H */

