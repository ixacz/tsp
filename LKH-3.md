# LKH-3 Algorithm
The LKH-3 Algorithm is a world class algorithm designed for solving the
traveling salesman problem (TSP).

TODO: Take 2-opt and 3-opt sections to else palce.

## 2-opt alogorithm:
its the simplest alogorithm in this family (of Local Search Algorithms),
its main goal is to cut crossover edges of a tour, beacuse any crossover
of two edges in a Euclidean space is definitely a not perfect tour and 
can be shorter.

#### its way of reconnecting the edges:
- it chooses two edges randomly (like the edge A - B and C - D).
- it deletes these two edges.
- reconnect them in a different way: A -C and B - D.
- then calculates the total distance:
    - if the new distance is shorter, it adopts it.
    - if not, it rejects the amendment, and try another two edges.
- this process continues until it test every combination possible
    and doesn't find any way to reduce the total distance.

## 3-opt algorithm:
it is a direct development for 2-opt, but its works with 3 edges at a 
time, and reconnect them in different orders.

#### its way of reconnecting the edges:
- its chooses 3 edges, then cuts the tour into 3 distenct sub-tours.
- then there will be 8 possible way to reconnect the sub-tours into a
    big tour, on of which is the original tour, and another two are 
    2-opt reconnections, and the last 5 are 3-opt ways to reconnect 
    the sub-tours.
- it tests these combinations of sub-tours to find the shortest one.
    - its aventage over 2-opt is that it can see more complicated connections
        and break more of them than what 2-opt can.
    - but that comes at a cost of Time-Complexily, whcih is O(n^3) rather than
        O(n^2) for 2-opt.

### the generic algorithm k-opt (reconnect k edges):
if we generilaized the idea of reconnecting the edges, we can go further by k 
number of edges and cut and reconnect them in all possible ways to escape from 
local solutions.

- if k was equal to 4 (opt-4), we delete 4 edges and try every combination 
    possible, and so on.
- so the theoretical rule is that when the value of k increases, the closer the
    solution is to being accurate from 100% (perfect solution), so if k = n 
    (the total number of cities), the algorithm will definitely reach to the 
    perfect solution.

## An intorduction to the genius of LKH-3 alogorithm:
In 1973, Shen Lin and Brian Kernighan introduced the LK algorithm,
which pioneered the concept of variable k-opt moves. Instead of swapping
a fixed number of edges (like changing exactly 2 or 3 connections), the 
algorithm dynamically determined how many edges to exchange on the fly 
to break out of suboptimal patterns.

In 2000, Keld Helsgaun revolutionized this heuristic by introducing LKH.
He added a powerful mathematical driver to guide the local search engine:
The Held-Karp Lower Bound. By combining subgradient optimization with the
LK swapping engine, LKH became the most powerful TSP solver in existence.

In 2017, Helsgaun released LKH-3, expanding the system to handle highly 
complex variations of the problem, such as multi-salesperson routing, 
time windows, and clustering, cementing its position as the world-record
holder for solving massive TSP instances efficiently.

LKH-3 achieves its speed by breaking the problem down into a strict 
pipeline of distinct phases. It transforms a vast geometric mess into a 
highly localized, hyper-focused optimization tasks.

### Step 1:
We begin with calculating the edges and distances between citiies.

### Step 2: The Held-Karp loop (Generating the mathematical floor)
section1:
1. The Mathematical Foundation: Lagrangian Relaxation

Before swapping a single edge, LKH-3 runs subgradient optimization to 
calculate a "penalized distance" for every city using a vector called 
`pi`. It extracts a 1-Tree on every iteration. If a city is visited too
many times (degree > 2), its penalty (`pi`) goes up, making its connecting
edges more expensive. If a city is isolated (degree < 2), its penalty
goes down. Over hundreds of fast iterations, this process flattens the
graph out, raising the Lower Bound until it sits directly underneath the
optimal tour length.

The Traveling Salesperson Problem can be modeled as a pure integer 
programming task: minimize total tour cost subject to the constraint
that every single node must have a degree of exactly 2 without creating
disjoint sub-loops. Because enforcing a degree of exactly 2 across all 
nodes simultaneously is NP-hard, the Held-Karp method uses a technique 
called Lagrangian Relaxation. Instead of strictly forcing the degree 
constraint, we remove it from the hard rules and move it directly into 
the optimization objective function as a penalty. For any chosen vector
of real numbers \pi = (\pi_1, \pi_2, \dots, \pi_N), we define a new, 
modified penalized cost matrix C':

