# 01-voxel-maths: 3d space in 1d memory

so before we can simulate radio waves propagating through a room, we need to construct the room in code. dealing with continuous 3D space computationally is impossible—you can't simulate finite element analysis on a space with infinite coordinate precision. 

instead, we discretise the room into a uniform grid of volumetric pixels, or "voxels". 

> **note on scale throughout this module:**
> i work through this whole chapter using a reference scenario of a **3m×3m×3m room at 10cm resolution (27,000 voxels)**. that's the physically realistic target for a production ESP32 deployment in a living room.
>
> the *committed simulator in this repo* actually runs at a smaller scale: **2m×2m×2m at 20cm (1,000 voxels)**. this is because at 27,000 voxels, the $\mathbf{W}^T\mathbf{W}$ matrix is $27{,}000 \times 27{,}000$ which is impractical for the direct Cholesky solver i use; you'd need iterative methods like Conjugate Gradient (see `docs/05-evaluation.md` for that trade-off). the maths is identical in both cases — only the matrix dimensions change.
>
> when you see "27,000" below, read it as the theoretical full-scale case. when you see "1,000", that's what actually runs when you type `./rti-simulator`.

think of it like finite integration. if we have a room measuring 3m $\times$ 3m $\times$ 3m, and we want 10cm resolution, we partition the space into $0.1\text{m}^3$ volumes.

* **x-axis (width):** $3.0\text{m} / 0.1\text{m} = 30$ voxels
* **y-axis (depth):** 30 voxels
* **z-axis (height):** 30 voxels
* **total volume:** $30 \times 30 \times 30 = 27,000$ voxels.

each voxel stores a single scalar value, represented as a `float` in C++. this value represents the radio attenuation coefficient of whatever occupies that physical volume. air has an attenuation close to 0. a human body (high water content, high dielectric constant) absorbs RF energy, so its attenuation is much higher.

## the problem with a 3d array

the intuitive, naive way to store this grid in C++ is a nested vector:
`std::vector<std::vector<std::vector<float>>> grid;`

**never do this in high-performance computing.**

why? memory allocation. a nested `std::vector` dynamically allocates memory on the heap. the vectors holding the Y and Z coordinates are not guaranteed to be physically near each other in RAM. 

this destroys cache locality. modern CPUs load data from RAM into ultra-fast L1/L2 caches in blocks (cache lines). if your data is contiguous in memory, the CPU pulls in the current voxel and the next 15 voxels simultaneously. if you use a nested 3D vector, the CPU hits a pointer, has to jump to a random address in RAM to find the next voxel, and flushes the cache. 

when we run the forward simulation, we have to iterate over these 27,000 voxels millions of times. cache misses will bottleneck the algorithm entirely.

## the solution: linear mapping

the standard solution in graphics and tensor programming is to flatten the 3D space into a single, contiguous 1D array.

we allocate the memory once: `std::vector<float> grid(27000);`
this guarantees the entire room sits in a single, unbroken block of RAM.

but now we need a mathematical bijective mapping. if the forward model needs to check the attenuation at 3D coordinate $(x, y, z)$, where exactly does that value live in our 1D index $i$?

### the universal mapping formula

let:
* $N_x$ = total voxels along the x-axis
* $N_y$ = total voxels along the y-axis
* $N_z$ = total voxels along the z-axis

we map the coordinates using row-major order. we iterate across the x-axis. when we reach $N_x$, we increment $y$ and start a new row. when we fill a 2D slice ($N_x \times N_y$), we increment $z$ and start a new slice.

the formula to find index $i$ is:
$$ i = x + (y \times N_x) + (z \times N_x \times N_y) $$

#### worked example 1: finding the memory address

assume a smaller test grid: $10 \times 10 \times 5$ ($N_x=10, N_y=10, N_z=5$).
we want to query the voxel at coordinate $(x=4, y=7, z=2)$.

1.  **z-offset:** we skip 2 full 2D slices. one slice is $10 \times 10 = 100$ voxels. skipping 2 slices gives $2 \times 100 = 200$.
2.  **y-offset:** we skip 7 full rows within the current slice. one row is 10 voxels. skipping 7 rows gives $7 \times 10 = 70$.
3.  **x-offset:** we move 4 columns along the current row.

$$i = 4 + 70 + 200 = 274$$
the attenuation value is located exactly at `grid[274]`.

---

## the inverse mapping: 1d back to 3d

when we run the inverse problem (reconstruction), the solver outputs a 1D vector of results. to visualise this, we need to map the 1D index $i$ back to its 3D spatial coordinate $(x,y,z)$. 

we achieve this using integer division and the modulo operator.

using the same $10 \times 10 \times 5$ grid, let's reverse index $347$.

**step 1: find z**
how many full 2D slices ($N_x \times N_y$) fit into 347?
$$z = \lfloor 347 / 100 \rfloor = 3$$
the remainder is exactly the index *within* that z-slice.
$$\text{remainder} = 347 \pmod{100} = 47$$

**step 2: find y**
using the remainder (47), how many full rows ($N_x$) fit into it?
$$y = \lfloor 47 / 10 \rfloor = 4$$
the new remainder is the position within that row.
$$\text{remainder} = 47 \pmod{10} = 7$$

**step 3: find x**
the final remainder is explicitly the x-coordinate.
$$x = 7$$

the 3D coordinate is $(7, 4, 3)$.

### your turn (exercise)

grid dimensions: $N_x = 20$, $N_y = 20$, $N_z = 10$.
calculate the 1D index for the voxel at $(x=15, y=5, z=8)$.

.
.
.

**answer:**
slice size = $20 \times 20 = 400$.
$i = 15 + (5 \times 20) + (8 \times 400)$
$i = 15 + 100 + 3200 = 3315$.

## c++ implementation

in `src/voxel_grid.cpp`, the implementation is incredibly clean, but handles the core memory bounds. 

```cpp
int VoxelGrid::GetIndex(int x, int y, int z) const {
    // bounds checking prevents segmentation faults if the ray-tracer 
    // overshoots the volume boundaries.
    if (x < 0 || x >= nx_ || y < 0 || y >= ny_ || z < 0 || z >= nz_) {
        return -1; 
    }
    
    // the linear mapping mapping
    return x + (y * nx_) + (z * nx_ * ny_);
}
```

with the spatial memory architecture defined, we can move to the physics engine: simulating the propagation and attenuation of RF signals through this volume using Siddon's algorithm.
