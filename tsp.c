/*
 * tsp.c - Custom TSP Solver using LKH algorithm, powered by
 * Lin-Kernighan heuristic search and Held-Karp relaxation.
 *
 * This Code is licnesed under the MIT licnese, see LICENSE.
 */

#include <stdio.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>


#define KROA100_OPTIMAL 21282.0
#define SY40_OPTIMAL 2414824 

// Change for the corresponding input file.
#define BEST_TOUR_SOLUTION KROA100_OPTIMAL

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
    double* pis;
    double* best_pis;
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
    ctx->tour = malloc(ctx->node_count * sizeof(int));
    ctx->pos = malloc(ctx->node_count * sizeof(int));
    allocate_candidate_matrix(ctx);
}

void print_named_tour(GraphContext* ctx) {
    if ((BEST_TOUR_SOLUTION == SY40_OPTIMAL)) {
        printf("\nOptimized Tour Sequence:\n");
        for (int i = 0; i < ctx->node_count; i++) {
            int node_id = ctx->tour[i] + 1;
            printf(" %s (%d) -> ", CYRIAN_CITIES_40[node_id], node_id);
            if ((i + 1) % 5 == 0) printf("\n");
        }
        printf("\n");
    }
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
                reading_cooredinates = true;
                continue;
            }
        } else {
            if (strstr(line, "EOF") || nodes_read >= num_nodes) break;

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
    printf("[Init] Successfully parsed & loaded data file. Nodes: %d, Edges: %d\n", ctx->node_count, ctx->edge_count);
}

int calculate_euclidean_distance(GraphContext* ctx, int from, int to) {
    long long dx = ctx->nodes[from].x - ctx->nodes[to].x;
    long long dy = ctx->nodes[from].y - ctx->nodes[to].y;
    long long squared_distance = (dx * dx) + (dy * dy);
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

    if (edge_a->penalized_cost < edge_b->penalized_cost) return -1;
    if (edge_a->penalized_cost > edge_b->penalized_cost) return 1;
    if (edge_a->distance < edge_b->distance) return -1;
    if (edge_a->distance > edge_b->distance) return 1;
    return 0;
}

int compare_edges_original(const void* a, const void* b) {
    const Edge* edge_a = (const Edge*)a;
    const Edge* edge_b = (const Edge*)b;
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

bool union_cities(DisjointSet ds, int city_a, int city_b) {
    int root_a = find_root(ds, city_a);
    int root_b = find_root(ds, city_b);

    if (root_a != root_b) {
        if (ds.ranks[root_a] < ds.ranks[root_b]) {
            ds.parents[root_a] = root_b;
        } else if (ds.ranks[root_a] > ds.ranks[root_b]) {
            ds.parents[root_b] = root_a;
        } else {
            ds.parents[root_b] = root_a;
            ds.ranks[root_a]++;
        }
        return true;
    }
    return false;
}

double sum_penalized_values(GraphContext* ctx) {
    double total = 0.0;
    for (int i = 0; i < ctx->node_count; i++) total += ctx->pis[i];
    return total;
}

// EXTRACT ORIGINAL MST (Static Geometry)
Edge* extract_raw_mst(GraphContext* ctx) {
    Edge* raw_mst = malloc((ctx->node_count - 1) * sizeof(Edge));

    // Sort by pure geometric distance
    qsort(ctx->edges, ctx->edge_count, sizeof(Edge), compare_edges_original);

    DisjointSet ds = disjointset_create(ctx->node_count);
    int mst_edge_count = 0;
    int edge_idx = 0;

    for (int i = 0; i < ctx->edge_count; i++) {
        int u = ctx->edges[i].from;
        int v = ctx->edges[i].to;
        
        if (union_cities(ds, u, v)) {
            raw_mst[edge_idx++] = ctx->edges[i];
            mst_edge_count++;
            if (mst_edge_count == ctx->node_count - 1) break;
        }
    }
    disjointset_free(ds);

    return raw_mst;
}

void extract_1_tree_weights(GraphContext* ctx, int spacial_node) {
    double one_tree_penalized_weight = 0.0;
    int mst_edge_count = 0;
    DisjointSet ds = disjointset_create(ctx->node_count);
    int spacial_node_connected_count = 0;

    for (int i = 0; i < ctx->node_count; i++) ctx->degrees[i] = 0;

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
            if (mst_edge_count == ctx->node_count - 2 && spacial_node_connected_count == 2) break;
        }
    }

    disjointset_free(ds);
    ctx->curr_lower_bound = one_tree_penalized_weight - 2 * sum_penalized_values(ctx);
}

