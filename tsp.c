#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

typedef struct Node Node;

typedef struct {
    Node* ref;
    double alpha;
    double distance;
} Candidate;

struct Node {
    int id;
    double x;
    double y;
    // Current Lagrange-Multiplier; node penalty (pi).
    double pi;
    // Degree of the node in te current 1-Tree.
    int degree;
    // G[i] = 2 - degree
    int subgradint;
    // Store candidates to this city.
    Candidate* candidates;
    int candidates_count;
};

typedef struct {
    int from;
    int to;
    double distance;
    double penalized_cost;
} Edge;

typedef struct {
    int from;
    int to;
} OneTreeEdge;

typedef struct {
    Node* nodes;
    int node_count;

    Edge* edges;
    int edge_count;

    OneTreeEdge* one_tree_edges;
    int one_tree_edge_count;

    double upper_bound;
    double best_lower_bound;
} GraphContext;

GraphContext graph_context_init() {
    GraphContext ctx;

    ctx.nodes = NULL;
    ctx.node_count = 0;
    ctx.edges = NULL;
    ctx.edge_count = 0;
    ctx.upper_bound = 0.0;
    ctx.best_lower_bound = 0.0;

    return ctx;
}

typedef struct {
    Node* cities;
    int count;
} Cities;

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
                ctx->nodes[index].pi = 0.0;
                ctx->nodes[index].degree = 0;
                ctx->nodes[index].subgradint = 0;
                ctx->nodes[index].candidates_count = 0;
                ctx->nodes[index].candidates = NULL;
            
                nodes_read++;
            }
        }
    }
    fclose(file);
    printf("[Parser] Successfully loaded data file!\n");
}

double _calculate_distance(double x1, double x2, double y1, double y2) {
    return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
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
                double dist = _calculate_distance(cn->x, tn->x, cn->y, tn->y);

                ctx->edges[edge_idx].from = cn->id;
                ctx->edges[edge_idx].to = tn->id;
                ctx->edges[edge_idx].distance = dist;
                ctx->edges[edge_idx].penalized_cost = dist;
                edge_idx++;
            }
        }
    }
}

