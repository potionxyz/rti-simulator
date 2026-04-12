# Architecture & System Design

So before writing any C++, I needed to design the actual system. Radio Tomographic Imaging (RTI) is fundamentally a massive data pipeline: measure signals $\rightarrow$ build a model of the room $\rightarrow$ solve an inverse problem $\rightarrow$ visualise the result.

If the architecture is messy, the maths becomes impossible to debug. I went with a modular, Object-Oriented approach. Here is the breakdown.

## High-Level Data Flow

The simulator works in four distinct stages. If this were a real hardware deployment, Stage 1 and 2 would be replaced by physical ESP32 microcontrollers pinging each other, but the math from Stage 3 onwards is identical.

1.  **Scene Generation:** Define the 3D room, place walls, and insert a "human" (a region of high radio attenuation).
2.  **Forward Simulation:** Ray-trace radio signals between virtual nodes to calculate how much signal is lost (attenuated) by the environment. This generates synthetic RSS (Received Signal Strength) measurements.
3.  **Reconstruction (The Inverse Problem):** Take *only* the signal measurements and the node coordinates, and mathematically rebuild the 3D map of the room to find the human.
4.  **Visualisation:** Export the 3D tensor data into something human-readable.

## C++ Class Architecture

I broke the system down into five core classes to keep memory management tight and responsibilities isolated.

### 1. `VoxelGrid` (The Spatial Map)
This is the lowest-level data structure. Instead of dealing with continuous 3D space (which is impossible to compute on the fly), discrete 3D cubes called "voxels" are used.

*   **Role:** Store the attenuation values of the physical space.
*   **Key Decision:** I avoided using `std::vector<std::vector<std::vector<float>>>` (a true 3D array). Why? Because nested vectors scatter memory across the heap, destroying CPU cache locality. When iterating over 27,000 voxels, cache misses will tank performance.
*   **Implementation:** I used a single, flat 1D `std::vector<float>` and mapped 3D coordinates $(x,y,z)$ to a 1D index using $i = x + (y \times N_x) + (z \times N_x \times N_y)$. This guarantees contiguous memory allocation.

### 2. `Node` & `Scene` (The Environment)
*   `Node`: A simple struct representing an ESP32 transceiver's $(x,y,z)$ coordinate in the room.
*   `Scene`: Orchestrates the setup. It holds the `VoxelGrid` and the list of `Node`s. It includes methods to insert "obstacles" (like walls or humans) by modifying specific blocks of voxels in the grid.

### 3. `ForwardModel` (The Simulator)
This class generates the synthetic data. It takes a `Scene` and simulates what the nodes would actually "hear".

*   **Role:** Calculate the baseline signal strength between empty nodes, then calculate the actual signal strength when obstacles are present.
*   **The Algorithm:** It uses **Siddon's Method** for line-voxel intersections. This is a classic ray-tracing algorithm from medical CT scanning. It calculates exactly how much of a radio wave's path intersects with a specific voxel.
*   **Output:** Generates the measurement vector $\mathbf{y}$ (the change in signal strength on every link).

### 4. `Reconstructor` (The Solver)
This is the mathematical core. All it knows is where the nodes are and the measurement vector $\mathbf{y}$. It has to guess the `VoxelGrid`.

*   **Role:** Solve the system $\mathbf{Wy} = \mathbf{x}$.
*   **The Weight Matrix ($\mathbf{W}$):** This matrix maps how every voxel affects every link. If we have 20 links and 10,000 voxels, $\mathbf{W}$ is a $20 \times 10,000$ matrix. Most voxels don't intersect most links, so this matrix is mostly zeros.
*   **Key Decision:** I used the `Eigen` C++ library's `SparseMatrix`. Storing a dense matrix of that size would blow up RAM instantly. Eigen handles sparse algebra incredibly efficiently.
*   **The Solver:** RTI is an ill-posed inverse problem (we have way more voxels than measurements). Standard least-squares fails. I implemented **Tikhonov Regularisation**, which adds a penalty term to smooth out the noise and find a stable solution.

### 5. `Visualiser` (The Output)
*   **Role:** Convert the reconstructed `VoxelGrid` into a 3D point cloud or 2D slice images.
*   **Implementation:** Exports the data into formats that can be read by external plotters or my web-based 3D viewer.

## Why C++ over Python?

For a mathematical prototype, Python (with NumPy) is the standard choice. So why did I build this in C++?

1.  **Memory Control:** RTI creates massive matrices. Python abstracts memory management away, which can lead to unpredictable RAM spikes and slow garbage collection when solving large inverse problems. C++ let me enforce cache-locality with flat vectors and strictly manage heap allocations.
2.  **Hardware Proximity:** The end goal of this project is to run on physical ESP32 microcontrollers (which are programmed in C/C++). By writing the core logic in C++, the mathematical backend is already highly portable to edge devices if I want to scale this beyond a simulator.
3.  **Performance:** Generating the weight matrix requires millions of ray-voxel intersection checks. C++ executes Siddon's algorithm orders of magnitude faster than a Python loop. Eigen's compiled sparse solvers are also brutally fast.

So basically, C++ forced me to understand the hardware implications of my algebra, rather than just calling `np.linalg.solve` and hoping for the best. If that makes sense.