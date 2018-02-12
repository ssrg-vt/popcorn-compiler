/*
 * Popcorn thread ID -> node mappings as specified by a thread schedule.
 */

#ifndef _MAPPING_H
#define _MAPPING_H

#include <stddef.h>

/* The default node on which to execute if no thread schedule is available. */
void set_default_node(int node);

/*
 * Return the node on which a thread should execute for an application region.
 *
 * @param region the user/compiler-defined region
 * @param ptid the Popcorn thread ID
 * @return the node on which to execute
 */
int get_node_mapping(size_t region, int ptid);

#endif /* _MAPPING_H */