double calculate_step_size(GraphContext* ctx) {
    double denominator = 0.0;
    for (int i = 0; i < ctx->node_count; i++) {
        double diff = (double) ctx->degrees[i] - 2.0;
        denominator += diff * diff;
    }
    if (denominator == 0.0) {
        printf("[DynamicStepSize] Found Perfect TSP Tour!\n");
        return 0.0;
    }
    double numerator = ctx->upper_bound - ctx->curr_lower_bound;
    return ctx->epsilon * (numerator) / denominator;
}

void estimate_upper_bound(GraphContext* ctx) {
    int n = ctx->node_count;
    bool* visited = (bool*) calloc(n, sizeof(bool));

    int current_node = 0;
    visited[current_node] = true;
    ctx->upper_bound_tour[0] = current_node;

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
        ctx->upper_bound += min_dist;
        visited[next_node] = true;
        ctx->upper_bound_tour[step] = next_node;
        current_node = next_node;
    }

    ctx->upper_bound += calculate_euclidean_distance(ctx, current_node, ctx->upper_bound_tour[0]);
    free(visited);
    printf("[Init] Initial Upper Bound: %lf\n", ctx->upper_bound);
}

typedef enum {
    HK_CONVERGED,
    HK_NATURAL_TOUR_FOUND,
    HK_ERROR_MEMORY
} HKStatus;

#define MIN_EPSILON 1e-4

void save_best_pi_values(GraphContext* ctx) {
    memcpy(ctx->best_pis, ctx->pis, ctx->node_count * sizeof(double));
}

HKStatus optimaize_held_karp_relaxation(GraphContext* ctx) {
    int spacial_node = 0;
    ctx->epsilon = 2.0;
    int initial_period = ctx->node_count / 2;
    if (initial_period < 10) initial_period = 10;
    if (initial_period > 100) initial_period = 100;
    ctx->best_lower_bound = -DBL_MAX;
    HKStatus status = HK_CONVERGED;

    for (int period = initial_period; period > 0 && ctx->epsilon > MIN_EPSILON;
            period *= 0.5, ctx->epsilon *= 0.5) {
        printf("[Held-Karp] Starting Period Phase. Scale Steps: %d, Epsilon (Step Size Modifier): %e\n",
                period, ctx->epsilon);
        bool improvement_found = false;

        for (int step = 1; step <= period; step++) {
            apply_edges_penalized_cost(ctx);
            qsort_edges(ctx);
            extract_1_tree_weights(ctx, spacial_node);
            
            if (ctx->curr_lower_bound > ctx->best_lower_bound) {
                printf("    [Lower Bound Improvement Found] Period Step %d: Old Best LB = %lf -> New Best LB = %lf (Diff: %+lf)\n",
                        step, ctx->best_lower_bound, ctx->curr_lower_bound, ctx->curr_lower_bound - ctx->best_lower_bound);
                ctx->best_lower_bound = ctx->curr_lower_bound;
                save_best_pi_values(ctx);
                improvement_found = true;
            }

            double step_size = calculate_step_size(ctx);
            if (step_size == 0.0) {
                status = HK_NATURAL_TOUR_FOUND;
                save_best_pi_values(ctx);
                goto exit;
            }

            for (int i = 0; i < ctx->node_count; i++) {
                int nodeid = ctx->nodes[i].id;
                double gradient = ctx->degrees[nodeid] - 2;
                ctx->pis[nodeid] += step_size * gradient;
            }
        }

        if (improvement_found) {
            period *= 2;
            ctx->epsilon *= 2;
            continue;
        }
    }
exit:
    return status;
}

