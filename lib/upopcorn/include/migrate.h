#ifndef _MIGRATE_H
#define _MIGRATE_H


/**
 * Migrate thread.  The optional callback function will be invoked before
 * execution resumes on destination architecture.
 *
 * @param callback a callback function to be invoked before execution resumes
 *                 on destination architecture
 * @param callback_data data to be passed to the callback function
 */
//void new_migrate(int nid, void (*callback)(void*), void *callback_data);
void new_migrate(int nid);

int get_context(void **ctx, int *size);


#endif /* _MIGRATE_H */

