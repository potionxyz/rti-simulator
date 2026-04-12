# 01-voxel-maths: 3d space in 1d memory

so before we can simulate radio waves bouncing around a room, we need to actually build the room in code. but computers don't really understand continuous 3D space.

imagine trying to describe exactly where a fly is in a room. you could use endless decimal places: it's at `x=1.234567...`, `y=0.98765...`, etc. that takes too much memory and processing power to simulate.

instead, we do what minecraft does.

we cut the room up into a grid of cubes. we call these "voxels" (volumetric pixels). if the room is 3 metres by 3 metres by 3 metres, and we want 10cm resolution, we fill the room with little 10cm x 10cm x 10cm invisible cubes.

* **x-axis:** 3.0m / 0.1m = 30 cubes
* **y-axis:** 30 cubes
* **z-axis:** 30 cubes
* **total:** 30 * 30 * 30 = 27,000 cubes.

each one of these cubes just holds a single decimal number (a `float` in C++). this number represents "radio attenuation" - basically, how much does whatever is inside this cube block a wifi signal? air is near 0. a brick wall is a high number. a human (mostly water) is somewhere in between.

## the problem with a 3d array

if you're just starting programming, the obvious way to store a grid of cubes is a 3D array or a nested list.
in python, it looks like `grid[x][y][z]`.
in C++, it looks like `std::vector<std::vector<std::vector<float>>> grid;`

**never do this in high-performance code.**

why? imagine a massive library.
a 1D array is like a single, massive bookshelf that stretches for a mile. you walk down it, reading book after book. it's incredibly fast.

a nested 3D array is like having one bookshelf that just holds cards. the card tells you to walk to a completely different building to find the next bookshelf. that bookshelf has another card, pointing to a third building.

in computer terms, scattered memory causes "cache misses". the CPU has to keep stopping to fetch data from slow RAM instead of its ultra-fast L1 cache. when you have to loop over 27,000 voxels thousands of times a second to simulate radio waves, 3D arrays will completely freeze your program.

## the solution: flattening the room

we take our 3D room of 27,000 cubes, and we squash it into a single, straight line of 27,000 numbers.
a 1D array. `std::vector<float> grid(27000);`

but now we have a maths problem: if i want to find the voxel at 3D coordinate `(x=5, y=10, z=2)`, where exactly is that in my massive 1D list of 27,000 numbers?

we need a formula to map 3D to 1D.

### the mapping formula (explained simply)

let's shrink the room. imagine a tiny 3x3x3 rubik's cube.
how many blocks total? 3 * 3 * 3 = 27.

let's say we number them from 0 to 26.
we count along the bottom row first (X). when we hit the end of the row, we step back and start the next row (Y). when we finish the whole bottom floor (a 2D slice), we move up one level (Z) and start again.

* `N_x` = total width (3)
* `N_y` = total depth (3)
* `N_z` = total height (3)

if i want block `(x=2, y=1, z=1)`:

1. **z=1:** this means i need to skip the entire bottom floor. one floor is `N_x * N_y` blocks (3*3 = 9 blocks). so skip 9.
2. **y=1:** i'm on the middle floor now. i need to skip one full row to get to `y=1`. a row is `N_x` blocks (3 blocks). so skip 3.
3. **x=2:** move along the current row by 2.

total skips: 9 + 3 + 2 = 14.
the block is at index 14 in our flat 1D list.

**the universal formula:**
$$ i = x + (y \times N_x) + (z \times N_x \times N_y) $$

### let's try it (worked example)

we have a room that is **10 blocks wide (X), 10 blocks deep (Y), and 5 blocks high (Z).**
we want the block at **(x=4, y=7, z=2)**.

what is its 1D index?

1.  **move Z:** skip 2 whole floors. one floor is $10 \times 10 = 100$ blocks. $2 \times 100 = 200$.
2.  **move Y:** skip 7 rows. one row is 10 blocks. $7 \times 10 = 70$.
3.  **move X:** move 4 blocks.

index $i = 4 + 70 + 200 = 274$.
it lives at `grid[274]`.

---

## reversing it: 1D back to 3D

sometimes we know the index (like `427`) and we need to know where that is in the 3D room. we do the formula backwards using integer division (where we ignore remainders) and modulo (`%`, which only gives the remainder).

using our 10x10x5 room again. where is index `347`?

**step 1: find Z (the floor)**
how many whole floors fit into 347? one floor is 100 blocks.
$347 / 100 = \text{3 whole floors}$.
so, **z = 3**.

what's left over? the remainder of $347 \div 100$ is 47.
we have 47 blocks to place on the 3rd floor.

**step 2: find Y (the row)**
we have 47 blocks. how many full rows of 10 fit into 47?
$47 / 10 = \text{4 whole rows}$.
so, **y = 4**.

what's left over? the remainder of $47 \div 10$ is 7.

**step 3: find X (the column)**
the remainder is just our X position.
so, **x = 7**.

coordinate: **(7, 4, 3)**.

### your turn (exercise)

room size: $N_x = 5$, $N_y = 5$, $N_z = 5$.
find the 1D index for coordinate **(x=2, y=3, z=4)**.

*work it out on paper before scrolling down.*

.
.
.

**answer:**
floor size = $5 \times 5 = 25$.
$i = 2 + (3 \times 5) + (4 \times 25)$
$i = 2 + 15 + 100 = 117$.

## how it looks in c++

in `src/voxel_grid.cpp`, i wrote it exactly like this. it's incredibly short, but it's the mathematical backbone of keeping the simulator fast.

```cpp
// 3D to 1D mapping
int VoxelGrid::GetIndex(int x, int y, int z) const {
    
    // safety check: if we try to access outside the room, the program will crash.
    if (x < 0 || x >= nx_ || y < 0 || y >= ny_ || z < 0 || z >= nz_) {
        return -1; 
    }
    
    // the formula
    return x + (y * nx_) + (z * nx_ * ny_);
}
```

we now have a room built entirely from a single, ultra-fast line of memory. next, we need to shoot radio waves through it.