Where C_{ij} is the original geographic distance between city i and city j.
If we add these penalties to every node, the cost of any subgraph containing
N edges scales systematically based on the degrees (d_i) of its internal nodes:

2. The Mechanics of the Loop: Subgradient Ascent

The optimization process uses Subgradient Ascent (the dual equivalent of 
subgradient descent) to maximize the lower bound of the graph. The loop 
executes through four repeating steps:

Step 1: Compute the Minimum 1-Tree
Using the current penalized weights (C'_{ij}), your code runs Kruskal's 
algorithm to find an MST on N-1 nodes, and then hooks the isolated special node
back up using its two cheapest penalized paths. The resulting total penalized 
weight is recorded as W(\pi).

Step 2: Calculate the Subgradient Vector (G)
The subgradient represents the mathematical direction of steepest error.
For each node i, we calculate how much its current degree (d_i) in the 
1-tree deviates from the target TSP tour degree of 2:

If a node is highly congested (e.g., d_i = 4), its subgradient is positive
(4 - 2 = +2). If a node is isolated or an endpoint (e.g., d_i = 1), its 
subgradient is negative (1 - 2 = -1). If a node perfectly hits a valid tour
state (d_i = 2), its subgradient collapses to zero (2 - 2 = 0).

Step 3: Compute the Step Size (t)
We cannot simply change \pi blindly; doing so would cause the system to
wildly overshoot the optimal balance. The change is scaled using a step
-size scalar t, driven by Held and Karp's classic convergence formula:

\lambda (Lambda): A user-controlled or adaptive dampening factor that
    starts near 2.0 and decays down toward 0 as iterations progress.
\text{Target\_Bound}: An estimation of the true upper limit (often
    seeded by an initial fast tour heuristic like nearest neighbor, or
    adapted by the outer loop).
Denominator (\sum G_i^2): The sum of squared errors. If the tree layout
    is chaotic and degrees are far from 2, the denominator grows, making
    the step size smaller and more stable.

Step 4: Update Node Penalties (\pi)
Finally, the penalty vector is adjusted for the next iteration:

If a node's degree was too high, its \pi_i value increases. In the next
iteration, all edges connected to that node become more expensive, naturally
forcing Kruskal's algorithm to avoid it. Conversely, neglected nodes get a
negative penalty adjustment, making their connections cheaper and highly 
attractive for the next run.

3. The Goal: Why LKH-3 Performs This Matrix Warp
It is crucial to understand that the Held-Karp loop does not change the 
physical optimal route. If a true optimal TSP tour is a closed loop passing
through every city, every node in that tour has a degree of exactly 2. Let's
look at what happens to the length of a true tour under this mathematical
transformation:

Notice that because every node in a valid tour is visited exactly twice,
the total sum of penalties added to a valid tour is exactly 2 \sum \pi_i.
This value is a constant for any valid tour layout. Therefore, if Tour X
was shorter than Tour Y under the original coordinates, Tour X is still 
shorter than Tour Y under the penalized coordinates. The relative ordering
of valid tours remains completely preserved. The True Secret: Eliminating
Deceptive Non-Tour Edges What does change is the cost of messy, non-tour 
1-trees. By raising the cost of congested nodes and lowering the cost of
isolated nodes, the Held-Karp loop systematically penalizes non-tour 
structures. As the loop converges, the penalized weight of the Minimum 
1-Tree rises rapidly, climbing until it hits a mathematical ceiling directly
beneath the optimal tour cost.

---
### 1-Tree Explenation:

and then we need to extract the 1-Tree, A valid TSP tour is exceptionally hard 
to find because of a brutal constraint: it must be a single, continuous loop 
where every city connects to exactly two other cities. Fiding a minimum-weight 
spaning structure with that exact property is an NP-Hard nightmare.

The 1-Tree bypass this difficulty by relaxing te rules just enough to make the
math easy, creating a structure that acts as a perfect proxy for a TSP tour.

Mathimatically, a `1-Tree` is a graph structure biult on N cities the contains 
exactly N edges and exactly one closed loop, and that is done with a simple
two-step decomposition trick:

- Build the core tree, pick one city to be the `special node` (like city 0),
    We completely remove it from the map and build a standard `Minimum Spaning 
    Tree` (MST) across the remaining N-1 cities using Kruskal's algorithm.

- Once that MST is found, we look back at City 0 and forcefully connect City 0
    back into the produced MST using its two absolute cheapest available edges.
    based on your current costs.

Because a standard MST has no loops and one extra node (city 0) will guarantee
to create exactly one cycle.

#### why the 1-Tree is a perfect mirror for TSP:
Lets look at structural requirements of a valid TSP tour versus a 1-Tree:

- A valid TSP tour has N edges, fully connected, has exactly one cycle, and
    every node has a degree of exactly 2.

- A 1-Tree has N edges, fully connected, has exactly one cycle, but its nodes
    degrees can be anything (degree of 1 or more), which is the number of 
    connected edges into it.

this means a valid TSP tour is aimply a spacial type of 1-Tree where every 
node's degree happens to equal 2, and its simple rules and structure makes it
incredibly easy to build.

#### The process of extracting the Minimum Spaning Tree (MST):
To extract the Minimum Spaning Tree (MST), we use `Kruskal's Algorithm`, since
we are building a `1-Tree`, we are ignoring a spacial node, so this leaves us 
with N - 1 cities, and in graph theory, a Minimum Spaning Tree connecting a set
of vertices (V) alway requires exactly V - 1 edges to connecte them all without
making a loop, thierfore our total edges number will be (N - 1) - 1 (the spacial
node).

##### The core tool: The Disjoint Set Union (DSU)
As Kruskal's algorithm scans through your sorted edges (from cheapest to most 
expensive), it asks a simple question: "if I add this edge, will it create an 
illegal closed loop?", the Disjoint Set structure anwsers this question instantly
by tracking which cities are already connected into structural branches. it uses
two simple arrays:

