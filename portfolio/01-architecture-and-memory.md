# 01-architecture-and-memory

so before writing any actual C++, i had to sit down and design the system architecture. radio tomographic imaging (RTI) is fundamentally just a massive data pipeline: measure signals -> build a mathematical model of the room -> solve a huge inverse problem matrix -> spit out a 3D image.

if the architecture underneath is messy, the maths is impossible to debug later. i went with a strictly modular, object-oriented approach. here's how the pieces fit together.

## high-level data flow

the simulator runs in four distinct stages. if this was a real physical deployment (which is the next step), stages 1 and 2 would just be the hardware ESP32s pinging each other. but the maths from stage 3 onwards is identical.

1.  **scene generation:** define a 3D bounding box for the room, drop in walls, and insert a "human" (basically a region of high radio attenuation).
2.  **forward simulation:** ray-trace radio signals between virtual nodes to calculate how much signal is lost (attenuated) by the environment. this generates synthetic RSS (received signal strength) measurements.
3.  **reconstruction (the inverse problem):** the core engine. it takes *only* the signal measurements and the physical coordinates of the nodes, and mathematically reconstructs the 3D map of the room to locate the human.
4.  **visualisation:** export the final 3D tensor data into something i can actually look at to see if the localisation worked.

## the C++ class structure

i broke the system down into five main classes to isolate responsibilities and keep memory management tight.

### 1. `VoxelGrid` (the spatial map)
this is the lowest-level data structure. computing continuous 3D space is basically impossible in real-time, so the room is discretised into 3D cubes called voxels.

*   **role:** store the physical attenuation values of the space.
*   **key decision:** what's the best way to store a 3D grid? the obvious answer is a 3D array: `std::vector<std::vector<std::vector<float>>>`. i explicitly didn't do this. nested vectors scatter memory dynamically across the heap. when you need to iterate over up to 27,000 voxels to build a weight matrix (or 1,000 at the actual scale i ended up running), those cache misses will absolutely tank performance.
*   **the fix:** i used a single, flat 1D `std::vector<float>`, allocating all memory contiguously upfront. i then mapped 3D coordinates $(x,y,z)$ to a 1D index using $i = x + (y \times N_x) + (z \times N_x \times N_y)$. it’s a standard graphics programming trick, but it makes the memory footprint incredibly tight and fast.

### 2. `Node` & `Scene` (the environment)
*   `Node`: a lightweight struct representing an ESP32 transceiver's coordinate in the room.
*   `Scene`: orchestrates the setup. it holds the `VoxelGrid` and the list of `Nodes`. it has methods to insert objects (like a person) by modifying blocks of voxels in the grid. fairly standard stuff.

### 3. `ForwardModel` (the physics engine)
this generates the synthetic data. it takes a `Scene` and simulates what the nodes would actually measure in real life.

*   **role:** calculate the baseline signal strength (empty room), then calculate the attenuated signal strength when obstacles are present.
*   **the algorithm:** it uses Siddon's Method for line-voxel intersections. it’s an old ray-tracing algorithm from medical CT scanning. it calculates exactly the path length of a radio wave passing through any given voxel.
*   **output:** generates the measurement vector $\mathbf{y}$ (the change in signal strength across every link).

### 4. `Reconstructor` (the solver)
this is where the heavy lifting happens. it knows nothing about the `Scene` except the node locations and the $\mathbf{y}$ vector. it has to guess what the `VoxelGrid` looks like.

*   **role:** solve the linear system $\mathbf{Wy} = \mathbf{x}$.
*   **the weight matrix ($\mathbf{W}$):** this matrix maps how every voxel affects every link. for an 8-node setup in a high-res grid, this matrix gets massive fast. most voxels don't intersect most links, meaning the matrix is mostly zeros.
*   **key decision (eigen):** storing a dense matrix of that size in memory is a bad idea. i pulled in the `Eigen` C++ library specifically for its `SparseMatrix` type. it only stores non-zero values, saving massive amounts of RAM and speeding up the matrix multiplication during reconstruction.
*   **the solver:** RTI is a severely ill-posed inverse problem. we have way more unknown voxels than we have link measurements. standard least-squares fails immediately. i had to implement Tikhonov Regularisation, which adds a penalty term to smooth out the noise and force a stable mathematical solution.

### 5. `Visualiser` (the output)
*   **role:** convert the reconstructed `VoxelGrid` into a raw 3D point cloud or 2D slice images.
*   **implementation:** exports data (like PPM images for slices) that i can feed into the web viewer i built.

## why C++ over python?

for a mathematical prototype like this, python and NumPy are the industry standard. it's way faster to write. so why did i build it in C++?

1.  **memory contiguity:** inverse problems create massive matrices. python completely abstracts memory management. this leads to unpredictable RAM spikes and slow garbage collection when solving large systems. C++ allowed me to enforce cache-locality with flat vectors and strictly manage heap allocations. i control where the data lives.
2.  **hardware proximity:** the end goal here is an actual hardware deployment on physical ESP32s (which are programmed in C/C++). by writing the core logic in C++ now, the mathematical backend is already highly portable to edge devices later.
3.  **execution speed:** building the weight matrix requires millions of ray-voxel intersection checks. C++ executes Siddon's algorithm orders of magnitude faster than a python loop ever could. Eigen's compiled sparse solvers are also incredibly fast.

basically, picking C++ forced me to understand the actual hardware and memory implications of my algebra, rather than just calling `np.linalg.solve` and hoping python figured it out in the background. if that makes sense.