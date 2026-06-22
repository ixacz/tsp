#include <stdio.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#define KROA100_OPTIMAL 21282.0
#define PR1002_OPTIMAL 259045.0
#define SY40_OPTIMAL 2414824 

#define BEST_TOUR_SOLUTION SY40_OPTIMAL

const char* CYRIAN_CITIES_40[] = {
    "", // Padding index 0 (since TSPLIB IDs start at 1)
    "Damascus", "Aleppo", "Homs", "Hama", "Latakia", 
    "Deir ez-Zor", "Raqqa", "Al-Hasakah", "Tartus", "As-Suwayda",
    "Daraa", "Idlib", "Palmyra", "Abu Kamal", "Qamishli", 
    "Manbij", "Al-Bab", "Afrin", "As-Safira", "Ain al-Arab",
    "Baniyas", "Jableh", "Al-Qardaha", "Al-Qusayr", "Ar-Rastan", 
    "Salamiyah", "Maarat al-Numan", "Jisr al-Shughur", "Douma", "Zabadani",
    "An-Nabek", "Yabroud", "Shahba", "Salkhad", "Mayadin", 
    "Amuda", "Al-Malikiyah", "Al-Thawrah", "Tell Abyad", "Ras al-Ayn"
};

typedef struct {
    int id;
    int x;
    int y;
} Node;

typedef struct {
    int from;
    int to;

    int distance;
    double penalized_cost;
} Edge;

#define MAX_CANDIDATES 5

typedef struct {
    Node* nodes;
    int node_count;
    // Current Lagrange-Multiplier value; node penalty (pi).
    double* pis;
    double* best_pis;
    // Degree of the node in te current 1-Tree.
    int* degrees;

    Edge* edges;
    int edge_count;
    
    int max_candidates;
    int* candidates;
    
    int* upper_bound_tour;
    double upper_bound;
    double epsilon;

    double curr_lower_bound;
    double best_lower_bound;

    int* tour;
    int* pos;
} GraphContext;

void allocate_candidate_matrix(GraphContext* ctx) {
    ctx->max_candidates = MAX_CANDIDATES;

    size_t total_elements = (size_t) ctx->node_count * (size_t) MAX_CANDIDATES;

    ctx->candidates = malloc(total_elements * sizeof(int));
    if (!ctx->candidates) {
        fprintf(stderr, "Error allocating candidates.");
        exit(1);
    }

    for (size_t i = 0; i < total_elements; i++) {
        ctx->candidates[i] = -1;
    }
}

#define GET_CANDIDATE(ctx, node, rank) ((ctx)->candidates[(node) * (ctx)->max_candidates + (rank)])

void init_graph_ctx(GraphContext* ctx, int n) {
    ctx->edge_count = ctx->node_count * (ctx->node_count - 1) / 2;
    ctx->edges = malloc(ctx->edge_count * sizeof(Edge));
    ctx->pis = (double*) calloc(n, sizeof(double));
    ctx->best_pis = (double*) calloc(n, sizeof(double));
    ctx->degrees = (int*) malloc(n * sizeof(double));
    ctx->upper_bound_tour = (int*) malloc(n * sizeof(int));
    allocate_candidate_matrix(ctx);
    ctx->tour = malloc(ctx->node_count * sizeof(int));
    ctx->pos = malloc(ctx->node_count * sizeof(int));
}