void update_edges_penalized_cost(GraphContext* ctx) {
    for (int i = 0; i < ctx->edge_count; i++) {
        double pi1 = ctx->edges[i].penalized_cost;
        double pi2 = ctx->edges[i].penalized_cost;
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

// Edges extract_minimum_spaning_tree(Edges edges, int num_cities, int spacial_node) {
//     Edges mst_tree = edges_create(num_cities);
//     int mst_edge_count = 0;
//     DisjointSet ds = disjointset_create(num_cities);
//     int target_edges = num_cities - 2;
//
//     for (int i = 0; i < edges.count; i++) {
//         Edge curr_edge = edges.edges[i];
//
//         if (curr_edge.from->id == spacial_node || curr_edge.to->id == spacial_node) continue;
//
//         if (union_cities(ds, curr_edge.from->id, curr_edge.to->id)) {
//             mst_tree.edges[mst_edge_count] = curr_edge;
//             mst_edge_count++;
//
//             // break once we reach our target edges.
//             if (mst_edge_count == target_edges) break;
//         }
//     }
//
//     mst_tree.count = mst_edge_count;
//     disjointset_free(ds);
//     return mst_tree;
// }
//
// Edges extract_1_tree(Edges edges, int num_cities, int spacial_node) {
//     Edges mst_tree = extract_minimum_spaning_tree(edges, num_cities, spacial_node);
//
//     // Find the 2 cheapest edges connectd to the spacial_node.
//     int connected_count = num_cities - 2;
//     for (int i = 0; i < edges.count; i++) {
//         if (connected_count == num_cities) break;
//
//         Edge curr_edge = edges.edges[i];
//         Node* fromcity = curr_edge.from;
//         Node* tocity = curr_edge.to;
//
//         if (fromcity->id == spacial_node || tocity->id == spacial_node) {
//             mst_tree.edges[connected_count] = curr_edge;
//             connected_count++;
//         }
//     }
//
//     mst_tree.count += 2;
//     return mst_tree;
// }

double sum_penalized_values(GraphContext* ctx) {
    double total_penalized_values = 0.0;

    for (int i = 0; i < ctx->node_count; i++) {
        total_penalized_values += ctx->nodes[i].pi;
    }

    return total_penalized_values;
}

typedef struct {
    double true_lower_bound_weight;
    double penalized_cost_weight;
} OneTreeWeights;

OneTreeWeights extract_1_tree(GraphContext* ctx, int spacial_node) {
    int mst_edge_count = 0;
    DisjointSet ds = disjointset_create(ctx->node_count);
    int target_edges = ctx->node_count - 2;

    double total_penalized_cost_weight = 0.0;

    int epacial_node_connected_count = 0;
    for (int i = 0; i < ctx->edge_count; i++) {
        if (ctx->edges[i].from == spacial_node || ctx->edges[i].to == spacial_node) {
            if (epacial_node_connected_count < 2) {
                total_penalized_cost_weight += ctx->edges[i].penalized_cost;
                ctx->nodes[i].degree++;
                ctx->nodes[i].degree++;
                epacial_node_connected_count++;
            } else {
                continue;
            }
        }

        if (union_cities(ds, ctx->edges[i].from, ctx->edges[i].to)) {
            total_penalized_cost_weight += ctx->edges[i].penalized_cost;
            ctx->nodes[ctx->edges[i].from].degree++;
            ctx->nodes[ctx->edges[i].to].degree++;

            // break once we reach our target edges.
            if (mst_edge_count == target_edges) break;
        }
    }

    disjointset_free(ds);
    
    double total_true_lower_bound_weight = total_penalized_cost_weight - 2 * sum_penalized_values(ctx);

    return (OneTreeWeights) { .true_lower_bound_weight = total_true_lower_bound_weight, .penalized_cost_weight = total_penalized_cost_weight };
}

// double calculate_dynamic_step_size(Degrees d, double currect_weight, double target_bound, double lambda) {
//     double denominator = 0.0;
//     //calculate the sum of squares.
//     for (int i = 0; i < d.count; i++) {
//         double diff = (double) d.degrees[i] - 2.0;
//         denominator += diff * diff;
//     }
//
//     // edge case: if denominator it 0, we found a perfect TSP tour.
//     // return 0 to stop shifting penaltes.
//     if (denominator == 0) {
//         printf("[DynamicStepSize] Found Perfict TSP Tour!\n");
//         return 0.0;
//     }
//
//     if (currect_weight >= target_bound) {
//         target_bound = currect_weight + 5.0;
//
//     }
//
//     // Dynaimc Step Size Formula: lambda * (Target - Current) / Denominator.
//     double step_size = lambda * (target_bound - currect_weight) / denominator;
//     // safety guard: step size should not fall below 0.
//     if (step_size < 0.0) return 0.0;
//     return step_size;
// }
//
// double estimate_target_bound(Cities c) {
//     int* visited = (int*) calloc(c.count, sizeof(int));
//     double total_tour_distance = 0.0;
//     int current_city_idx = 0;
//     visited[current_city_idx] = 1;
//
//     for (int i = 0; i < c.count - 1; i++) {
//         int nearest_city_idx = -1;
//         double shortest_distance = 1e9;
//
//         for (int j = 0; j < c.count; j++) {
//             if (visited[j] == 1) continue;
//
//             double dist = _calculate_distance(&c.cities[current_city_idx], &c.cities[i]);
//             if (dist < shortest_distance) {
//                 shortest_distance = dist;
//                 nearest_city_idx = i;
//             }
//         }
//
//         // Move to the nearest neighbor.
//         total_tour_distance += shortest_distance;
//         current_city_idx = nearest_city_idx;
//         visited[current_city_idx] = 1;
//     }
//
//     double return_distance = _calculate_distance(&c.cities[current_city_idx], &c.cities[0]);
//     total_tour_distance += return_distance;
//
//     free(visited);
//     return total_tour_distance;
// }
//
// // Valid tsp tour all it cities has exactly 2 edges.
// bool is_valid_tsp_tour(Degrees d) {
//     bool is_true_tour = true;
//
//     for (int i = 0; i < d.count; i++) {
//         if (d.degrees[i] != 2) {
//             is_true_tour = false;
//             break;
//         }
//     }
//
//     return is_true_tour;
// }
//
// typedef struct {
//     Edges one_tree;
//     double best_lower_bound;
// } HeldKarpOneTree;

HeldKarpOneTree optimaize_held_karp_relaxation(Cities c, Edges e, int period, double epsilon) {
    int spacial_node = 0;
    double target_bound = estimate_target_bound(c);
    // Variables to track if the bound is improving.
    int steps_without_improvement = 0;
    // Allocate an array to save the absolute best 1-tree we found.
    Degrees d = degrees_create(c.count);
    double* best_pis = (double*) calloc(c.count, sizeof(double));

    printf("[Held-Karp] Begin Held-Karp Optimaizetion with Settings:\n");
    printf("    Number of cities: %d\n", c.count);
    printf("    Number of edges: %d\n", e.count);
    printf("    Spacial Node (Node): %d\n", spacial_node);
    printf("    Epsilon = %lf\n", epsilon);
    printf("    Estimated Target Bound = %lf\n", target_bound);

    Edges best_one_tree = edges_create(c.count);
    double best_lower_bound = -1e9; 

    while (epsilon > 0.000001) {
        for (int i = 0; i < period; i++) {}

    }
    for (int i = 0; i < opts.max_iterations; i++) {
        // Set every city degree to zero.
        degrees_reset(d);

        // generate & sort edges, then extract the 1-tree.
        edges_sort(e);
        edges_cost_update(e);
        Edges one_tree = extract_1_tree(e, c.count, opts.spacial_node);

        // Count the edges for every node (Node)
        OneTreeWeights one_tree_weights = calculate_1_tree_wights(one_tree);

        // and the current weights of our 1-tree.
        count_1_tree_degrees(one_tree, d);

        bool has_reset = false;

        // Track Held-Karp improvement with the true geometric weights.
        if (one_tree_weights.geometric_wight > best_lower_bound) {
            best_lower_bound = one_tree_weights.geometric_wight;
            steps_without_improvement = 0;

            // Save the best tree
            memcpy(best_one_tree.edges, one_tree.edges, best_one_tree.count * sizeof(Edge));

            // Save best penalty values on improvement.
            for (int j = 0; j < c.count; j++) best_pis[j] = c.cities[j].pi;
        } else steps_without_improvement++;

        // Helsgaun's rule: if no improvement happend for a period, reduce lambda.
        if (steps_without_improvement >= opts.period) {
            opts.epsilon *= 0.9;
            printf("[Held-Karp] Stagnation Hit!, backtracking and injecting jitter, New Lambda: %lf\n", opts.epsilon);

            for (int j = 0; j < c.count; j++) {
                double jitter = (((double) rand() / RAND_MAX) * 0.1) - 0.5;
                c.cities[j].pi = best_pis[j] + jitter;
                if (c.cities[j].pi < 0.0) c.cities[j].pi = 0.0;
            }

            steps_without_improvement = 0;
            has_reset = true;
        }

        double dynamic_step_size = calculate_dynamic_step_size(d, one_tree_weights.costs_wight, opts.target_bound, opts.epsilon);

        if (i % 20 == 0) {
            printf("Iter %d | True Geometric Weight: %10lf | Penalized Weight: %10lf | Lambda: %lf | DSS: %lf\n", 
                    i, one_tree_weights.geometric_wight, one_tree_weights.costs_wight, opts.epsilon, dynamic_step_size);
        }

        if (dynamic_step_size < 1e-6) {
            bool is_true_tour = is_valid_tsp_tour(d);

            if (is_true_tour) {
                printf("[Held-Karp] SUCCESS: Perfict 1-Tree Reached at iteration %d\n", i);
            } else {
                printf("[Held-Karp] HALT: Subgradint stagnation. Step size fell below threshold.\n");
            }
            break;
        }

        if (!has_reset) {
            // Apply the formula using the dynamic step size calculated.
            for (int j = 0; j < c.count; j++) {
                c.cities[j].pi += dynamic_step_size * (d.degrees[j] - 2);
                // Prevent negative pi and cost.
                // if (c.cities[j].pi < 0.0) c.cities[j].pi = 0.0;
            }
        }

        edges_free(one_tree);
    }

    free(best_pis);
    free(d.degrees);
    return (HeldKarpOneTree) { .one_tree = best_one_tree, .best_lower_bound = best_lower_bound };
}
//
// void solve_held_karp_ascent() {
//
// }
//
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
// void visualize_city_candidates(Node* cities, int num_cities, int target_city_id) {
//     int target_city_idx = -1;
//     for (int i = 0; i < num_cities; i++) {
//         if (cities[i].id == target_city_id) {
//             target_city_idx = i;
//             break;
//         }
//     }
//
//     if (target_city_idx == -1) {
//         printf("[Candidates] Visualization Error: Node ID %d is not found.\n", target_city_idx);
//         return;
//     }
//
//     Node c = cities[target_city_idx];
//     printf("\n-----------------------------------------------------------\n");
//     printf("    Candidate Set Profile for Node ID: %d (Alpha: %lf)", c.id, c.pi);
//     printf("\n-----------------------------------------------------------\n");
//     printf(" Rank | Neighbor ID | Alpha-nearness | Physical Distance");
//     printf("\n-----------------------------------------------------------\n");
//
//     for (int k = 0; k < c.candidates_count; k++) {
//         Candidate cand = c.candidates[k];
//
//         if (cand.alpha < 1e-6) {
//             printf(" [%d] | Node %-9d | %-14.4lf | %-17.2lf (1-Tree Edge)\n", k+1, cand.to_id, cand.alpha, cand.distance);
//         } else {
//             printf(" [%d] | Node %-9d | %-14.4lf | %-17.2lf\n", k+1, cand.to_id, cand.alpha, cand.distance);
//         }
//     }
//     printf("\n-----------------------------------------------------------\n");
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
//
//
// // void print_candidate_matix(Candidate** cm, int rows, int columns) {
// //     for (int i = 0; i < rows; i++) {
// //         printf("%d |", i);
// //         for (int j = 0; j < columns; j++) {
// //             printf(" (%d - %.2f) ", cm[i][j].target_city, cm[i][j].distance);
// //         }
// //         printf("|\n");
// //     }
// // }
//
// bool are_cities_consecutive(int u, int v, int* tour, int num_cities) {
//     for (int i = 0; i < num_cities; i++) {
//         int next = tour[(i + 1) % num_cities];
//         if ((tour[i] == u && next == v) || (tour[i] == v && next == u)) {
//             return true;
//         }
//     }
//
//     return false;
// }
//
// void dynamic_k_opt_step(
//     int current_city,
//     int* tour,
//     int num_cities,
//     Node* cities,
//     Candidate** candidate_matrix,
//     double current_gain, int k
// )
// {
//     printf("-> Evaluating at level k = %d | Current Gain = %.2f\n", k, current_gain);
//
//     for (int i = 0; i < MAX_CANDIDATES; i++)
//     {
//         int next_candidate = candidate_matrix[current_city][i].target_city;
//         double added_edge_cost = candidate_matrix[current_city][i].distance;
//
//         if (are_cities_consecutive(current_city, next_candidate, tour, num_cities)) 
//         {
//             continue;
//         }
//
//         double removed_edge_cost = 150.0;
//         double local_gain = removed_edge_cost - added_edge_cost;
//         double next_cumlative_gain = current_city + local_gain;
//
//         if (next_cumlative_gain > 0)
//         {
//             printf("    [SIGNAL: INCREASE K] Gain is positive (%.2f). Lifting from k=%d to k=%d\n",
//                     next_cumlative_gain, k, k + 1);
//             dynamic_k_opt_step(next_candidate, tour, num_cities, cities, candidate_matrix, next_cumlative_gain, k + 1);
//             return;
//         }
//         else 
//         {
//             printf("    [SIGNAL: HALT] Next gain would be %.2f (<= 0). Tree pruned at k=%d\n",
//                     next_cumlative_gain, k);
//         }
//     }
// }

int main()
{
    GraphContext ctx = graph_context_init();

    load_cities_from_file(&ctx, "./att48.tsp");
    compute_edges(&ctx);
    qsort_edges(&ctx);

    int period = 10 / 2 > 100 ? 10 / 2 : 100;
    (void) period;

    OneTreeWeights otws = extract_1_tree(&ctx, 0);
    (void) otws;

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
