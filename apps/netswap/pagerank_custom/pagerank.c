#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define DAMPING_FACTOR 0.85 // Damping factor for PageRank algorithm
#define NUM_NODES 7500 // Number of nodes in the graph
#define NUM_EDGES 20000 // Number of edges in the graph
#define ITERATIONS 40
#define ARR_SZ 9999
#define PAGERANK 1
#define PRINT_DETAILS 0

typedef struct{
	int edges[NUM_NODES];
	int* arr;
} gNode;

// Function to initialize the graph
void initialize_graph(gNode* graph) {
    popcorn_check_migrate();
    int i, j;
    for (i = 0; i < NUM_NODES; i++) {
        for (j = 0; j < NUM_NODES; j++) {
            graph[i].edges[j] = 0;
        }
    	graph[i].arr = (int*) calloc(ARR_SZ, sizeof(int));
    }
    popcorn_check_migrate();
}

// Function to add an edge to the graph
void add_edge(gNode* graph, int start, int end) {
    popcorn_check_migrate();
    graph[start].edges[end] = 1;
    popcorn_check_migrate();
}

// Function to print the graph
void print_graph(gNode* graph, int num_nodes) {
    popcorn_check_migrate();
    int i, j;
    for (i = 0; i < num_nodes; i++) {
        printf("Node %d: ", i);
        for (j = 0; j < num_nodes; j++) {
            if (graph[i].edges[j] == 1) {
                printf("%d ", j);
            }
        }
        printf("\n");
    }
    popcorn_check_migrate();
}

#if PAGERANK
//void calc_rank(int i){
void calc_rank(gNode* graph, double* ranks, int i, int* out_degrees){
//    	popcorn_check_migrate();
	int j, k;
	int num_nodes = NUM_NODES;
        double old_ranks[NUM_NODES];
	//int* out_degrees = (int *) calloc(num_nodes, sizeof(int));

        // Save the old ranks
        for (j = 0; j < num_nodes; j++) {
            old_ranks[j] = ranks[j];
        }
	printf("=====> 1 \n");
	//memset(old_degrees, 0, sizeof(int)*num_nodes);
        for (j = 0; j < num_nodes; j++) {
            double rank_sum = 0;
            for (k = 0; k < num_nodes; k++) {
                if (graph[k].edges[j] == 1) {
                    rank_sum += old_ranks[k] / ((double)out_degrees[k]);
                }
            }
            ranks[j] = (1 - DAMPING_FACTOR) / num_nodes + DAMPING_FACTOR * rank_sum;
        }
	printf("=====> 3 \n");
	//free(out_degrees);	
    	popcorn_check_migrate();
}
 
touch_arr(gNode* graph){
	for(int i=0; i<NUM_NODES; i++)
		memset(graph[i].arr, 1, sizeof(int)*ARR_SZ);
}

// Function to run the PageRank algorithm
void pagerank(gNode* graph, double* ranks, int num_nodes) {
    popcorn_check_migrate();
    int i, j, k;

    // Initialize the ranks to 1/N
    for (i = 0; i < num_nodes; i++) {
        ranks[i] = 1.0 / num_nodes;
    }

    int* out_degrees = (int *) calloc(num_nodes, sizeof(int));
    for (k = 0; k < num_nodes; k++) {
	    out_degrees[k] = 0;
	    for (j = 0; j < num_nodes; j++) {
		    if (graph[k].edges[j] == 1) {
			    out_degrees[k]++;
		    }
	    }
    }
	   
    // Run the PageRank algorithm for 100 iterations
    for (i = 0; i < ITERATIONS; i++) {
	printf("Iteration %d \n", i);
    	calc_rank(graph, ranks, i, out_degrees);
	if(i==ITERATIONS-1)
		touch_arr(graph);
  	//calc_rank(i);
    }
    free(out_degrees);	
    popcorn_check_migrate();
}
#endif

int main() {
    int num_nodes, num_edges, i, start, end;
    clock_t startT, endT, end_warmup;
    startT = time(NULL);

#if 0
    // Read in the number of nodes and edges
    printf("Enter the number of nodes: ");
    scanf("%d", &num_nodes);
    printf("Enter the number of edges: ");
    scanf("%d", &num_edges);
#endif

    num_nodes = NUM_NODES;
    num_edges = NUM_EDGES;

    gNode* graph = (gNode*) malloc(sizeof(gNode)* NUM_NODES);
    double* ranks = (double*) malloc(sizeof(double)* NUM_NODES);

    // Initialize the graph
    initialize_graph(graph);

    // Generate random edges
    //srand(time(NULL));
    for (i = 0; i < num_edges; i++) {
        start = rand() % num_nodes;
        end = rand() % num_nodes;
        add_edge(graph, start, end);
    }

#if PRINT_DETAILS 
    // Print the graph
    print_graph(graph, num_nodes);
#endif

#if PAGERANK
    // Run the PageRank algorithm
    pagerank(graph, ranks, num_nodes);

#if PRINT_DETAILS
    printf("========================================= RANKS ==========================================\n");
    // Print the PageRank scores
    for (i = 0; i < num_nodes; i++) {
	    printf("Node %d: %.4f\n", i, ranks[i]);
    }
#endif
#endif

    endT = time(NULL);
    printf("Time taken: %ld seconds\n", (endT-startT));
}

