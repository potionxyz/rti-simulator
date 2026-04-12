# 00-prerequisites: the maths you need first

before diving into the exact c++ simulator code, you must be completely fluent in three areas of advanced mathematics:

## 1. the dot product (scalar product)
if $\mathbf{a} = \begin{pmatrix} x_1 \\ y_1 \\ z_1 \end{pmatrix}$ and $\mathbf{b} = \begin{pmatrix} x_2 \\ y_2 \\ z_2 \end{pmatrix}$, then:
$$ \mathbf{a} \cdot \mathbf{b} = x_1 x_2 + y_1 y_2 + z_1 z_2 $$
**why you need this:**
the forward model (signal $\Delta y$) is basically a massive dot product between a single row of the Weight matrix $\mathbf{W}$ (the distances travelled through each voxel) and the column vector $\mathbf{x}$ (the attenuation of each voxel).

## 2. matrix multiplication dimensions
if matrix $\mathbf{A}$ is an $m \times p$ matrix, and matrix $\mathbf{B}$ is a $p \times n$ matrix, you can multiply them to get an $m \times n$ matrix $\mathbf{C}$.
**why you need this:**
the core equation is $\mathbf{Wx} = \mathbf{y}$. $\mathbf{W}$ is a $28 \times 27000$ matrix. $\mathbf{x}$ is a $27000 \times 1$ column vector. multiplying them results in a $28 \times 1$ column vector ($\mathbf{y}$). understanding dimension compatibility is how you catch segfaults in c++.

## 3. the l2-norm (euclidean length)
the $L_2$-norm of a vector $\mathbf{v}$ with elements $v_1, v_2, \dots, v_n$ is its physical length:
$$ || \mathbf{v} ||_2 = \sqrt{v_1^2 + v_2^2 + \dots + v_n^2} $$
squaring the norm removes the square root:
$$ || \mathbf{v} ||_2^2 = v_1^2 + v_2^2 + \dots + v_n^2 $$
**why you need this:**
least squares and tikhonov regularisation both work by minimising the squared norm of a vector (e.g. minimising the squared residual errors).

## 4. partial derivatives (conceptually)
you don't need to manually derive matrix calculus equations, but you must know what the $\nabla$ (nabla) operator is. to find the minimum of a curve (like an error function in least squares), you take the derivative and set it to exactly zero. 

if you have these four concepts locked down from advanced mathematics, you are mathematically prepared for `01-voxel-maths.md`.