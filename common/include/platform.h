/*
 * Popcorn-specific platform information.
 *
 * Author: Rob Lyerly <rlyerly@vt.edu>
 * Date: February 13th, 2018
 */

/* The size of a page in the system. */
#define PAGESZ 4096UL

/*
 * Round down & up to the nearest pages, respectively.  Arguments must be of
 * unsigned long/uint64_t type.
 */
#define PAGE_ROUND_DOWN( x ) ((x) & ~(PAGESZ - 1))
#define PAGE_ROUND_UP( x ) PAGE_ROUND_DOWN((x) + PAGESZ - 1)

/* The maximum number of nodes supported by the system. */
#define MAX_POPCORN_NODES 32
