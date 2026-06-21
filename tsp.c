#include <stdio.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>


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
    
    int* upper_bound_tour;
    double upper_bound;
    double epsilon;

    double curr_lower_bound;
    double best_lower_bound;
} GraphContext;

void init_graph_ctx(GraphContext* ctx, int n) {
    ctx->edge_count = ctx->node_count * (ctx->node_count - 1) / 2;
    ctx->edges = malloc(ctx->edge_count * sizeof(Edge));
    ctx->pis = (double*) calloc(n, sizeof(double));
    ctx->best_pis = (double*) calloc(n, sizeof(double));
    ctx->degrees = (int*) malloc(n * sizeof(double));
    ctx->upper_bound_tour = (int*) malloc(n * sizeof(int));
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
    printf("[Parser] Successfully loaded data file!\n");
}


int calculate_euclidean_distance(GraphContext* ctx, int from, int to) {
    double xd = ctx->nodes[from].x - ctx->nodes[to].x;
    double yd = ctx->nodes[from].y - ctx->nodes[to].y;

    double distance = sqrt(pow(xd, 2) + pow(yd, 2));

    // TSPLIB specifies standard rounding to the nearest integer.
    return (int) (distance + 0.5);
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
    int epacial_node_connected_count = 0;

    for (int i = 0; i < ctx->node_count; i++) {
        ctx->degrees[i] = 0;
    }

    for (int i = 0; i < ctx->edge_count; i++) {
        int fromid = ctx->edges[i].from;
        int toid = ctx->edges[i].to;

        if (fromid == spacial_node || toid == spacial_node) {
            if (epacial_node_connected_count < 2) {
                one_tree_penalized_weight += ctx->edges[i].penalized_cost;
                ctx->degrees[fromid]++;
                ctx->degrees[toid]++;
                epacial_node_connected_count++;
            }
            continue;
        } 

        if (union_cities(ds, fromid, toid)) {
            one_tree_penalized_weight += ctx->edges[i].penalized_cost;
            ctx->degrees[fromid]++;
            ctx->degrees[toid]++;
            mst_edge_count++;

            // break once we reach our target edges.
            if (mst_edge_count == ctx->node_count - 2) break;
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
                ctx->best_lower_bound = ctx->curr_lower_bound;
                save_best_pi_values(ctx);
                improvement_found = true;
                printf("Held-Karp improvement_found.\n");
            }

            double step_size = calculate_step_size(ctx);
            printf("Held-Karp step_size = %lf\n", step_size);
            if (step_size == 0.0) {
                status = HK_NATURAL_TOUR_FOUND;
                save_best_pi_values(ctx);
                goto exit;
            }

            // Update transient pi penalties using the subgradient (G_i = d_i - 2).
            for (int i = 0; i < ctx->node_count; i++) {
                int nodeid = ctx->nodes[i].id;
                double gradient = ctx->degrees[nodeid] - 2;
                ctx->pis[i] += step_size * gradient;
            }

            for (int i = 0; i < ctx->node_count; i++) {
                // printf("pi(%d) = %lf ", i, ctx->pis[i]);
            
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


// typedef struct {
//     int neighbors[NUM_CITIES];
//     int costs[NUM_CITIES];
//     int count;
// } AdjacencyList;
//
// // Helper function to preform Depth-First Search (DFS) traversal across the 1-tree branches.
// void find_max_path_edges(int current_node, int parent_node, double current_max, int root_node, AdjacencyList* adj, double** max_weight) {
//     // Store the largest edge found on the path from "root_node" to "current_node".
//     max_weight[root_node][current_node] = current_max;
//
//     // Traverse all structural neighbors in out 1-tree.
//     for (int i = 0; i < adj[current_node].count; i++) {
//         int neighbor = adj[current_node].neighbors[i];
//         double edge_cost = adj[current_node].costs[i];
//
//         if (neighbor != parent_node) {
//             double next_max = (edge_cost > current_max) ? edge_cost : current_max;
//             find_max_path_edges(neighbor, current_node, next_max, root_node, adj, max_weight);
//         }
//     }
// }
//
// double** compute_alpha_nearness(Node* cities, Edge* optimaized_one_tree, int num_cities, int num_edges) {
//     AdjacencyList* adj = calloc(num_cities, sizeof(AdjacencyList));
//
//     for (int i = 0; i < num_cities; i++) {
//         int u = optimaized_one_tree[i].from->id;
//         int v = optimaized_one_tree[i].from->id;
//         double cost = optimaized_one_tree[i].distance;
//
//         adj[u].neighbors[adj[u].count] = v;
//         adj[u].costs[adj[u].count++] = cost;
//
//         adj[v].neighbors[adj[v].count] = u;
//         adj[v].costs[adj[v].count++] = cost;
//     }
//
//     // Allocate 2D max_weight matrix lookup table.
//     double** max_weight = (double**) malloc(num_cities * sizeof(double*));
//     for (int i = 0; i < num_cities; i++) {
//         max_weight[i] = (double*) malloc(num_cities * sizeof(double));    
//     }
//
//     // compute max edge on path for all city pairs by running DFS from every city.
//     for (int i = 0; i < num_cities; i++) {
//         find_max_path_edges(i, -1, 0.0, i, (AdjacencyList*) adj, max_weight);   
//     }
//
//     // Allocate and populate our finall 2D Alpha matrix.
//     double** alpha_matrix = (double**) malloc(num_cities * sizeof(double*));
//     for (int i = 0; i < num_cities; i++) {
//         alpha_matrix[i] = (double*) malloc(num_cities * sizeof(double));
//     }
//
//     for (int i = 0; i < num_cities; i++) {
//         for (int j = 0; j < num_cities; j++) {
//             if (i == j) {
//                 alpha_matrix[i][j] = 0.0;
//                 continue;
//             }
//
//             // get abse distance and add final node penalties (alpha-nearness values).
//             double base_dist = calculate_distance(cities[i], cities[j]);
//             double adjusted_cost = base_dist + cities[i].pi + cities[j].pi;
//
//             // Alpha = AdjustedCost - MaxEdgeOnPath
//             alpha_matrix[i][j] = adjusted_cost - max_weight[i][j];
//
//             if (alpha_matrix[i][j] < 1e-6) alpha_matrix[i][j] = 0.0;
//         }
//     }
//
//     for (int i = 0; i < num_cities; i++) free(max_weight[i]);
//     free(max_weight);
//     free(adj);
//
//     return alpha_matrix;
// }
//
// int compare_candidates(const void* a, const void* b) {
//     Candidate* cand_a = (Candidate*) a;
//     Candidate* cand_b = (Candidate*) b;
//
//     if (cand_a->alpha < cand_b->alpha) return -1;
//     if (cand_a->alpha > cand_b->alpha) return 1;
//
//     // if alpha values are equal, prefer the physically shrter edge.
//     if (cand_a->distance < cand_b->distance) return -1;
//     if (cand_a->distance > cand_b->distance) return 1;
//
//     return 0;
// }
//
// void build_candidate_sets(Node* cities, int num_cities, double** alpha_matrix, int max_candidates) {
//     // temp array to hold all possible neighbors for sorting.
//     Candidate* temp_pool = (Candidate*) malloc((num_cities - 1) * sizeof(Candidate));
//
//     for (int i = 0; i < num_cities; i++) {
//         int pool_idx = 0;
//
//         for (int j = 0; j < num_cities; j++) {
//             if (i == j) continue;
//
//             temp_pool[pool_idx].to_id = cities[j].id;
//             temp_pool[pool_idx].alpha = alpha_matrix[i][j];
//             temp_pool[pool_idx].distance = calculate_distance(cities[i], cities[j]);
//             pool_idx++;
//         }
//
//         qsort(temp_pool, num_cities - 1, sizeof(Candidate), compare_candidates);
//
//         int actual_keep_count = (num_cities - 1 < max_candidates) ? (num_cities - 1) : max_candidates;
//
//         cities[i].candidates = (Candidate*) malloc(actual_keep_count * sizeof(Candidate));
//         cities[i].candidates_count = actual_keep_count;
//
//         for (int k = 0; k < actual_keep_count; k++) {
//             cities[i].candidates[k] = temp_pool[k];
//             // visualize_city_candidates(cities, num_cities, i);
//         }
//     }
//
//     free(temp_pool);
//     printf("[Candidate Sets] Successfully generated top-%d candidates edges for all %d cities.\n", max_candidates, num_cities);
// }

int main()
{
    GraphContext ctx = {};

    load_cities_from_file(&ctx, "./kroA100.tsp");
    compute_edges(&ctx);

    ctx.upper_bound = estimate_target_bound(&ctx);

    optimaize_held_karp_relaxation(&ctx);

    printf("UP = %lf, LB = %lf\n", ctx.upper_bound, ctx.best_lower_bound);

    // HeldKarpOptions hko = {
    //     .max_iterations = 2000,
    //     .epsilon = 0.5,
    //     .period = 20,
    //     .target_bound = estimate_target_bound(c),  
    //     .spacial_node = 0  
    // }; 
    //
    // HeldKarpOneTree ot = optimaize_held_karp(c, e, hko);
    // for (int i = 0; i < ot.one_tree.count; i++) {
    //     Edge ce = ot.one_tree.edges[i];
    //     printf("%4d: HeldKarpEdge(%d, %d, %lf, %lf)\n", i, ce.from->id, ce.to->id, ce.distance, ce.cost);
    // }

    // Edge* one_tree = optimaize_held_karp(cities, num_cities, num_edges, 0);
    //
    // double** alpha_matrix = compute_alpha_nearness(cities, one_tree, num_cities, num_edges);
    //
    // build_candidate_sets(cities, num_cities, alpha_matrix, MAX_CANDIDATES);
    
    return 0;
    
}