typedef struct {
    int target;
    double cost; // GEOMETRIC cost, not penalized
} TreeEdge;

typedef struct {
    TreeEdge* edges;
    int* head;
    int* next;
    int edge_count;
} TreeGraph;

void add_tree_edge(TreeGraph* g, int u, int v, double cost) {
    int e = g->edge_count++;
    g->edges[e].target = v;
    g->edges[e].cost = cost;
    g->next[e] = g->head[u];
    g->head[u] = e;
}

// Static Geometric BFS
void bfs_max_edges(TreeGraph* g, int root, int n, double* c_max, bool* visited) {
    int* queue = malloc(n * sizeof(int));
    int head = 0, tail = 0;
    visited[root] = true;
    c_max[root] = 0.0;
    queue[tail++] = root;
    
    while (head < tail) {
        int current_node = queue[head++];
        double current_max = c_max[current_node];
        
        for (int e = g->head[current_node]; e != -1; e = g->next[e]) {
            int neighbor = g->edges[e].target;
            if (!visited[neighbor]) {
                visited[neighbor] = true;
                double edge_cost = g->edges[e].cost;
                c_max[neighbor] = (edge_cost > current_max) ? edge_cost : current_max;
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

int compare_alphas(const void *a, const void *b) {
    double alpha_a = ((AlphaPair*)a)->alpha;
    double alpha_b = ((AlphaPair*)b)->alpha;
    return (alpha_a > alpha_b) - (alpha_a < alpha_b);
}

// COMPUTED ON STATIC DISTANCES
void compute_all_candidate_sets(GraphContext* ctx, TreeGraph *g) {
    int node_count = ctx->node_count;
    int max_candidates = ctx->max_candidates;
    double* c_max = malloc(node_count * sizeof(double));
    AlphaPair* pairs = malloc(node_count * sizeof(AlphaPair));
    bool* visited = malloc(node_count * sizeof(bool));

    for (int i = 0; i < node_count; i++) {
        for (int j = 0; j < node_count; j++) {
            visited[j] = false;
            c_max[j] = 0.0;
        }

        bfs_max_edges(g, i, node_count, c_max, visited);

        for (int j = 0; j < node_count; j++) {
            pairs[j].node_id = j;
            if (i == j) {
                pairs[j].alpha = DBL_MAX; 
            } else {
                // FIXED: Use ORIGINAL geometric distance only, NO penalties
                double dist = calculate_euclidean_distance(ctx, i, j);
                double alpha_val = dist - c_max[j];
                pairs[j].alpha = (alpha_val < 1e-6) ? 0.0 : alpha_val;
            }
        }

        qsort(pairs, node_count, sizeof(AlphaPair), compare_alphas);

        for (int k = 0; k < max_candidates; k++) {
            ctx->candidates[i * max_candidates + k] = pairs[k].node_id;
        }

        if ((i + 1) % 10 == 0 || (i + 1) == node_count) {
            printf("[Static Candidates] Computed alpha-nearness for %d / %d nodes.\n", i + 1, node_count);
        }
    }

    free(c_max);
    free(pairs);
}

TreeGraph init_tree_graph(int n) {
    TreeGraph g;

    g.edge_count = 0;
    g.edges = malloc(2 * n * sizeof(TreeEdge));
    g.next  = malloc(2 * n * sizeof(int));
    g.head  = malloc(n * sizeof(int));
    
    for (int i = 0; i < n; i++) g.head[i] = -1;

    return g;
}

void generate_alpha_candidate_sets(GraphContext* ctx) {
    // Extract MST on RAW distances, no penalties
    Edge* raw_mst = extract_raw_mst(ctx);

    int n = ctx->node_count;

    TreeGraph g = init_tree_graph(n);

    // Use only N-1 edges of the MST
    for (int i = 0; i < n - 1; i++) {
        int u = raw_mst[i].from;
        int v = raw_mst[i].to;
        double weight = raw_mst[i].distance; // GEOMETRIC
        
        add_tree_edge(&g, u, v, weight);
        add_tree_edge(&g, v, u, weight);
    }

    compute_all_candidate_sets(ctx, &g);

    free(g.edges);
    free(g.next);
    free(g.head);
    free(raw_mst);
}

#define MAX_LK_DEPTH 10

typedef struct {
    int t[2 * MAX_LK_DEPTH + 1];
    int current_depth;
    bool* edge_status_changed;
} LKContext;

bool is_edge_disjoint(LKContext* lkctx, int u, int v, int current_k) {
    for (int i = 1; i < current_k; i++) {
        int b1 = lkctx->t[2 * i - 1], b2 = lkctx->t[2 * i];
        if ((b1 == u && b2 == v) || (b1 == v && b2 == u)) return false;
    }
    for (int i = 1; i < current_k; i++) {
        int a1 = lkctx->t[2 * i], a2 = lkctx->t[2 * i + 1];
        if ((a1 == u && a2 == v) || (a1 == v && a2 == u)) return false;
    }
    return true;
}

typedef struct {
    int k_idx;
    int direction;
    double saved_gain;
} LKLoopState;

int get_tour_successor(GraphContext* ctx, int node) {
    return ctx->tour[(ctx->pos[node] + 1) % ctx->node_count];
}

int get_tour_predecessor(GraphContext* ctx, int node) {
    return ctx->tour[(ctx->pos[node] - 1 + ctx->node_count) % ctx->node_count];
}

typedef struct { 
    int to_local;
    bool is_added; 
} VirtualEdge;

typedef struct {
    int node;
    int pos; 
} SortNode;

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
    ws->t[2 * k_depth] = t_2k; 

    SortNode nodes[20];
    for (int i = 1; i <= num_nodes; i++) {
        nodes[i-1].node = ws->t[i];
        nodes[i-1].pos = ctx->pos[ws->t[i]];
    }

    qsort(nodes, num_nodes, sizeof(SortNode), compare_sort_nodes);

    VirtualEdge v_graph[20][2];
    int v_count[20] = {0};

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

    for (int i = 0; i < num_nodes; i++) {
        int u = nodes[i].node;
        int succ = get_tour_successor(ctx, u);
        if (!is_broken_edge(ws, k_depth, u, succ)) {
            int next_local = (i + 1) % num_nodes;
            v_graph[i][v_count[i]++] = (VirtualEdge){next_local, false};
            v_graph[next_local][v_count[next_local]++] = (VirtualEdge){i, false};
        }
    }

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

    int *new_tour = malloc(ctx->node_count * sizeof(int));
    int filled = 0;
    int curr_local = l_t1;
    int curr_node = lkctx->t[1];
    int prev_local = -1;

    new_tour[filled++] = curr_node;

    while (filled < ctx->node_count) {
        int edge_idx = (v_graph[curr_local][0].to_local == prev_local) ? 1 : 0;
        VirtualEdge edge = v_graph[curr_local][edge_idx];
        int next_local = edge.to_local;

        if (edge.is_added) {
            curr_node = nodes[next_local].node;
            if (filled < ctx->node_count) new_tour[filled++] = curr_node;
        } else {
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

    for (int i = 0; i < ctx->node_count; i++) {
        ctx->tour[i] = new_tour[i];
        ctx->pos[new_tour[i]] = i;
    }

    free(new_tour);
}

double calculate_current_tour_cost(GraphContext* ctx) {
    double total = 0.0;
    for (int i = 0; i < ctx->node_count; i++) {
        total += calculate_euclidean_distance(ctx, ctx->tour[i], ctx->tour[(i + 1) % ctx->node_count]);
    }
    return total;
}

bool lk_search_step(GraphContext* ctx, LKContext* lkctx) {
    LKLoopState loop_stack[MAX_LK_DEPTH];
    int depth = 1;
    loop_stack[depth].k_idx = 0;
    loop_stack[depth].direction = 0;
    loop_stack[depth].saved_gain = 0;

    while (depth > 0) {
        int t_2i = lkctx->t[2 * depth];
        double cumulative_gain = loop_stack[depth].saved_gain;
        bool step_deepened = false;

        for (; loop_stack[depth].k_idx < ctx->max_candidates; loop_stack[depth].k_idx++) {
            int t_2i_plus_1 = GET_CANDIDATE(ctx, t_2i, loop_stack[depth].k_idx);

            if (t_2i_plus_1 == lkctx->t[2 * depth - 1] || !is_edge_disjoint(lkctx, t_2i, t_2i_plus_1, depth)) continue;

            double added_cost = calculate_euclidean_distance(ctx, t_2i, t_2i_plus_1);
            double break_cost = calculate_euclidean_distance(ctx, lkctx->t[2 * depth - 1], t_2i);
            double step_gain = break_cost - added_cost;

            if (cumulative_gain + step_gain <= 1e-6) continue;

            lkctx->t[2 * depth + 1] = t_2i_plus_1;

            for (; loop_stack[depth].direction < 2; loop_stack[depth].direction++) {
                int t_2i_plus_2 = (loop_stack[depth].direction == 0)
                    ? get_tour_successor(ctx, t_2i_plus_1) : get_tour_predecessor(ctx, t_2i_plus_1);

                if (!is_edge_disjoint(lkctx, t_2i_plus_1, t_2i_plus_2, depth)) continue;

                double closure_gain = calculate_euclidean_distance(ctx, t_2i_plus_1, t_2i_plus_2) -
                                      calculate_euclidean_distance(ctx, t_2i_plus_2, lkctx->t[1]);

                if (cumulative_gain + step_gain + closure_gain > 1e-6) {
                    lkctx->current_depth = depth + 1;
                    if (validate_tour_feasibility(ctx, lkctx, t_2i_plus_2)) {
                        lkctx->t[2 * depth + 2] = t_2i_plus_2;
                        execute_variable_lk_swap(ctx, lkctx);
                        return true;
                    }
                }

                if (depth + 1 < MAX_LK_DEPTH) {
                    lkctx->t[2 * depth + 2] = t_2i_plus_2;
                    int next_depth = depth + 1;
                    loop_stack[next_depth].k_idx = 0;
                    loop_stack[next_depth].direction = 0;
                    loop_stack[next_depth].saved_gain = cumulative_gain + step_gain;
                    loop_stack[depth].direction++;
                    depth = next_depth;
                    step_deepened = true;
                    break;
                }
            }
            if (step_deepened) break;
            loop_stack[depth].direction = 0;
        }

        if (step_deepened) continue;
        
        depth--;
        if (depth > 0 && loop_stack[depth].direction >= 2) {
            loop_stack[depth].direction = 0;
            loop_stack[depth].k_idx++;
        }
    }
    return false;
}


void run_lk_engine(GraphContext* ctx) {
    bool improvement_found = true;
    LKContext lkctx;
    lkctx.edge_status_changed = calloc(ctx->node_count, sizeof(bool));


    int epoch = 0;
    double lower_bound = ctx->best_lower_bound;
    double current_cost = calculate_current_tour_cost(ctx);
    
    printf("[LK Engine] Init. Baseline: %.4lf | HK Lower Bound: %.4lf\n", current_cost, lower_bound);

    while (improvement_found) {
        improvement_found = false;
        epoch++;
        current_cost = calculate_current_tour_cost(ctx);
        printf("    [LK Engine] Epoch %d. Cost: %.4lf\n", epoch, current_cost);
        printf("    [LK Engine] Gap From Actual Solution: %lf%%\n", ((current_cost - BEST_TOUR_SOLUTION) / BEST_TOUR_SOLUTION) * 100.0);

        for (int n_idx = 0; n_idx < ctx->node_count; n_idx++) {
            lkctx.t[1] = ctx->tour[n_idx];
            lkctx.current_depth = 1;

            lkctx.t[2] = get_tour_successor(ctx, lkctx.t[1]);
            memset(lkctx.edge_status_changed, 0, ctx->node_count * sizeof(bool));
            if (lk_search_step(ctx, &lkctx)) { improvement_found = true; break; }

            lkctx.t[2] = get_tour_predecessor(ctx, lkctx.t[1]);
            memset(lkctx.edge_status_changed, 0, ctx->node_count * sizeof(bool));
            if (lk_search_step(ctx, &lkctx)) { improvement_found = true; break; }
        }
    }


    current_cost = calculate_current_tour_cost(ctx);
    printf("-----------------------------------------------------\n");
    printf("[Execution Summary] Converged after %d epochs.\n", epoch);
    printf("    [LK Engine] Local Optima Achieved. Execution converged after %d epochs.\n", epoch);
    printf("    [LK Engine] Held-Karp Best Lower Bound: %lf\n", ctx->best_lower_bound);
    printf("    [LK Engine] Final LK Optimaized Tour Cost: %lf\n", current_cost);
    printf("    Gap From Actual Solution: %lf%%\n", ((current_cost - BEST_TOUR_SOLUTION) / BEST_TOUR_SOLUTION) * 100.0);
    printf("-----------------------------------------------------\n");

    free(lkctx.edge_status_changed);
}

void generate_nearest_neighbor_tour(GraphContext* ctx, int start_node) {
    int hit_elite_count = 0;
    int fallback_count = 0;

    bool *visited = calloc(ctx->node_count, sizeof(bool));
    ctx->tour[0] = start_node;
    ctx->pos[start_node] = 0;
    visited[start_node] = true;
    int current_node = start_node;
    int m = ctx->max_candidates;

    for (int i = 1; i < ctx->node_count; i++) {
        int next_node = -1;
        double min_dist = DBL_MAX;

        if (ctx->candidates != NULL) {
            for (int k = 0; k < m; k++) {
                int candidate = ctx->candidates[current_node * m + k];
                if (!visited[candidate]) {
                    double dist = calculate_euclidean_distance(ctx, current_node, candidate);
                    if (dist < min_dist) { min_dist = dist; next_node = candidate; }
                }
            }
        }

        if (next_node == -1) {
            // For tracking progress.
            fallback_count++;

            for (int j = 0; j < ctx->node_count; j++) {
                if (!visited[j]) {
                    double dist = calculate_euclidean_distance(ctx, current_node, j);
                    if (dist < min_dist) { min_dist = dist; next_node = j; }
                }
            }
        } else hit_elite_count++;

        ctx->tour[i] = next_node;
        ctx->pos[next_node] = i; 
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

    // Load Data & Edges
    load_cities_from_file(&ctx, "./kroA100.tsp");
    compute_edges(&ctx);

    // Caculate estimated upper bound.
    estimate_upper_bound(&ctx);
    
    // Generate candidates based on TRUE MST
    generate_alpha_candidate_sets(&ctx);

    // Held-Karp Optimaization.
    optimaize_held_karp_relaxation(&ctx);

    // Initial Tour Construction & LK Local Search Improvement.
    generate_nearest_neighbor_tour(&ctx, 0);
    run_lk_engine(&ctx);

    print_named_tour(&ctx);

    return 0;
}