- **parents**: tracks the immediate root (parent) of each city in its group, if
    a city is its own parent, it is the root of its independent group.
- **ranks**: tracks the depth of the tree branches of a group to ensure that when
    we merge two groups, we connect the shorter one underneath the taller one 
    (keeping our code faster).

When the algorithm starts, every city is a completly isolated individual. No one
knows anyone else. Each city forms its own independent "social circle" of exactly
one person. The `Disjoint Set` manages these circles using two core operations:

- **Find**: Asks a city, "Who is the absolute leader (root parent) of your social
    circle?".
- **Union**: Takes two completly sparate social circles and merge them together 
    under s single, unified leader.

As the algorithm loops over the sorted adges, it takes two city ids and check for
thier roots (Each in its own social circle or group), if thier roots are not equal
it can safely takes the edge between them, and then combines the shorter ranked 
group into the taller one, so the bigger ranked group's root is the root of the 
smaller ranked group. If the roots are equal it skips them because they have a 
shorter in common route so they do not need a direct edge between them that will 
cause a closed loop.

### Subgradient Ascent Error Explenation:
To understand how the subgradient represents the mathematical direction 
of steepest error, it helps to step back from code and look at the
geometry of optimization. If you are climbing a mountain in a thick fog
and want to reach the peak as quickly as possible, you look at the ground 
beneath your feet and step in the direction where the slope goes up the
steepest. In calculus, that direction of steepest ascent is called the 
gradient. However, our Held-Karp objective function graph is not a 
perfectly smooth, rounded mountain. Because a 1-tree is made of sharp, 
discrete edge selections, the function graph looks like a jagged, multi-
faced diamond or a pyramid. It has sharp ridges and corners where standard
calculus breaks down because you can't take a traditional derivative at a
sharp point. This is where the subgradient comes in. It is a generalization
of the gradient for jagged, non-smooth functions. 

1. What is "Error" in Held-Karp?
In Phase 2, our absolute "dream state" is a valid TSP tour. In a
valid tour, every single city has a target degree of exactly 2. If the 
algorithm generates a 1-tree where a city has a degree of 4, we have a 
structural error. The city is over-congested. If a city has a degree of 1,
we also have an error; the city is isolated. We can define the individual
structural error for any node i as:

2. Connecting Structural Error to the Slope of the Function
Recall our generalized penalized cost formula:

Let's look at how changing a single city's penalty (\pi_i) affects the 
total penalized cost (W(\pi)). If we treat the chosen 1-tree edges as 
temporarily fixed, we can take the partial derivative of the cost with 
respect to that city's penalty:

This tells us that the current rate of change (the slope) of our cost 
function along the \pi_i axis is exactly equal to the node's current 
degree, d_i. However, we are trying to maximize the lower bound, but 
we must do so without letting the penalties distort the graph infinitely.
The dual optimization problem dictates that we want to balance the 
system where our target step adjusts the slope relative to our constraint
(the target degree of 2). When we subtract the target constraint, the 
vector of directional slopes becomes:

