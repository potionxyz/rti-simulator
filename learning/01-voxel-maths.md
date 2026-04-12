# 01-Voxel-Maths: 3D Spaces in 1D Memory

Before we simulate radio waves, we need a room. A continuous 3D room is impossible to simulate exactly, so we discretise it into cubes called "voxels" (volumetric pixels).

## The Concept

Imagine a $3\text{m} \times 3\text{m} \times 3\text{m}$ room. If we want $10\text{cm}$ resolution, we divide the room into $0.1\text{m}$ cubes. 
*   **X-axis:** $3.0 / 0.1 = 30$ voxels
*   **Y-axis:** $30$ voxels
*   **Z-axis:** $30$ voxels
Total volume = $30 \times 30 \times 30 = 27,000$ voxels.

Each voxel stores a single `float`: its radio attenuation (how much it blocks a WiFi signal). Air is near 0. Water/flesh is higher. Walls are very high.

## The Problem: 3D Arrays vs 1D Vectors

The intuitive way to store this in C++ is a 3D array or nested vector: `std::vector<std::vector<std::vector<float>>> grid;`

**Do not do this.**

Nested vectors allocate memory dynamically in scattered chunks across your computer's RAM. When you iterate through a 3D grid natively, the CPU cache completely misses, forcing it to fetch from slow main memory constantly. In high-performance C++, cache-locality is king.

**The Solution:** Flatten the 3D grid into a single 1D `std::vector<float> grid;` initialized with `27000` elements.

## The Mathematical Mapping

This requires mapping a 3D coordinate $(x, y, z)$ into a single 1D index $i$.

Let:
*   $N_x$ = number of voxels in the X dimension
*   $N_y$ = number of voxels in the Y dimension
*   $N_z$ = number of voxels in the Z dimension

If we iterate X first, then Y, then Z (Row-Major Order), the formula to find the 1D index $i$ given a 3D coordinate $(x, y, z)$ is:

$$i = x + (y \times N_x) + (z \times N_x \times N_y)$$

### Worked Example

We have a $5 \times 5 \times 5$ grid ($N_x=5, N_y=5, N_z=5$).
We want to access the voxel at $(x=2, y=3, z=1)$.

1.  Move along X: $+2$
2.  Move along Y: $+3$ rows of 5 ($3 \times 5 = 15$)
3.  Move along Z: $+1$ full 2D slice ($1 \times 5 \times 5 = 25$)

$i = 2 + (3 \times 5) + (1 \times 25)$
$i = 2 + 15 + 25 = 42$

The voxel is at `grid[42]`.

### The Inverse Function

To go backward (1D index $i$ back to 3D coordinate $x, y, z$), we use modulo and integer division. This is useful for reconstruction visualization.

*   $z = i / (N_x \times N_y)$   (Integer division drops the remainder)
*   $\text{remainder} = i \pmod{N_x \times N_y}$
*   $y = \text{remainder} / N_x$
*   $x = \text{remainder} \pmod{N_x}$

Let's test it on index 42:
*   $z = 42 / 25 = \mathbf{1}$
*   $\text{remainder} = 42 \pmod{25} = 17$
*   $y = 17 / 5 = \mathbf{3}$
*   $x = 17 \pmod{5} = \mathbf{2}$

Result: $(2, 3, 1)$. The math holds.

## Your Try (Mental Exercise)

You have a $10 \times 10 \times 10$ grid. 
You are looking at index 347 in the flat vector. What are its $(x, y, z)$ coordinates? (Solve before reading ahead).

.
.
.

**Answer:**
$z = 347 / 100 = 3$
Remainder = $347 \pmod{100} = 47$
$y = 47 / 10 = 4$
$x = 47 \pmod{10} = 7$

Coordinate: $(7, 4, 3)$.

## C++ Implementation (See: `src/voxel_grid.cpp`)

```cpp
// Constructor
VoxelGrid::VoxelGrid(float width, float height, float depth, float voxel_size) 
    : nx_(std::ceil(width / voxel_size)), 
      ny_(std::ceil(height / voxel_size)), 
      nz_(std::ceil(depth / voxel_size)) {
    // Single allocation. Fast. Contiguous.
    grid_.resize(nx_ * ny_ * nz_, 0.0f); 
}

// 3D to 1D
int VoxelGrid::GetIndex(int x, int y, int z) const {
    // Bounds checking is crucial to prevent segfaults
    if (x < 0 || x >= nx_ || y < 0 || y >= ny_ || z < 0 || z >= nz_) return -1;
    return x + (y * nx_) + (z * nx_ * ny_);
}
```

## Why Imperial Cares
If an interviewer asks "Why a 1D vector for a 3D grid?", answering "To optimise CPU L1/L2 cache hits through contiguous memory allocation" is an instant green flag. It shows you think like a C++ systems engineer, not just a mathematician.