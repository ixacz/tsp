# Custom Traveling Salesman Problem (TSP) Solver 

A simplified C-based engine for solving the Traveling Salesman Problem (TSP)
using Lin-Kernighan heuristic and Held-Karp relaxation.

## Getting the source code from Github
To get the source of this project, run the following command:

```bash
git clone https://github.com/ixacz/tsp.git
```

## Compling the source file
To compile this project, run:

```bash
make build
```

## Unzipping the .zip file
Make sure you have tne `unzip` utility

```bash
unzip ./travelling-salesman-probem.zip -d tsp
```

## How to run the program?
1- Choose the dataset file (syria40.tsp or kroA100.tsp)
2- make sure it is in the same directory where you run the program.
3- change the code to load that file:

```c
...
load_cities_from_file(&ctx, "./filename.tsp");
...
```

4- Change the best optimal solution macro at the top of the tsp.c file
    to match your file name:

```c
...
#define KROA100_OPTIMAL 21282.0
#define SY40_OPTIMAL 2414824 

// Change for the coresponding input file.
#define BEST_TOUR_SOLUTION SY40_OPTIMAL
...
```

5- run the program:

```bash
./bin/tsp-bin-name
```