This vector G is our subgradient.

3. Why It Points in the Direction of "Steepest Error Correction"
The subgradient vector tells us exactly how to adjust our 
coordinates to fix our structural mistakes. Because it is a vector,
it points simultaneously in N different directions (one for each city).

Let's see what happens when the subgradient dictates our next step 
(\pi_i \leftarrow \pi_i + t \cdot G_i):

If a city has a degree of 5 (G_i = +3):

The slope is highly positive. The subgradient points heavily in the 
positive direction. By stepping forward, we aggressively increase 
\pi_i. This forces the edges connected to this city to become massive 
and expensive on the very next iteration, forcing Kruskal's algorithm
to strip edges away from it.

If a city has a degree of 1 (G_i = -1): The subgradient points in the 
negative direction. By stepping backward, we decrease \pi_i. This makes
the edges connected to this city incredibly cheap, forcing Kruskal's 
algorithm to route paths through it on the next iteration.

The next step in the LKH-3 pipeline is Phase 3: The Alpha (\alpha) Transformation and Candidate Set Generation.

This phase is where the magic happens. It takes your optimized 1-tree and uses it as a mathematical filter to permanently delete roughly 95% to 99% of the edges in your graph, reducing a computationally impossible problem into a series of blistering-fast local lookups.

1. What is the Alpha (\alpha) Value?
In traditional graph theory, if you want to know if an edge is good, you look at its distance. But in LKH-3, we look at its \alpha-value (also known as the excess cost or regret value).
The \alpha-value of an edge (i,j) is mathematically defined as: How much would the total cost of the Minimum 1-Tree increase if we forced the algorithm to include this specific edge?

2. Why Do We Do This? (The Candidate Set Filter)
If you have a dataset of 1,000 cities, every city has 999 potential roads it could take. When the local search engine (k-opt) starts trying to swap edges to find the best tour, searching through 999 options for every single city creates an exponential slowdown (O(N^2) or worse).

Phase 3 solves this by building a Candidate Set.
For every single city, LKH-3 calculates the \alpha-value of all its connecting edges.
It sorts those edges from lowest \alpha to highest \alpha.
It keeps the top 5 candidates (Helsgaun proved 5 is the sweet spot) and throws everything else away.
When your Phase 4 k-opt engine runs later, it operates in a pruned environment. If City A wants to swap an edge, it doesn't look at 999 cities across the map—it only evaluates its 5 pre-approved local candidates. This transforms your search space into an incredibly fast O(1) constant-time routine.

