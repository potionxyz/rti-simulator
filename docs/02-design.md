# Design

## 1. system architecture

the simulator is built as a pipeline of five modules, each handling one stage of the RTI process. data flows forward through the pipeline, and each module can be tested independently.

```
  Scene Definition    Node Network
       |                  |
       v                  v
  +----------+    +-----------+
  | VoxelGrid |<---| NodeNetwork|
  +----------+    +-----------+
       |                |
       v                v
  +-------------------+
  |   Forward Model   |
  | (Siddon's trace)  |
  +-------------------+
       |
       v
   measurements (y)
       |
       v
  +-------------------+
  |   Reconstructor   |
  | (Tikhonov/BP)     |
  +-------------------+
       |
       v
  +-------------------+
  |    Visualiser     |
  | (ASCII/PPM/HTML)  |
  +-------------------+
```

## 2. data flow

1. **input**: room dimensions, voxel size, node positions, obstacle definitions
2. **voxel grid**: 3D space discretised into N voxels, stored as flat vector x
3. **weight matrix**: W computed via Siddon's algorithm (sparse, links x voxels)
4. **measurements**: y = Wx + noise (one value per link)
5. **reconstruction**: x_hat recovered from y using regularised inversion
6. **output**: ASCII slices, PPM images, interactive 3D HTML viewer

## 3. class design

### VoxelGrid
- stores 3D attenuation data as `std::vector<double>` (flat, cache-friendly)
- provides (i,j,k) <-> flat index conversion
- maps directly to column vector x in the reconstruction equation

### Node / Link / NodeNetwork
- Node: sensor position (x, y, z) with ID
- Link: connection between two nodes with baseline and current RSS
- NodeNetwork: manages nodes and auto-generates N*(N-1)/2 unique links
- preset configurations: 2D square (4 nodes), 3D cube (8 nodes), custom perimeter

### Scene / Obstacle
- Obstacle: bounding box with attenuation coefficient
- Scene: collection of obstacles, stamps onto VoxelGrid
- convenience methods for common objects (person, wall)
- JSON serialisation for reproducible testing

### ForwardModel
- computes weight matrix W using Siddon's ray-voxel intersection algorithm
- W stored as Eigen::SparseMatrix (99.8% of entries are zero)
- simulates measurements with configurable Gaussian noise
- mathematically: y = Wx + noise

### Reconstructor
- three methods implemented:
  - back-projection: x = W^T * y (fast, blurry)
  - least-squares: x = (W^T W)^{-1} W^T y (unstable)
  - Tikhonov: x = (W^T W + lambda*I)^{-1} W^T y (best)
- evaluation metrics: MSE, localisation error, TP/FP/FN
- auto-thresholding for binary detection

### Visualiser
- ASCII terminal output (density characters)
- PPM image export (heat map colour mapping)
- HTML/Three.js interactive 3D viewer

## 4. algorithm selection and justification

### why Siddon's algorithm for ray tracing?
Siddon's method (1985) efficiently computes ray-voxel intersections in O(nx + ny + nz) time per ray. alternatives like brute-force checking every voxel would be O(nx * ny * nz). for a 20x20x20 grid, that's 60 vs 8000 operations per link. with 28 links, the difference is 1,680 vs 224,000 operations.

the algorithm works by finding where the ray crosses each grid plane, sorting these crossing points, and stepping through them. each interval between crossings corresponds to the ray being inside one voxel.

### why Tikhonov regularisation?
the core problem y = Wx is underdetermined: 28 equations, 1000+ unknowns. the matrix W^T W is rank-deficient (rank <= 28), so direct inversion fails. Tikhonov adds lambda*I to W^T W, making it positive definite and invertible.

the trade-off controlled by lambda:
- small lambda: trusts measurements, but amplifies noise
- large lambda: smooth solution, but loses spatial detail
- optimal lambda: balances data fit and solution stability

i chose Tikhonov over other regularisation methods (LASSO, total variation) because:
1. it has a closed-form solution (no iterative optimisation needed)
2. the maths is transparent and explainable
3. it's the standard method in the RTI literature (Wilson & Patwari 2010)

### why C++ over Python?
1. **performance**: matrix operations on 1000+ voxels benefit from compiled code
2. **memory control**: understanding how data is laid out in memory (flat vector vs 3D array) is essential for EIE
3. **embedded relevance**: the real system would run on ESP32 microcontrollers, which use C++
4. **demonstrates competence**: choosing the harder language shows technical confidence

### why flat vector storage?
a 3D array (`double grid[nx][ny][nz]`) scatters data across memory. a flat `std::vector<double>` stores everything contiguously, which means:
- better CPU cache performance (sequential access pattern)
- direct mapping to Eigen vectors for matrix operations
- simpler serialisation for file I/O

## 5. mathematical derivations

### forward model
each link measurement is the line integral of attenuation along the link path:

```
y_i = integral of x(r) dr along link i
```

discretised into voxels:

```
y_i = sum_j (w_ij * x_j)
```

where w_ij is the length of link i that passes through voxel j.

in matrix form: **y = Wx**

### Tikhonov reconstruction
minimise: ||Wx - y||^2 + lambda * ||x||^2

taking the derivative and setting to zero:

```
d/dx [||Wx - y||^2 + lambda * ||x||^2] = 0
2 W^T (Wx - y) + 2 lambda x = 0
(W^T W + lambda I) x = W^T y
x = (W^T W + lambda I)^{-1} W^T y
```

### weight matrix sparsity
with N nodes, there are N*(N-1)/2 links. each link passes through approximately sqrt(nx^2 + ny^2 + nz^2) voxels. for 28 links and a 10x10x10 grid, the weight matrix is 28 x 1000 with ~560 non-zero entries out of 28,000 total (2% density). sparse storage reduces memory from 224KB to ~13KB.
