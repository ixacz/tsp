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