The Concept of \alpha-Nearness in Graph Sensitivity Analysis
In combinatorial optimization, identifying which edges have the highest probability of belonging to the optimal Traveling Salesperson tour requires moving beyond raw spatial distance. While the penalized weights (C'_{ij}) generated by the Held-Karp relaxation provide an excellent lower bound, they do not explicitly measure the individual worth of non-tree edges. To solve this, the Lin-Kernighan-Helsgaun (LKH) algorithm relies on a sensitivity metric known as \alpha-nearness (or the regret value).

The \alpha-nearness of an edge serves as a mathematical filter, quantifying the exact cost penalty that would be incurred if the system were forced to include a specific, suboptimal edge into the optimal Minimum 1-Tree.

1. Mathematical Definition of Regret
Let W(\pi) denote the total penalized cost of the absolute optimal Minimum 1-Tree discovered at the end of the subgradient optimization phase.

Suppose we select an arbitrary edge (i,j) from the global set of all possible edges E, and we impose a strict constraint: the algorithm must generate a new minimum 1-tree that explicitly includes edge (i,j). Let the cost of this newly constrained 1-tree be denoted as W_{i,j}(\pi).

The \alpha-nearness value for the edge (i,j) is defined as the difference between these two states:

Because W(\pi) represents the unconstrained global minimum, the constrained tree W_{i,j}(\pi) must be strictly greater than or equal to W(\pi). Therefore, \alpha(i,j) is always a non-negative value (\alpha \ge 0).


2. Topological Interpretation of Alpha Values
The magnitude of an edge's \alpha-value provides a precise, theoretical assessment of its utility:

\alpha(i,j) = 0: The edge (i,j) is already a natural component of the optimal 1-tree. Forcing its inclusion changes nothing, resulting in zero excess cost. These edges represent the backbone of the optimal network.

Small \alpha(i,j): The edge is not in the current 1-tree, but forcing it into the structure causes only a minimal increase in the global lower bound. These edges are mathematically highly competitive and have a significant probability of appearing in the true optimal TSP tour.

Large \alpha(i,j): Forcing the edge fundamentally disrupts the 1-tree, causing the total cost to skyrocket. Even if the edge is geographically short, its high \alpha-value indicates that it is a topological dead-end or structural trap.
By utilizing this metric, LKH transitions from an absolute measurement of distance to a relative measurement of structural necessity.


3. Computational Feasibility: The Path-Maximum Shortcut
Calculating W_{i,j}(\pi) independently for every possible edge in a dense graph would require executing Kruskal’s algorithm O(N^2) times, leading to an intractable computational bottleneck. However, Helsgaun bypasses this by leveraging the fundamental topological properties of spanning trees.

By definition, adding any new edge (i,j) to an existing tree creates exactly one closed cycle. To restore the strict tree structure (and eliminate the cycle) while maintaining the minimum possible cost, the algorithm must remove the single most expensive edge located on the original tree path connecting node i to node j.

Let P(i,j) represent the unique sequence of edges in the optimal 1-tree that connects node i to node j. The \alpha-nearness can be computed directly without re-evaluating the entire graph using the following formula:

Where:
C'_{ij} is the penalized weight of the new edge being forced into the tree.
\max C'_{uv} is the weight of the heaviest edge currently sitting on the path P(i,j) within the tree.

NOTE: consider adding this at the last part.
Summary for Optimization
This transformation is the mathematical crux of LKH-3's efficiency. Instead of executing millions of redundant tree calculations, the \alpha-matrix can be populated rapidly by mapping the maximum path weights between all node pairs along the branches of the frozen 1-tree. The resulting \alpha-values provide an optimal, mathematically rigorous scoring system to isolate the most critical edges in the network.

The Algorithmic Mechanics of Alpha Calculation: The Cycle-Exchange Theorem
While the theoretical definition of \alpha-nearness—forcing an edge into the Minimum 1-Tree and measuring the total cost difference—is conceptually sound, calculating it naively is computationally intractable. A brute-force approach would require running Kruskal’s or Prim’s algorithm O(N^2) times (once for every potential edge), resulting in an overwhelming global time complexity.
To resolve this, the Lin-Kernighan-Helsgaun (LKH) algorithm relies on a foundational property of graph topology known as the Fundamental Cycle. This property allows the algorithm to calculate the \alpha-value of any edge in O(1) constant time, provided a single, highly efficient pre-computation step has been executed.

1. The Topological Principle of Cycle Creation
A Minimum Spanning Tree (MST) on N nodes contains exactly N-1 edges and possesses no closed loops. By the definition of tree topology, there is exactly one unique, continuous path connecting any two arbitrary nodes, i and j. Let this unique path be denoted as P(i,j).

If we take a non-tree edge (i,j) and force it into the existing optimal tree, the structure immediately violates the tree constraint by forming exactly one closed loop. This loop consists of the new edge (i,j) combined with the original internal tree path P(i,j).

2. The Cost of Restoration (The Exchange Move)
To restore the graph to a valid tree structure, exactly one edge from this newly formed cycle must be deleted.
Because the objective of the algorithm is to find the minimum possible cost for this newly constrained tree, it must discard the most mathematically expensive edge within that cycle. Since the new edge (i,j) is strictly forced to remain, the deleted edge must be chosen from the original tree path P(i,j).
Therefore, the net change in the total cost of the tree (which is exactly the definition of \alpha) is simply the cost of the edge we added, minus the cost of the edge we threw away.
This yields the direct calculation formula:

Where:
C'_{ij} is the penalized Held-Karp weight of the newly added edge.
\max_{(u,v) \in P(i,j)} C'_{uv} represents the penalized weight of the heaviest edge natively existing on the path between i and j.

3. The O(N^2) Pre-Computation Strategy
While the formula above isolates the calculation to a single path, traversing the tree to find the maximum edge for every single (i,j) pair individually would still be computationally expensive.

Instead of searching the path on-demand, LKH pre-computes these path maximums for the entire graph simultaneously using an optimized traversal algorithm (typically Breadth-First Search or Depth-First Search).
The process executes as follows:

The algorithm selects a node i as the "root" of the tree.
It initiates a traversal outward along the tree branches.
As it visits an adjacent node v via an edge (u,v), it continuously tracks the heaviest edge seen so far. The maximum path weight to the new node v is simply the maximum of two values: the highest weight recorded up to the predecessor u, or the weight of the immediate connecting edge (u,v).
It records this maximum value in a static N \times N lookup table.
Mathematically, the recurrence relation during traversal is:

Summary for Methodological Implementation
By rooting a traversal at each of the N nodes, the algorithm populates an entire N \times N matrix of path maximums in exactly O(N^2) time. Once this matrix is built in memory, calculating the \alpha-nearness for any theoretical edge (i,j) is reduced to a simple, instantaneous arithmetic subtraction.
This transformation from an O(N^4) brute-force graph reconstruction to an elegant O(N^2) dynamic programming traversal is the specific architectural mechanism that allows LKH-3 to filter massive networks into 5-candidate subsets without stalling the CPU.

—
Graph Sparsification and the Construction of Candidate Sets
While the \alpha-matrix provides a mathematically rigorous sensitivity score for every potential connection in the network, utilizing this N \times N matrix directly during the routing phase remains computationally unfeasible. The objective of Phase 3 is to translate the continuous numerical regret (\alpha-nearness) into a discrete, highly sparse topological structure known as the Candidate Graph.
This process systematically isolates the most critical edges while permanently discarding statistically irrelevant connections, bounding the upcoming search space from O(N^2) to O(N).

1. Mathematical Definition of the Candidate Set
For any given node i in a complete graph G = (V, E), the global neighborhood encompasses N-1 possible edges. The Candidate Set C(i) is defined as a strictly bounded subset of these adjacent nodes, containing only the k most promising connections as dictated by the \alpha-matrix.
To construct C(i), the algorithm evaluates the set of all adjacent nodes j \in V \setminus \{i\} and sorts them in ascending order based on a multi-tiered evaluation criteria:

Primary Metric (\alpha-nearness): Nodes are ranked primarily by \alpha(i,j). Edges where \alpha(i,j) = 0 (those natively belonging to the optimal minimum 1-tree) are placed at the absolute top of the hierarchy.

Secondary Tie-Breaker (Penalized Weight): If multiple edges share an identical \alpha-value (which is highly common for \alpha = 0), they are sub-sorted by their Held-Karp penalized weight, C'_{ij}.

Tertiary Tie-Breaker (Geometric Distance): In the rare event of a penalized weight tie, the raw spatial distance C_{ij} serves as the final tie-breaker.

The algorithm truncates this sorted list, retaining only the top MAX\_CANDIDATES edges and discarding the rest.


2. The Theoretical Bound: Choosing the Candidate Limit
The size of the candidate set, |C(i)|, represents a critical trade-off between optimality preservation and execution speed.

If |C(i)| is too large, the branching factor of the subsequent local search engine will cause an exponential combinatorial explosion. If |C(i)| is too small, the algorithm risks pruning an edge that actually belongs to the true optimal Traveling Salesperson tour, mathematically locking the solver out of the global minimum.

Through extensive empirical analysis on TSPLIB datasets, Helsgaun demonstrated that the Held-Karp \alpha-nearness metric is so accurate that the true optimal edges almost invariably rank within the top 5 positions. Consequently, the standard parameter in LKH-3 enforces a strict limit of:

By restricting each node to a maximum degree of 5 within the candidate graph, the global search space collapses to an upper bound of 5N edges, rendering the time complexity of local search operations strictly linear with respect to the graph size.



3. Directed Selection and Graph Symmetrization
A critical structural challenge arises during candidate set generation due to the asymmetric nature of local neighborhood rankings.

Because the Candidate Sets are generated independently for each node, the relationships are initially directed. Node i might identify node j as its absolute best candidate (\alpha(i,j) = 0). However, node j might have 5 other connections with even lower localized weights, meaning node i fails to make the cut into C(j).

If the graph is left in this asymmetric state, the local search engine encounters fatal dead-ends, as a routing path traversed from j to i would be invisible to the algorithm while traversing from i to j.

To resolve this and ensure the structural integrity of the Candidate Graph, the algorithm executes a Symmetrization Pass.

After all initial sets are populated, the algorithm iterates through the network enforcing bidirectional consistency:

If appending i causes C(j) to exceed the predefined MAX\_CANDIDATES limit, the limit for node j is temporarily relaxed to accommodate the symmetric link. This guarantees that the final Candidate Graph operates as a completely undirected topology, providing a safe, closed, and highly optimized environment for the Lin-Kernighan routing engine.

---
Phase 4: Tour Construction and the Variable k-opt Local Search Engine
With the global search space mathematically bounded by the undirected Candidate Graph, the Lin-Kernighan-Helsgaun (LKH) pipeline transitions into physical routing. The algorithm first constructs an initial, potentially suboptimal, valid tour (typically via a greedy depth-first search through the candidate edges). The objective then shifts to iteratively refining this sequence using the Lin-Kernighan variable-depth local search heuristic.

1. The Mathematical Foundation of k-opt Exchanges
Local search algorithms optimize a Traveling Salesperson Problem by executing continuous topological perturbations, known as edge exchanges.

Let T represent the current valid tour. A standard k-opt move involves identifying a set of exactly k edges currently in the tour, X = \{x_1, x_2, \dots, x_k\}, and severing them. This action fractures the continuous tour into k disjoint paths. To restore validity, the algorithm introduces a new set of k edges, Y = \{y_1, y_2, \dots, y_k\}, reconnecting the exposed endpoints into a new, closed tour, T'.
The exchange is executed if and only if the total spatial cost of the newly introduced edges is strictly less than the cost of the severed edges, yielding a positive geometric reduction (or "gain") in the total tour length:

Where C(e) denotes the distance cost of an edge e. If \Delta > 0, the move is accepted, T is replaced by T', and the optimization cycle restarts.

2. The Limitation of Fixed k-opt Algorithms
Traditional local search heuristics, such as 2-opt or 3-opt, operate with a static, pre-defined depth. A 2-opt algorithm only ever evaluates pairs of edges, while a 3-opt algorithm evaluates triplets.
The critical flaw in fixed-depth optimization is the phenomenon of local minima. A tour may reach a state where no possible combination of 2 or 3 edge swaps yields a positive \Delta. The algorithm terminates, assuming it has found the optimum. However, a highly complex 6-opt or 8-opt swap might possess the capacity to break the deadlock and achieve a shorter global distance. Attempting to hardcode a static 8-opt search is computationally impossible, as the branching factor for combinations of 8 edges across a large graph is astronomically high.

3. The Lin-Kernighan Innovation: Variable-Depth Sequential Exchange
The Lin-Kernighan (LK) algorithm bypasses the trap of static depths by introducing a variable k-opt sequential exchange. Instead of evaluating all k edges simultaneously, LK constructs the exchange dynamically, edge by edge, forming an alternating sequence of broken and added connections.
The algorithm builds an alternating path of edges:

At each depth level i:
The algorithm severs an existing tour edge x_i.
It appends a new candidate edge y_i.
It evaluates if the sequence can be closed into a valid tour by simulating the addition of a final closing edge, y_i^*.
If closing the tour at the current depth i yields a positive gain, the new tour is recorded as the best known solution. However, the algorithm does not stop. It continues plunging deeper into the search tree, exploring i+1, i+2, and beyond, dynamically evaluating k-opt moves of arbitrary complexity (often reaching depths of k=10 or more) to see if an even greater gain can be uncovered further down the sequential branch.

4. The Positive Gain Criterion (Bounding the Search Tree)
To prevent this recursive sequence from searching infinitely, Lin and Kernighan introduced a strict mathematical pruning rule known as the Positive Gain Criterion.
Let g_i be the individual gain of a single step in the sequence:

The cumulative gain G_i at any depth i is the sum of the individual step gains:

The algorithm is strictly permitted to continue deepening the search to i+1 if and only if the cumulative gain remains strictly positive (G_i > 0). This constraint guarantees that the intermediate, open path remains geometrically competitive. The moment G_i \le 0, the sequential exchange is mathematically proven to be a dead-end, and the recursive branch is immediately pruned.

5. Synergy with the Candidate Set
The true computational efficiency of the LKH-3 engine is realized in how Phase 4 interacts with Phase 3.
When the sequential exchange attempts to select a new edge y_i to append to the alternating path, it does not scan the global graph. The selection of y_i is strictly restricted to the edges residing in the current node's Candidate Set. By forcing the sequential exchange to only evaluate the top 5 edges dictated by the Held-Karp \alpha-matrix, the recursive branching factor is heavily suppressed. The Lin-Kernighan engine can dynamically plunge to massive k-opt depths in milliseconds because it only explores topological combinations that have mathematically survived the lower-bound regret filtering process.
—
Phase 5:
The Algorithmic Architecture of Variable-Depth Search: The Sequential Exchange

Traditional local search heuristics for the Traveling Salesperson Problem (TSP), such as 2-opt and 3-opt, operate within statically defined topological neighborhoods. A statically bounded search evaluates all possible combinations of exactly k edge exchanges. As k increases, the computational complexity scales exponentially as O(N^k), rendering arbitrary depths impossible to compute in dense networks. Consequently, static heuristics frequently converge prematurely, becoming trapped in local minima where no valid k-opt swap yields a positive geometric reduction, even if a higher-order swap (e.g., 8-opt or 12-opt) could escape the topological trap.

The Lin-Kernighan (LK) heuristic fundamentally resolves this structural bottleneck by replacing static neighborhood evaluation with a variable-depth sequential exchange. Rather than pre-defining the complexity of the swap, the algorithm constructs complex k-opt moves dynamically, allowing the search depth to adapt in real-time based on the mathematical promise of the traversal.

1. The Mechanics of the Alternating Path
The sequential exchange constructs a k-opt move edge-by-edge, forming an alternating sequence of severed connections and inserted candidates.
Let the current valid tour be T. The algorithm initiates a topological perturbation by selecting a base node t_1 and breaking an existing tour edge x_1 = (t_1, t_2). This fractures the tour into an open Hamiltonian path. To begin repairing the structure, the algorithm selects a new candidate edge y_1 = (t_2, t_3) to append to the sequence.

This process generalizes recursively. At any given depth level i, the algorithm extends the alternating path by:
Breaking a native tour edge x_i = (t_{2i-1}, t_{2i}).
Adding a new candidate edge y_i = (t_{2i}, t_{2i+1}).
This generates a sequential chain of alternating edges: x_1, y_1, x_2, y_2, \dots, x_i, y_i. Because the edges are selected sequentially rather than simultaneously, the combinatorial explosion associated with evaluating k independent edges at once is entirely bypassed.

2. Tour Closure and Evaluation at Depth i
At every depth iteration i \ge 2, the alternating path remains an open topological structure. To evaluate if the current sequence of swaps has uncovered a superior global minimum, the algorithm must simulate closing the path back into a valid tour T'.

This is achieved by mathematically connecting the final exposed node t_{2i+1} back to the initial origin node t_1 via a closing edge, denoted as y_i^* = (t_{2i+1}, t_1).

The total reduction in distance, or the "gain," for this dynamically closed i-opt move is calculated as:
\begin{equation}
\Delta_i = \sum_{j=1}^{i} C(x_j) - \sum_{j=1}^{i-1} C(y_j) - C(y_i^*)
\end{equation}
If \Delta_i > 0, the newly constructed tour T' is strictly shorter than T and is recorded as the new incumbent best solution. However, unlike static algorithms, the LK engine does not terminate upon finding a positive closure. It records the improvement but continues the recursive plunge to depth i+1, exploring whether a deeper, more complex sequence might yield an even greater \Delta.

3. The Positive Gain Criterion: Bounding the Recursion
To prevent the sequential exchange from devolving into an infinite depth-first search, Lin and Kernighan established a rigorous mathematical pruning mechanism known as the Positive Gain Criterion.
The algorithm evaluates the cumulative partial gain, G_i, of the open alternating path (explicitly excluding the hypothetical closing edge y_i^*):
\begin{equation}
G_i = \sum_{j=1}^{i} C(x_j) - \sum_{j=1}^{i} C(y_j)
\end{equation}
The fundamental condition for extending the search to depth i+1 is that the current open sequence must maintain a strictly positive cumulative gain:
\begin{equation}
G_i > 0
\end{equation}
If G_i \le 0, the sequence has accrued too much geometric penalty, and it is mathematically impossible for subsequent edge additions to recover the deficit. The moment this criterion is violated, the current branch of the search tree is instantly pruned. This aggressive bounding ensures that the engine only expends computational cycles on topological trajectories that remain highly competitive.

4. Backtracking and the LKH-3 Extension
While the original LK heuristic restricted sequential moves to simple alternating paths, Keld Helsgaun's LKH framework enhances this architecture by incorporating broad backtracking mechanisms.
If a sequence fails to yield an improved tour and triggers the Positive Gain limit, LKH-3 systematically backtracks up the search tree, substituting alternative candidate edges (e.g., swapping y_i for a different local candidate) to explore parallel branches. Furthermore, because the selection of y_i is governed exclusively by the sparse \alpha-nearness Candidate Sets from Phase 3, this variable-depth engine can effortlessly execute sequential exchanges exceeding depths of k=50. This synthesis of \alpha-filtering and variable-depth recursion grants the algorithm an unparalleled capacity to escape deep local minima and isolate the true global optimum.

