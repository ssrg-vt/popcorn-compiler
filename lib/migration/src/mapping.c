#include <stdlib.h>
#include <stdio.h>
#include "config.h"

/* Default node ID if no mapping is available. */
static int default_node = 0;
void set_default_node(int node) { default_node = node; }

/* A Popcorn thread ID -> node mapping for a given application region. */
typedef struct mapping {
  size_t region; // The application region number
  size_t num; // The number of node mappings for this region
  int *node; // node mappings, use PTID as index
} mapping_t;

static size_t num_mappings = 0; // Number of mappings parsed from file
static mapping_t *mappings = NULL; // The guts

/* Free any dynamically-allocated data. */
static void __attribute__((destructor)) cleanup()
{
  size_t i;

  if(mappings)
  {
    for(i = 0; i < num_mappings; i++)
      if(mappings[i].node) free(mappings[i].node);
    free(mappings);
  }

  num_mappings = 0;
  mappings = NULL;
}

/*
 * Users can tell the runtime the name of the file containing the thread
 * schedule by setting the POPCORN_THREAD_SCHEDULE environment variable.
 * Otherwise, the runtime will look for the file DEF_THREAD_SCHEDULE.
 */
#define DEF_THREAD_SCHEDULE "thread-schedule.txt"
#define ENV_POPCORN_THREAD_SCHEDULE "POPCORN_THREAD_SCHEDULE"

/* Comparison function for sorting by region numbers. */
int region_compare(const void *a, const void *b)
{
  mapping_t *map_a = (mapping_t *)a;
  mapping_t *map_b = (mapping_t *)b;
  if(map_a->region < map_b->region) return -1;
  else if(map_a->region > map_b->region) return 1;
  else return 0;
}

/*
 * Parse the mapping file, if one is available. Files contain a Popcorn
 * thread ID (PTID) -> node mapping in the following format:
 *
 *  <region #> <# entries> <PTID 0 node> ... <PTID N node>
 *
 * Region number: an integer marking a user-defined region ID, or -1 for the
 *                default mapping
 *
 * Number of node entries: an unsigned integer noting the number of node
 *                         mappings in the current line
 *
 * PTID node number: an integer representing the node on which the thread
 *                   (denoted by its index in the mapping list) should execute
 *
 * The file may contain multiple lines, one per region.  Regions are
 * implementation-dependent and may be defined by the user or compiler.
 */
static void __attribute__((constructor)) read_mapping_schedule()
{
  int c, read;
  size_t i, j;
  const char *fn;
  FILE *fp;

  if(!(fn = getenv(ENV_POPCORN_THREAD_SCHEDULE))) fn = DEF_THREAD_SCHEDULE;
  if(!(fp = fopen(fn, "r")))
  {
#if _DEBUG == 1
    perror("Could not open thread schedule file");
#endif
    return;
  }

  // Start by figuring out how many mappings are in the file
  num_mappings = 1;
  while((c = getc(fp)) != EOF)
    if(c == '\n') num_mappings++;
  fseek(fp, 0, SEEK_SET);

  // Allocate storage & parse file
  mappings = (mapping_t *)calloc(num_mappings, sizeof(mapping_t));
  for(i = 0; i < num_mappings; i++)
  {
    // The first few fields are fixed and will tell us the variable parts
    read = fscanf(fp, "%lu %lu", &mappings[i].region, &mappings[i].num);

    // Check if we accidentally have an extra newline character
    if(read == EOF)
    {
      num_mappings--;
      break;
    }
    // Otherwise, ensure the format wasn't screwed up
    else if(read != 2)
    {
#if _DEBUG == 1
      fprintf(stderr, "Parsing error: invalid thread mapping "
                      "format, line %lu\n", i);
#endif
      cleanup();
      return;
    }

    // Allocate storage & parse node mapping list
    mappings[i].node = calloc(mappings[i].num, sizeof(int));
    for(j = 0; j < mappings[i].num; j++)
    {
      read = fscanf(fp, "%d", &mappings[i].node[j]);
      if(read != 1)
      {
#if _DEBUG == 1
        fprintf(stderr, "Parsing error: not enough node "
                        "mappings, line %lu\n", i);
#endif
        cleanup();
        return;
      }
    }
  }

  // Sort so we can do a binary search based on region ID
  qsort(mappings, num_mappings, sizeof(mapping_t), region_compare);

#if _DEBUG == 1
  printf("-> Thread schedule <-\n");
  for(i = 0; i < num_mappings; i++)
  {
    printf("Region %lu: %lu mappings", mappings[i].region, mappings[i].num);
    for(j = 0; j < mappings[i].num; j++) printf(" %d", mappings[i].node[j]);
    printf("\n");
  }
#endif

  fclose(fp);
}

/* Comparison function for finding region. */
int region_find(const void *k, const void *m)
{
  size_t key = *(size_t *)k;
  mapping_t *map = (mapping_t *)m;
  if(key < map->region) return -1;
  else if(key > map->region) return 1;
  else return 0;
}

int get_node_mapping(size_t region, int ptid)
{
  if(ptid < 0) return default_node;
  mapping_t *map = bsearch(&region, mappings, num_mappings,
                           sizeof(mapping_t), region_find);
  if(!map) return default_node;

  if((size_t)ptid < map->num) return map->node[ptid];
  return default_node;
}

