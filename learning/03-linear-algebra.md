# 03-linear-algebra: casting physics into matrices

in the previous section, we established that the signal loss $\Delta y_i$ on a single link $i$ is the sum of the attenuation $x_j$ in each voxel $j$, multiplied by the distance $W_{ij}$ the signal travels through that voxel.

$$ \Delta y_i = W_{i,1}x_1 + W_{i,2}x_2 + \dots + W_{i,N}x_N $$

you will immediately recognise this as a linear equation. 

if we have an 8-node sensor network, we have $8 \times 7 / 2 = 28$ unique line-of-sight links. therefore, we have 28 simultaneous linear equations. 

we can write this elegantly using matrix notation:

$$ \mathbf{y} = \mathbf{Wx} $$

*   $\mathbf{y}$ is a column vector of length $M$ (the 28 link measurements).
*   $\mathbf{x}$ is a column vector of length $N$ (the 27,000 voxel unknown attenuations).
*   $\mathbf{W}$ is an $M \times N$ matrix (28 rows by 27,000 columns). this is our "weight matrix" or "system matrix".

every row in $\mathbf{W}$ represents a single physical radio link. every column represents a single physical voxel in the room. the value at $W_{i,j}$ is the distance link $i$ travels through voxel $j$.

## the sparse matrix problem

we have a mathematical model. the immediate C++ instinct is to declare a 2D array or vector to hold $\mathbf{W}$: `std::vector<std::vector<float>> W(M, std::vector<float>(N));`.

let's look at the physics again. a straight line passing through a $30 \times 30 \times 30$ voxel grid will only intersect roughly 30 to 50 voxels. 

this means out of the 27,000 columns in a single row of $\mathbf{W}$, only about 50 contain actual non-zero floating-point numbers. the remaining 26,950 columns are exactly exactly `0.0`. 

the matrix $\mathbf{W}$ is over **99.8% empty**. 

in linear algebra, this is called a **sparse matrix**. storing it as a standard dense matrix in memory is catastrophically inefficient. 
1.  **memory footprint:** representing billions of zeros explicitly wastes RAM.
2.  **computational complexity:** when the CPU performs the matrix multiplication $\mathbf{y} = \mathbf{Wx}$, it will spend 99.8% of its clock cycles calculating `0.0 * x_j = 0.0`. multiplying by zero millions of times is a waste of processing power.

### compressed sparse row (csr) format

instead of storing a full 2D grid, high-performance linear algebra libraries (like Eigen in C++) store sparse matrices using three flat 1D vectors:
1.  **values:** a list of all the non-zero float values in the matrix.
2.  **column indices:** the column index for each of those values.
3.  **row pointers:** the index in the "values" array where each new row begins.

this strips the matrix down to only its essential data, reducing memory from gigabytes to mere kilobytes, and allowing matrix multiplications to skip the zeros entirely.

## the inverse problem: solving for x

our forward simulator uses $\mathbf{W}$ and a known $\mathbf{x}$ to generate mock sensor data $\mathbf{y}$.

but the ultimate goal of Radio Tomographic Imaging is the **inverse problem**. in the real world, the ESP32s measure $\mathbf{y}$, the physics engine generates $\mathbf{W}$ based on where you placed the nodes, and the computer has to solve for $\mathbf{x}$ (the image of the room).

in standard linear algebra, solving $\mathbf{Wx} = \mathbf{y}$ is straightforward. you find the inverse of $\mathbf{W}$, denoted $\mathbf{W}^{-1}$, and multiply both sides:

$$ \mathbf{W}^{-1}\mathbf{Wx} = \mathbf{W}^{-1}\mathbf{y} \implies \mathbf{x} = \mathbf{W}^{-1}\mathbf{y} $$

### the dimensionality trap

there is a fatal flaw in this approach. **a matrix must be square ($N \times N$) to have an inverse.** 

our $\mathbf{W}$ matrix is $28 \times 27000$. it is severely rectangular. 
in mathematical terms, the problem is **underdetermined**. we have 27,000 unknowns (voxels), but only 28 equations (measurements). 

imagine i give you the equation $a + b = 10$, and ask you to solve for $a$ and $b$. you can't. it could be 5 and 5, or 9 and 1. there are infinite valid solutions. 

this is the core mathematical challenge of volumetric imaging. how do you find the *most physically likely* solution out of an infinite set of mathematical possibilities?

we solve this using the calculus of variations: the **Least Squares** method and **Tikhonov Regularisation**, which we cover next.