// When printing your final optimized tour:
void print_named_tour(GraphContext* ctx) {
    printf("\nOptimized Tour Sequence:\n");
    for (int i = 0; i < ctx->node_count; i++) {
        int node_id = ctx->tour[i] + 1; // Map 0-indexed offset back to TSPLIB ID
        printf(" %s (%d) -> ", CYRIAN_CITIES_40[node_id], node_id);
        if ((i + 1) % 5 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}

void load_cities_from_file(GraphContext* ctx, const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        fprintf(stderr, "[Parser Error] Could not open file: %s\n", filepath);
        exit(1);
    }

    char line[256];
    int num_nodes = 0;
    bool reading_cooredinates = false;
    int nodes_read = 0;

    while (fgets(line, sizeof(line), file)) {
        if (!reading_cooredinates) {
            if (strstr(line, "DIMENSION")) {
                char* colon = strchr(line, ':');
                if (colon) {
                    num_nodes = atoi(colon + 1);
                } else {
                    sscanf(line, "DIMENSION %*s %d", &num_nodes);
                }

                if (num_nodes <= 0) {
                    fprintf(stderr, "[Parser Error] Invalid city count");
                    fclose(file);
                    exit(1);
                }

                ctx->nodes = (Node*) calloc(num_nodes, sizeof(Node));
                ctx->node_count = num_nodes;
                init_graph_ctx(ctx, num_nodes);
            }
            
            if (strstr(line, "NODE_COORD_SECTION")) {
                if (ctx->nodes == NULL) {
                    fprintf(stderr, "[Parser Error] Found NODE_COORD_SECTION before DIMENSION header.\n");
                    fclose(file);
                    exit(1);
                }
                reading_cooredinates = true;
                continue;
            }
        } else {
            if (strstr(line, "EOF") || nodes_read >= num_nodes) {
                break;
            }

            int temp_id;
            double temp_x, temp_y;

            if (sscanf(line, "%d %lf %lf", &temp_id, &temp_x, &temp_y) == 3) {
                int index = nodes_read;

                ctx->nodes[index].id = temp_id - 1;
                ctx->nodes[index].x = temp_x;
                ctx->nodes[index].y = temp_y;
            
                nodes_read++;
            }
        }
    }
    fclose(file);
    printf("[Init] Successfully parced & loaded the data file. Nodes (N): %d, Edge Count: %d\n",
            ctx->node_count, ctx->edge_count);
}


int calculate_euclidean_distance(GraphContext* ctx, int from, int to) {
    long long dx = ctx->nodes[from].x - ctx->nodes[to].x;
    long long dy = ctx->nodes[from].y - ctx->nodes[to].y;
    
    long long squared_distance = (dx * dx) + (dy * dy);

    // TSPLIB specifies standard rounding to the nearest integer.
    return (int) (sqrt(squared_distance) + 0.5);
}

void compute_edges(GraphContext* ctx) {
    if (ctx->edges == NULL && ctx->nodes != NULL) {
        ctx->edge_count = ctx->node_count * (ctx->node_count - 1) / 2;
        ctx->edges = malloc(ctx->edge_count * sizeof(Edge));
    }
    int edge_idx = 0;

    for (int i = 0; i < ctx->node_count; i++) {
        Node* cn = &ctx->nodes[i];

        for (int j = 0; j < ctx->node_count; j++) {
            if (j > i) {
                Node* tn = &ctx->nodes[j];
                int dist = calculate_euclidean_distance(ctx, cn->id, tn->id);

                ctx->edges[edge_idx].from = cn->id;
                ctx->edges[edge_idx].to = tn->id;
                ctx->edges[edge_idx].distance = dist;
                ctx->edges[edge_idx].penalized_cost = dist;
                edge_idx++;
            }
        }
    }
}

void apply_edges_penalized_cost(GraphContext* ctx) {
    for (int i = 0; i < ctx->edge_count; i++) {
        int fromid = ctx->edges[i].from;
        int toid = ctx->edges[i].to;

        double pi1 = ctx->pis[fromid];
        double pi2 = ctx->pis[toid];
        double dist = ctx->edges[i].distance;
        ctx->edges[i].penalized_cost = dist + pi1 + pi2;
    }
}


int compare_edges(const void* a, const void* b) {
    const Edge* edge_a = (const Edge*)a;
    const Edge* edge_b = (const Edge*)b;

    if (edge_a->penalized_cost< edge_b->penalized_cost) return -1;
    if (edge_a->penalized_cost> edge_b->penalized_cost) return 1;

    if (edge_a->distance < edge_b->distance) return -1;
    if (edge_a->distance > edge_b->distance) return 1;

    return 0;
}

void qsort_edges(GraphContext* ctx) {
    qsort(ctx->edges, ctx->edge_count, sizeof(Edge), compare_edges);
}

typedef struct {
    int* parents;
    int* ranks;
    int count;
} DisjointSet;

typedef struct {
    DisjointSet* sets;
    int count;
} DisjointSets;
 
DisjointSet disjointset_create(int count) {
    DisjointSet ds;
    ds.parents = (int*) malloc(count * sizeof(int));
    ds.ranks = (int*) malloc(count * sizeof(int));
    ds.count = count;
  
    for (int i = 0; i < count; i++) {
        ds.parents[i] = i;
        ds.ranks[i] = 0;
    }

    return ds;
}

void disjointset_free(DisjointSet ds) {
    free(ds.parents);
    free(ds.ranks);
} 

int find_root(DisjointSet ds, int city_id) {
    int current = city_id;
    while (current != ds.parents[current]) {
        current = ds.parents[current];
    }
    int root = current;

    current = city_id;
    while (current != root) {
        int next_parent = ds.parents[current];
        ds.parents[current] = root;
        current = next_parent;
    }

    return root;
}

// Merge two city branches together, returns true if merged, false if thay were already connected.
bool union_cities(DisjointSet ds, int city_a, int city_b) {
    int root_a = find_root(ds, city_a);
    int root_b = find_root(ds, city_b);

    if (root_a != root_b) {
        // union by rank, attach the smaller tree under the bigger one.
        if (ds.ranks[root_a] < ds.ranks[root_b]) {
            ds.parents[root_a] = root_b;
        } else if (ds.ranks[root_a] > ds.ranks[root_b]) {
            ds.parents[root_b] = root_a;
        } else {
            ds.parents[root_b] = root_a;
            // increase root_a's rank because we attached root_b into it.
            // so it should be bigger by definition.
            ds.ranks[root_a]++;
        }
        return true;
    }
    return false;
}

double sum_penalized_values(GraphContext* ctx) {
    double total_penalized_values = 0.0;

    for (int i = 0; i < ctx->node_count; i++) {
        total_penalized_values += ctx->pis[i];
    }

    return total_penalized_values;
}

void extract_1_tree_weights(GraphContext* ctx, int spacial_node) {
    double one_tree_penalized_weight = 0.0;
    int mst_edge_count = 0;
    DisjointSet ds = disjointset_create(ctx->node_count);
    int spacial_node_connected_count = 0;

    for (int i = 0; i < ctx->node_count; i++) {
        ctx->degrees[i] = 0;
    }

    for (int i = 0; i < ctx->edge_count; i++) {
        int fromid = ctx->edges[i].from;
        int toid = ctx->edges[i].to;

        if (fromid == spacial_node || toid == spacial_node) {
            if (spacial_node_connected_count < 2) {
                one_tree_penalized_weight += ctx->edges[i].penalized_cost;
                ctx->degrees[fromid]++;
                ctx->degrees[toid]++;
                spacial_node_connected_count++;
            }
            continue;
        } 

        if (union_cities(ds, fromid, toid)) {
            one_tree_penalized_weight += ctx->edges[i].penalized_cost;
            ctx->degrees[fromid]++;
            ctx->degrees[toid]++;
            mst_edge_count++;

            // break once we reach our target edges.
            if (mst_edge_count == ctx->node_count - 2 && spacial_node_connected_count == 2) break;
        }
    }

    disjointset_free(ds);

    // Compute the standard Held-Karp lower bound adjustment: L(pi) = W(T_pi) - 2 * sum(pi_i).
    ctx->curr_lower_bound = one_tree_penalized_weight - 2 * sum_penalized_values(ctx);
}

double calculate_step_size(GraphContext* ctx) {
    double denominator = 0.0;
    //calculate the sum of squares.
    for (int i = 0; i < ctx->node_count; i++) {
        double diff = (double) ctx->degrees[i] - 2.0;
        denominator += diff * diff;
    }

    // printf("Current Denominator = %lf\n", denominator);
    // edge case: if denominator it 0, we found a perfect TSP tour.
    // return 0 to stop shifting penaltes.
    if (denominator == 0.0) {
        printf("[DynamicStepSize] Found Perfict TSP Tour!\n");
        return 0.0;
    }

    // Dynaimc Step Size Formula: Epsilon * (Upper Bound - Current Lower Bound) / Denominator.
    double numerator = ctx->upper_bound - ctx->curr_lower_bound;
    // printf("Current Numerator = %lf\n", numerator);
    double step_size = ctx->epsilon * (numerator) / denominator;
    return step_size;
}

double estimate_target_bound(GraphContext* ctx) {
    int n = ctx->node_count;
    bool* visited = (bool*) calloc(n, sizeof(bool));

    int current_node = 0;
    visited[current_node] = true;
    ctx->upper_bound_tour[0] = current_node;

    double total_ub_cost = 0.0;

    for (int step = 1; step < n; step++) {
        int next_node = -1;
        double min_dist = DBL_MAX;

        for (int i = 0; i < n; i++) {
            if (!visited[i]) {
                double dist = calculate_euclidean_distance(ctx, current_node, i);
                if (dist < min_dist) {
                    min_dist = dist;
                    next_node = i;
                }
            }
        }

        total_ub_cost += min_dist;
        visited[next_node] = true;
        ctx->upper_bound_tour[step] = next_node;
        current_node = next_node;
    }

    total_ub_cost += calculate_euclidean_distance(ctx, current_node, ctx->upper_bound_tour[0]);
    free(visited);

    printf("[Init] Naive Nearest Neighbor Search tour cost (Initial Upper Bound): %lf\n", total_ub_cost);
    return total_ub_cost;
}



// // Valid tsp tour all it cities has exactly 2 edges.
bool is_valid_tsp_tour(GraphContext* ctx) {
    bool is_true_tour = true;

    for (int i = 0; i < ctx->node_count; i++) {
        if (ctx->degrees[i] != 2) {
            is_true_tour = false;
            break;
        }
    }

    return is_true_tour;
}

typedef enum {
    HK_CONVERGED,
    HK_NATURAL_TOUR_FOUND,
    HK_ERROR_MEMORY
} HKStatus;

#define MIN_EPSILON 1e-4

// typedef struct {
//     Edges one_tree;
//     double best_lower_bound;
// } HeldKarpOneTree;

void save_best_pi_values(GraphContext* ctx) {
    memcpy(ctx->best_pis, ctx->pis, ctx->node_count * sizeof(double));
}

HKStatus optimaize_held_karp_relaxation(GraphContext* ctx) {
    int spacial_node = 0;
    ctx->epsilon = 2.0;

    // Determine LKH dynamic initial period scaling.
    int initial_period = ctx->node_count / 2;
    if (initial_period < 10) initial_period = 10;
    if (initial_period > 100) initial_period = 100;

    ctx->best_lower_bound = -DBL_MAX;
    HKStatus status = HK_CONVERGED;

    // The outer loop.
    for (int period = initial_period; 
         period > 0 && ctx->epsilon > MIN_EPSILON;
         period *= 0.5, ctx->epsilon *= 0.5) {
        printf("[Held-Karp] Starting Period Phase. Scale Steps: %d, Epsilon (Step Size Modifier): %e\n",
                period, ctx->epsilon);
        
        bool improvement_found = false;

        // The inner loop.
        for (int step = 1; step <= period; step++) {
            // Update penalized_cost for all edges: new penalized_cost = geometric distance + pi_i + pi_j.
            apply_edges_penalized_cost(ctx);
            // Sort edges in ascending order.
            qsort_edges(ctx);

            extract_1_tree_weights(ctx, spacial_node);
            
            // Evaluate if we found a new higher upper bound.
            if (ctx->curr_lower_bound > ctx->best_lower_bound) {
                printf("    [Lower Bound Improvement Found] Period Step %d: Old Best LB = %lf -> New Best LB = %lf (Diff: %+lf)\n",
                        step, ctx->best_lower_bound, ctx->curr_lower_bound, ctx->curr_lower_bound - ctx->best_lower_bound);

                ctx->best_lower_bound = ctx->curr_lower_bound;
                save_best_pi_values(ctx);
                improvement_found = true;
            }

            double step_size = calculate_step_size(ctx);
            if (step_size == 0.0) {
                printf("    [Step Size] Convergence Achieved! Natural valid tour discovered during subgradient walk.\n");
                status = HK_NATURAL_TOUR_FOUND;
                save_best_pi_values(ctx);
                goto exit;
            }

            // Update transient pi penalties using the subgradient (G_i = d_i - 2).
            for (int i = 0; i < ctx->node_count; i++) {
                int nodeid = ctx->nodes[i].id;
                double gradient = ctx->degrees[nodeid] - 2;
                ctx->pis[nodeid] += step_size * gradient;
            }
        }

        // LKH Monemtum rule: if we improved late in this phase, rise the monemtum
        // and run this exact period scale again befor forcing decay.
        if (improvement_found) {
            period *= 2;
            ctx->epsilon *= 2;
            continue;
        }
    }

exit:
    return status;
}

// Reconstructs the 1-tree and returns the total penalized lower bound cost
double reconstruct_optimal_1tree(GraphContext* ctx, Edge *output_edges) {
    int n = ctx->node_count;
    int edge_idx = 0;
    double total_1tree_cost = 0.0;

    // Workspace arrays for Prim's Algorithm (O(N) space)
    double* min_dist = malloc(n * sizeof(double));
    int* parent = malloc(n * sizeof(int));
    bool* in_mst = calloc(n, sizeof(bool));

    // Initialize Prim's workspace starting from node 1 (skipping node 0)
    for (int i = 1; i < n; i++) {
        min_dist[i] = DBL_MAX;
        parent[i] = -1;
    }
    min_dist[1] = 0.0; // Start building MST from node 1

    // --- STEP 1: Prim's Algorithm on Nodes 1 to N-1 ---
    for (int step = 1; step < n; step++) {
        // Find the unvisited node with the minimum penalized distance
        int u = -1;
        double min_val = DBL_MAX;
        for (int i = 1; i < n; i++) {
            if (!in_mst[i] && min_dist[i] < min_val) {
                min_val = min_dist[i];
                u = i;
            }
        }

        in_mst[u] = true;
        total_1tree_cost += min_val;

        // If it's not the starting node, record the MST edge
        if (parent[u] != -1) {
            output_edges[edge_idx].from = parent[u];
            output_edges[edge_idx].to = u;
            output_edges[edge_idx].penalized_cost = min_val;
            edge_idx++;
        }

        // Update neighbors' configurations
        for (int v = 1; v < n; v++) {
            if (!in_mst[v]) {
                // Penalized distance calculation: c_uv + pi[u] + pi[v]
                double p_dist = calculate_euclidean_distance(ctx, u, v) + ctx->best_pis[u] + ctx->best_pis[v];
                if (p_dist < min_dist[v]) {
                    min_dist[v] = p_dist;
                    parent[v] = u;
                }
            }
        }
    }

    // --- STEP 2: Find the 2 Cheapest Penalized Edges from Node 0 ---
    int best_v1 = -1, best_v2 = -1;
    double min_e1 = DBL_MAX, min_e2 = DBL_MAX;

    for (int v = 1; v < n; v++) {
        double p_dist = calculate_euclidean_distance(ctx, 0, v) + ctx->best_pis[0] + ctx->best_pis[v];
        
        if (p_dist < min_e1) {
            min_e2 = min_e1;
            best_v2 = best_v1;
            
            min_e1 = p_dist;
            best_v1 = v;
        } else if (p_dist < min_e2) {
            min_e2 = p_dist;
            best_v2 = v;
        }
    }

    // Add node 0's two chosen edges to the output structure
    output_edges[edge_idx].from = 0;
    output_edges[edge_idx].to = best_v1;
    output_edges[edge_idx].penalized_cost = min_e1;
    edge_idx++;

    output_edges[edge_idx].from = 0;
    output_edges[edge_idx].to = best_v2;
    output_edges[edge_idx].penalized_cost = min_e2;
    edge_idx++;

    total_1tree_cost += (min_e1 + min_e2);

    // Clean up temporary workspace
    free(min_dist);
    free(parent);
    free(in_mst);

    printf("[1-Tree] Reconstruction complete using locked Pi multipliers. Penalized Cost: %lf\n",
            total_1tree_cost);

    return total_1tree_cost;
}

typedef struct {
    int target;
    double penalized_cost;
} TreeEdge;

// Forword Star Representation.
typedef struct {
    TreeEdge* edges;    // Size: 2 * N (since edges are bidirectional).
                        // Holds the actual paylod data for an edge index `e`.
                        // Witch has the node it points to `target`, and
                        // its penalized distance `penalized_cost`.

    int* head;          // Size: N (points to the start of a node's edges).
                        // Maps a node ID to in index in the `edges` array,
                        // witch acts as an entery point to that node's list
                        // of connections.

    int* next;          // Size: 2 * N (linked list).
                        // Handles the chaining, it tells the program where
                        // to head once it finish with the current edge.

    int edge_count;
} TreeGraph;


void add_tree_edge(TreeGraph* g, int u, int v, double penalized_cost) {
    // 1- An edge slot is claimed at index `e = g->edge_count++`.
    int e = g->edge_count++;

    // 2- The edge at index `e` is loaded with the target node ID `v`,
    //  and its penalized distance `penalized_cost`.
    g->edges[e].target = v;

    // 3- The new edge is hooked to the front of node `u`'s list,
    //  `g->next[e] = g->head[u];`.
    g->edges[e].penalized_cost = penalized_cost;

    // 4- The new entery is updated to point to this new edge,
    //  `g->head[u] = e`.
    g->next[e] = g->head[u];
    g->head[u] = e;
}

// The objective of this function is to start at a `root` node and find
// c_max(foot, j) -- the heaviest penalized edge weight along the uniqe
// path inside the tree form root to every other node.
void bfs_max_edges(TreeGraph* g, int root, int n, double* c_max, bool* visited) {
    // FIFO queue (First In First Out).
    int* queue = malloc(n * sizeof(int));
    // Tracks the fornt of the `queue`. where elements read/popped.
    int head = 0;
    // Tracks the back of the `queue`. where new elements appended.
    int tail = 0;
    
    // Seed the `root` node as visited.
    visited[root] = true;
    // Initialize `root`'s cost to zero.
    c_max[root] = 0.0;
    // And push it into the back of the queue by storing it and advance the tail pointer.
    queue[tail++] = root;
    
    // As long as head is longer the the tail, unexamined nodes are remaining.
    while (head < tail) {
        // Reads the next node from the queue and shifts the `head` pointer right.
        int current_node = queue[head++];
        // Fetches the heaviest edge cost excountered on the path all the way from `root` node
        // to this `current_node`.
        double current_max = c_max[current_node];
        
        // Step through the adjacency list:
        // 1- It looks up `g->head[current_node]` to find the starting point where `current_node`'s
        //  edges are starting in the global array.
        // 2- it extracts the `neighbor` ID stored at that edge slot payload `g->edges[e].target`.
        // 3- after processing, `e = g->next[e].target` jumps directly the next packed edge index
        //  belonging to `current_node`.
        for (int e = g->head[current_node]; e != -1; e = g->next[e]) {
            int neighbor = g->edges[e].target;
            
            // Check if the current_node's neighbor is not visited.
            if (!visited[neighbor]) {
                // If not set it to true.
                visited[neighbor] = true;
                // Get its penalized_cost.
                double edge_penalized_cost = g->edges[e].penalized_cost;
                // Check if its penalized_cost is larger than we have seen till now, if so record it
                // to the max cost encountered up to this point.
                c_max[neighbor] = (edge_penalized_cost > current_max) ? edge_penalized_cost : current_max;
                // Save the neighbor in the queue so it can be used to finds its neighbors.
                queue[tail++] = neighbor;
            }
        }
    }

    free(queue);
}

typedef struct {
    int node_id;
    double alpha;
} AlphaPair;

// Comparison function for sorting alphas lowest to highest
int compare_alphas(const void *a, const void *b) {
    double alpha_a = ((AlphaPair*)a)->alpha;
    double alpha_b = ((AlphaPair*)b)->alpha;
    return (alpha_a > alpha_b) - (alpha_a < alpha_b);
}

void compute_all_candidate_sets(GraphContext* ctx, TreeGraph *g) {
    int node_count = ctx->node_count;
    int max_candidates = ctx->max_candidates;

    // Temporary arrays allocated once, reused for every node loop
    double* c_max = malloc(node_count * sizeof(double));
    AlphaPair* pairs = malloc(node_count * sizeof(AlphaPair));
    bool* visited = malloc(node_count * sizeof(bool));

    for (int i = 0; i < node_count; i++) {
        // Reset all tracking pools for every node pass.
        for (int j = 0; j < node_count; j++) {
            visited[j] = false;
            c_max[j] = 0.0;
        }

        // Run BFS from node 'i' to find max path edges to all other nodes
        bfs_max_edges(g, i, node_count, c_max, visited);

        // Calculate alpha values for all possible neighbors
        for (int j = 0; j < node_count; j++) {
            pairs[j].node_id = j;
            
            if (i == j) {
                pairs[j].alpha = DBL_MAX; // Can't connect a node to itself
            } else {
                // penalized_distance = original_rounded_dist + pi[i] + pi[j]
                double p_dist = calculate_euclidean_distance(ctx, i, j) + ctx->pis[i] + ctx->pis[j];
                
                // alpha = penalized_distance - max_edge_on_path
                double alpha_val = p_dist - c_max[j];
                
                // Clean up tiny floating point precision noise (e.g. -0.0000001 -> 0.0)
                pairs[j].alpha = (alpha_val < 1e-6) ? 0.0 : alpha_val;
            }
        }

        // Sort neighbors so the lowest alphas rise to the top
        qsort(pairs, node_count, sizeof(AlphaPair), compare_alphas);

        // Fill row 'i' of your flat candidate matrix with the top M node IDs
        for (int k = 0; k < max_candidates; k++) {
            ctx->candidates[i * max_candidates + k] = pairs[k].node_id;
        }

        if ((i + 1) % 10 == 0 || (i + 1) == node_count) {
            printf("[Candidates Generation] Computed alpha-sets for %d / %d nodes.\n", 
                    i + 1, node_count);
        }
    }

    // Clean up temporary workspace memory
    free(c_max);
    free(pairs);
}

void generate_alpha_candidates(GraphContext* ctx, Edge* reconstructed_edges) {
    int n = ctx->node_count;

    TreeGraph g;
    g.edge_count = 0;
    g.edges = malloc(2 * n * sizeof(TreeEdge));
    g.next = malloc(2 * n * sizeof(int));
    g.head = malloc(n * sizeof(int));
    
    // Initialize the head array to -1 (signifying empy liked lists).
    for (int i = 0; i < n; i++) {
        g.head[i] = -1;
    }

    for (int i = 0; i < n; i++) {
        int u = reconstructed_edges[i].from;
        int v = reconstructed_edges[i].to;
        double weight = reconstructed_edges[i].penalized_cost;
        
        // Add bidirectional edges to the tree structure.
        add_tree_edge(&g, u, v, weight);
        add_tree_edge(&g, v, u, weight);
    }

    compute_all_candidate_sets(ctx, &g);

    free(g.edges);
    free(g.next);
    free(g.head);
}

#define MAX_LK_DEPTH 10

typedef struct {
    int t[2 * MAX_LK_DEPTH + 1];    // Array to store the node sequence.
    int current_depth;              // Currenr k-opt depth.
    bool* edge_status_changed;      // Track manipulated edges by a boolean flag.
} LKContext;

bool is_edge_disjoint(LKContext* lkctx, int u, int v, int current_k) {
    for (int i = 1; i < current_k; i++) {
        int b1 = lkctx->t[2 * i - 1];
        int b2 = lkctx->t[2 * i];

        if ((b1 == u && b2 == v) || (b1 == v && b2 == u)) {
            return false;
        }
    }

    for (int i = 1; i < current_k; i++) {
        int a1 = lkctx->t[2 * i];
        int a2 = lkctx->t[2 * i + 1];

        if ((a1 == u && a2 == v) || (a1 == v && a2 == u)) {
                return false;
        }
    }

    return true;
}

typedef struct {
    int k_idx;          // Position inside the candidate matrix row.
    int direction;      // Current tour direction (0 = successor, 1 = predecessor)
    double saved_gain;  // Cumulative gain before entering the tier.
} LKLoopState;

int get_tour_successor(GraphContext* ctx, int node) {
    int current_idx = ctx->pos[node];
    int next_idx = (current_idx + 1) % ctx->node_count;
    return ctx->tour[next_idx];
}

int get_tour_predecessor(GraphContext* ctx, int node) {
    int current_idx = ctx->pos[node];
    int prev_idx = (current_idx - 1 + ctx->node_count) % ctx->node_count;
    return ctx->tour[prev_idx];
}

typedef struct { int to_local; bool is_added; } VirtualEdge;
typedef struct { int node; int pos; } SortNode;

int compare_sort_nodes(const void *a, const void *b) {
    return ((SortNode*)a)->pos - ((SortNode*)b)->pos;
}

bool is_broken_edge(LKContext *lkctx, int k_depth, int u, int v) {
    for (int i = 1; i <= k_depth; i++) {
        int b1 = lkctx->t[2*i - 1]; int b2 = lkctx->t[2*i];
        if ((b1 == u && b2 == v) || (b1 == v && b2 == u)) return true;
    }
    return false;
}

int get_local_idx(SortNode *nodes, int num_nodes, int node_id) {
    for (int i = 0; i < num_nodes; i++) {
        if (nodes[i].node == node_id) return i;
    }
    return -1;
}

bool validate_tour_feasibility(GraphContext *ctx, LKContext *ws, int t_2k) {
    int k_depth = ws->current_depth;
    int num_nodes = 2 * k_depth;
    ws->t[2 * k_depth] = t_2k; // Temporarily pin the closure target


    SortNode nodes[20];
    for (int i = 1; i <= num_nodes; i++) {
        nodes[i-1].node = ws->t[i];
        nodes[i-1].pos = ctx->pos[ws->t[i]];
    }
    qsort(nodes, num_nodes, sizeof(SortNode), compare_sort_nodes);


    VirtualEdge v_graph[20][2];
    int v_count[20] = {0};


    // 1. Map Added Edges virtually
    int l_t1 = get_local_idx(nodes, num_nodes, ws->t[1]);
    int l_t2k = get_local_idx(nodes, num_nodes, ws->t[num_nodes]);
    v_graph[l_t1][v_count[l_t1]++] = (VirtualEdge){l_t2k, true};
    v_graph[l_t2k][v_count[l_t2k]++] = (VirtualEdge){l_t1, true};


    for (int i = 1; i < k_depth; i++) {
        int l_t2i = get_local_idx(nodes, num_nodes, ws->t[2*i]);
        int l_t2i_p1 = get_local_idx(nodes, num_nodes, ws->t[2*i + 1]);
        v_graph[l_t2i][v_count[l_t2i]++] = (VirtualEdge){l_t2i_p1, true};
        v_graph[l_t2i_p1][v_count[l_t2i_p1]++] = (VirtualEdge){l_t2i, true};
    }


    // 2. Map Unbroken/Intact Original Tour Segments virtually
    for (int i = 0; i < num_nodes; i++) {
        int u = nodes[i].node;
        int succ = get_tour_successor(ctx, u);
        if (!is_broken_edge(ws, k_depth, u, succ)) {
            int next_local = (i + 1) % num_nodes;
            v_graph[i][v_count[i]++] = (VirtualEdge){next_local, false};
            v_graph[next_local][v_count[next_local]++] = (VirtualEdge){i, false};
        }
    }


    // 3. Trace the micro-graph to verify it forms a single cycle
    bool visited_local[20] = {false};
    int curr_local = 0, prev_local = -1, count_visited = 0;
    do {
        visited_local[curr_local] = true;
        count_visited++;
        int next_local = (v_graph[curr_local][0].to_local != prev_local) 
                         ? v_graph[curr_local][0].to_local 
                         : v_graph[curr_local][1].to_local;
        prev_local = curr_local;
        curr_local = next_local;
    } while (curr_local != 0 && !visited_local[curr_local]);


    return (count_visited == num_nodes && curr_local == 0);
}


void execute_variable_lk_swap(GraphContext* ctx, LKContext* lkctx) {
    int k_depth = lkctx->current_depth;
    int num_nodes = 2 * k_depth;

    SortNode nodes[20];
    for (int i = 1; i <= num_nodes; i++) {
        nodes[i-1].node = lkctx->t[i];
        nodes[i-1].pos = ctx->pos[lkctx->t[i]];
    }

    qsort(nodes, num_nodes, sizeof(SortNode), compare_sort_nodes);

    VirtualEdge v_graph[20][2];
    int v_count[20] = {0};

    // Reconstruct the verified micro-graph
    int l_t1 = get_local_idx(nodes, num_nodes, lkctx->t[1]);
    int l_t2k = get_local_idx(nodes, num_nodes, lkctx->t[num_nodes]);
    v_graph[l_t1][v_count[l_t1]++] = (VirtualEdge){l_t2k, true};
    v_graph[l_t2k][v_count[l_t2k]++] = (VirtualEdge){l_t1, true};

    for (int i = 1; i < k_depth; i++) {
        int l_t2i = get_local_idx(nodes, num_nodes, lkctx->t[2*i]);
        int l_t2i_p1 = get_local_idx(nodes, num_nodes, lkctx->t[2*i + 1]);
        v_graph[l_t2i][v_count[l_t2i]++] = (VirtualEdge){l_t2i_p1, true};
        v_graph[l_t2i_p1][v_count[l_t2i_p1]++] = (VirtualEdge){l_t2i, true};
    }

    for (int i = 0; i < num_nodes; i++) {
        int u = nodes[i].node;
        int succ = get_tour_successor(ctx, u);
        if (!is_broken_edge(lkctx, k_depth, u, succ)) {
            int next_local = (i + 1) % num_nodes;
            v_graph[i][v_count[i]++] = (VirtualEdge){next_local, false};
            v_graph[next_local][v_count[next_local]++] = (VirtualEdge){i, false};
        }
    }

    // Allocate memory for the new tour configuration
    int *new_tour = malloc(ctx->node_count * sizeof(int));
    int filled = 0;
    int curr_local = l_t1;
    int curr_node = lkctx->t[1];
    int prev_local = -1;

    new_tour[filled++] = curr_node;

    // Linearly stitch the tour segments back together
    while (filled < ctx->node_count) {
        int edge_idx = (v_graph[curr_local][0].to_local == prev_local) ? 1 : 0;
        VirtualEdge edge = v_graph[curr_local][edge_idx];
        int next_local = edge.to_local;

        if (edge.is_added) {
            curr_node = nodes[next_local].node;
            if (filled < ctx->node_count) new_tour[filled++] = curr_node;
        } else {
            // Determine segment traversal direction
            if (next_local == (curr_local + 1) % num_nodes) {
                int walk = get_tour_successor(ctx, curr_node);
                int target = nodes[next_local].node;
                while (walk != target) {
                    if (filled < ctx->node_count) new_tour[filled++] = walk;
                    walk = get_tour_successor(ctx, walk);
                }
                if (filled < ctx->node_count) new_tour[filled++] = target;
                curr_node = target;
            } else {
                int walk = get_tour_predecessor(ctx, curr_node);
                int target = nodes[next_local].node;
                while (walk != target) {
                    if (filled < ctx->node_count) new_tour[filled++] = walk;
                    walk = get_tour_predecessor(ctx, walk);
                }
                if (filled < ctx->node_count) new_tour[filled++] = target;
                curr_node = target;
            }
        }
        prev_local = curr_local;
        curr_local = next_local;
    }

    // Commit changes back to Context and synchronize the pos inversion map
    for (int i = 0; i < ctx->node_count; i++) {
        ctx->tour[i] = new_tour[i];
        ctx->pos[new_tour[i]] = i;
    }

    free(new_tour);
}

double calculate_current_tour_cost(GraphContext* ctx) {
    double total_cost = 0.0;
    for (int i = 0; i < ctx->node_count; i++) {
        int from = ctx->tour[i];
        int to = ctx->tour[(i + 1) % ctx->node_count];
        total_cost += calculate_euclidean_distance(ctx, from, to);
    }

    return total_cost;
}

bool lk_search_step(GraphContext* ctx, LKContext* lkctx) {
    int max_candidates = ctx->max_candidates;
    
    // Alocating the tracking state frames
    LKLoopState loop_stack[MAX_LK_DEPTH];
    
    // Seed depth step with 1.
    int depth = 1;
    loop_stack[depth].k_idx = 0;
    loop_stack[depth].direction = 0;
    loop_stack[depth].saved_gain = 0;

    while (depth > 0) {
        int t_2i = lkctx->t[2 * depth];
        double cumulative_gain = loop_stack[depth].saved_gain;
        bool step_deepened = false;

        for (; loop_stack[depth].k_idx < max_candidates; loop_stack[depth].k_idx++) {
            int t_2i_plus_1 = GET_CANDIDATE(ctx, t_2i, loop_stack[depth].k_idx);

            // Prevent tracking back along immediate links or bearking disjointness.
            if (t_2i_plus_1 == lkctx->t[2 * depth - 1] || !is_edge_disjoint(lkctx, t_2i, t_2i_plus_1, depth)) {
                continue;
            }

            double added_edge_cost = calculate_euclidean_distance(ctx, t_2i, t_2i_plus_1);
            double current_beark_cost = calculate_euclidean_distance(ctx, lkctx->t[2 * depth - 1], t_2i);
            double step_gain = current_beark_cost - added_edge_cost;

            // Check Gain Criterion
            if (cumulative_gain + step_gain <= 1e-6) {
                continue;
            }

            lkctx->t[2 * depth + 1] = t_2i_plus_1;

            // Core Evaluation Branch: Find or resume finding t_2i_plus_1
            for (; loop_stack[depth].direction < 2; loop_stack[depth].direction++) {
                int t_2i_plus_2 = (loop_stack[depth].direction == 0)
                    ? get_tour_successor(ctx, t_2i_plus_1)
                    : get_tour_predecessor(ctx, t_2i_plus_1);

                if (!is_edge_disjoint(lkctx, t_2i_plus_1, t_2i_plus_2, depth)) {
                    continue;
                }

                double closure_gain = calculate_euclidean_distance(ctx, t_2i_plus_1, t_2i_plus_2) -
                                      calculate_euclidean_distance(ctx, t_2i_plus_2, lkctx->t[1]);

                // Success Condition: Valid closure found.
                double total_gain = cumulative_gain + step_gain + closure_gain;
                if (total_gain > 1e-6) {
                    lkctx->current_depth = depth + 1;
                    if (validate_tour_feasibility(ctx, lkctx, t_2i_plus_2)) {
                        lkctx->t[2 * depth + 2] = t_2i_plus_2;
                        
                        execute_variable_lk_swap(ctx, lkctx);
                        return true;
                    }
                }

                //Step Forword, check if we can deepen the search tier safely.
                if (depth + 1 < MAX_LK_DEPTH) {
                    lkctx->t[2 * depth + 2] = t_2i_plus_2;

                    // Initialize the next execution frame.
                    int next_depth = depth + 1;
                    loop_stack[next_depth].k_idx = 0;
                    loop_stack[next_depth].direction = 0;
                    loop_stack[next_depth].saved_gain = cumulative_gain + step_gain;

                    // Advenced state incerment to ensure that when we eventually
                    // backtrack to this frame, we try the ALTERNATE choice next.
                    loop_stack[depth].direction++;

                    depth = next_depth;
                    step_deepened = true;
                    break;
                }
            }

            if (step_deepened) break;
            
            // Reset the inner direction loop for the next candidate row change.
            loop_stack[depth].direction = 0;
        }

        if (step_deepened) continue;
        
        // Backtrack Branch: if both loops are spent at this depth level, drop down
        depth--;
        if (depth > 0) {
            // Check if the frame we dropped into had completed its direction options.
            if (loop_stack[depth].direction >= 2) {
                loop_stack[depth].direction = 0;
                loop_stack[depth].k_idx++; // Push to next elite candidate target.
            }
        }
    }

    return false; // No profitable k-opt configurations found from this base root.
}

void run_lk_engine(GraphContext* ctx) {
    bool improvement_found = true;
    LKContext lkctx;
    lkctx.edge_status_changed = calloc(ctx->node_count, sizeof(bool));

    int epoch = 0;
    double lower_bound = ctx->best_lower_bound;
    double current_cost = calculate_current_tour_cost(ctx);
    double initial_optimality_gap = ((current_cost - lower_bound) / lower_bound) * 100.0;
    printf("[LK Engine] Initializing Engine... Baseline Tour Cost: %.4lf | Held-Karp Lower Bound: %.4lf\n",
            current_cost, lower_bound);
    printf("[LK Engine] Starting Optimality Gap: %.4lf%%\n\n", initial_optimality_gap);

    while (improvement_found) {
        improvement_found = false;
        epoch++;

        if (epoch % 10 == 0) {
            current_cost = calculate_current_tour_cost(ctx);
            double epoch_optimality_gap = ((current_cost - lower_bound) / lower_bound) * 100.0;
            printf("    [LK Engine] Epoch %d, Scanning Node Layout. Cost: %.4lf | Current Optimality Gap: %.4lf%%\n",
                    epoch, current_cost, epoch_optimality_gap);
            double gap_from_actual_solution = ((current_cost - BEST_TOUR_SOLUTION) / BEST_TOUR_SOLUTION) * 100.0;
            printf("    [LK Engine] Gap From The Actual Solution: %.4lf%%\n",
                    gap_from_actual_solution);
        }

        for (int n_idx = 0; n_idx < ctx->node_count; n_idx++) {
            // Set the base anchor t1 to the currentndoe in the tour layout.
            lkctx.t[1] = ctx->tour[n_idx];
            lkctx.current_depth = 1;

            // Strategy A: Try successor first, Choose initial broken edge target t2.
            lkctx.t[2] = get_tour_successor(ctx, lkctx.t[1]);
            // Wipe the midification history clean before starting a new search tree.
            memset(lkctx.edge_status_changed, 0, ctx->node_count * sizeof(bool));

            if (lk_search_step(ctx, &lkctx)) {
                improvement_found = true;
                break; // Tour layout updated, restart outer scanning loop.
            }

            // Strategy B: Try bearking the backword edge, predecessor as t2 if successor yielded no improvements.
            lkctx.t[2] = get_tour_predecessor(ctx, lkctx.t[1]);

            memset(lkctx.edge_status_changed, 0, ctx->node_count * sizeof(bool));

            if (lk_search_step(ctx, &lkctx)) {
                improvement_found = true;
                break;
            }
        }
    }

    current_cost = calculate_current_tour_cost(ctx);
    double final_optimality_gap = ((current_cost - lower_bound) / lower_bound) * 100.0;
    double gap_from_actual_solution = ((current_cost - BEST_TOUR_SOLUTION) / BEST_TOUR_SOLUTION) * 100.0;
    printf("-----------------------------------------------------\n");
    printf("[Execution Summary]\n");
    printf("    [LK Engine] Local Optima Achieved. Execution converged after %d epochs.\n", epoch);
    printf("    [LK Engine] Held-Karp Lower Bound: %lf\n", ctx->best_lower_bound);
    printf("    [LK Engine] Final LK Optimaized Tour Cost: %lf\n", current_cost);
    printf("    [LK Engine] Final Optimality Gap Remaining: %lf%%\n", final_optimality_gap);
    printf("    [LK Engine] Gap From The Actual Solution: %.4lf%%\n",
            gap_from_actual_solution);
    printf("-----------------------------------------------------\n");

    free(lkctx.edge_status_changed);
}

void generate_nearest_neighbor_tour(GraphContext* ctx, int start_node) {
    int hit_elite_count = 0;
    int fallback_count = 0;

    // Allocate a flat bitmask tracking array for visited nodes
    bool *visited = calloc(ctx->node_count, sizeof(bool));
    if (!visited) {
        fprintf(stderr, "Error: Memory allocation failed for NNS tracking.\n");
        return;
    }

    // Seed the first node of the tour
    ctx->tour[0] = start_node;
    ctx->pos[start_node] = 0;
    visited[start_node] = true;

    int current_node = start_node;
    int m = ctx->max_candidates;

    // Main construction loop: Find the next closest city for each position i
    for (int i = 1; i < ctx->node_count; i++) {
        int next_node = -1;
        double min_dist = DBL_MAX;

        // OPTIMIZATION PHASE A: Check the elite candidate matrix first O(1).
        if (ctx->candidates != NULL) {
            for (int k = 0; k < m; k++) {
                int candidate = ctx->candidates[current_node * m + k];
                if (!visited[candidate]) {
                    double dist = calculate_euclidean_distance(ctx, current_node, candidate);
                    if (dist < min_dist) {
                        min_dist = dist;
                        next_node = candidate;
                    }
                }
            }
        }

        // OPTIMIZATION PHASE B: Fallback scan if elite candidates are spent O(N).
        if (next_node == -1) {
            // For tracking progress.
            fallback_count++;

            for (int j = 0; j < ctx->node_count; j++) {
                if (!visited[j]) {
                    double dist = calculate_euclidean_distance(ctx, current_node, j);
                    if (dist < min_dist) {
                        min_dist = dist;
                        next_node = j;
                    }
                }
            }
        } else hit_elite_count++; // For tracking progress.

        // Commit the chosen node to the tour layout structures
        ctx->tour[i] = next_node;
        ctx->pos[next_node] = i; // Perfect dynamic alignment of our O(1) inversion map
        visited[next_node] = true;
        
        current_node = next_node;

        if (i % 10 == 0) { 
            printf("[Nearest Neighbor Initial Tour] Alpha hits: %d, Global Scanning Fallbacks: %d\n",
                    hit_elite_count, fallback_count);
        }
    }

    free(visited);
}

int main()
{
    GraphContext ctx = {};

    load_cities_from_file(&ctx, "./syria40.tsp");
    compute_edges(&ctx);

    ctx.upper_bound = estimate_target_bound(&ctx);

    optimaize_held_karp_relaxation(&ctx);

    allocate_candidate_matrix(&ctx);
    
    Edge* optimal_1_tree = malloc(ctx.node_count * sizeof(Edge));

    double penalized_lower_bound = reconstruct_optimal_1tree(&ctx, optimal_1_tree);
    (void) penalized_lower_bound;

    generate_alpha_candidates(&ctx, optimal_1_tree);

    generate_nearest_neighbor_tour(&ctx, 0);

    run_lk_engine(&ctx);

    print_named_tour(&ctx);

    return 0;
}
