# 04-tikhonov-regularisation: handling ill-posed data

we left off with $M$ linear equations (measurements, $\mathbf{y}$) and $N$ unknowns (voxels, $\mathbf{x}$). our system is modeled as $\mathbf{y} = \mathbf{Wx}$.

because $M \ll N$ (e.g., 28 measurements vs. 27,000 voxels), $\mathbf{W}$ is a rectangular matrix, not a square matrix. an inverse matrix $\mathbf{W}^{-1}$ simply does not exist. and even if it did, real-world data contains noise.

if we try to solve for $\mathbf{y_{noisy}} = \mathbf{Wx}$, any exact mathematical solution will amplify that noise to infinity, resulting in an output image that looks like TV static instead of a human. this is known mathematically as an **ill-posed inverse problem**.

## the least squares approach

to find the "best fit" solution when exact equations fail, we use a concept from statistics: **Least Squares**.

instead of finding an $\mathbf{x}$ that perfectly satisfies $\mathbf{y} = \mathbf{Wx}$, we want to find an $\mathbf{x}$ that minimises the difference (the error) between what our forward model *predicts*, and what the ESP32 sensors *actually measure*.

this difference is the residual vector: $\mathbf{r} = \mathbf{Wx} - \mathbf{y}$.

the length (or magnitude) of this vector is represented mathematically using the $L_2$-norm, squared:

$$ || \mathbf{Wx} - \mathbf{y} ||_2^2 $$

this equation means: square the difference of every single measurement, and sum them up. it is the sum of squared errors.

our goal is to find the vector $\mathbf{x}$ that makes this term as close to zero as possible. using multivariate calculus (taking the gradient with respect to $\mathbf{x}$ and setting it to zero), we arrive at the standard linear least squares solution:

$$ \mathbf{x} = (\mathbf{W}^T \mathbf{W})^{-1} \mathbf{W}^T \mathbf{y} $$

*   $\mathbf{W}^T$ is the transpose of the weight matrix.
*   $(\mathbf{W}^T \mathbf{W})$ creates a square $N \times N$ matrix.

### why least squares fails in rti

this formula is mathematically sound, but in Volumetric Radio Tomographic Imaging, it physically fails.

the matrix $(\mathbf{W}^T \mathbf{W})$ is massive ($27000 \times 27000$). because the problem is severely underdetermined (we lack measurements for thousands of voxels), many rows in this matrix are near zero or linearly dependent.

in linear algebra, a matrix like this has a very high **condition number**. it is near-singular. when the computer attempts to calculate the inverse $(\mathbf{W}^T \mathbf{W})^{-1}$, the tiny noise fluctuations in $\mathbf{y}$ are multiplied by astronomically large values.

the computed $\mathbf{x}$ becomes wildly unstable, oscillating between $+10,000$ and $-10,000$ attenuation in adjacent voxels. the image is destroyed by its own mathematical variance.

## the solution: tikhonov regularisation

how do we stop the math from panicking? we add a penalty.

if the problem with Least Squares is that it allows $\mathbf{x}$ to oscillate wildly with massive positive and negative values, we need to enforce a physical rule: **"the attenuation of the room must be reasonably smooth, and extreme voxel values are heavily penalised."**

this is called **Regularisation**. the most common form for linear inverse problems is Tikhonov Regularisation (also known as Ridge Regression in machine learning).

we modify our original error function to include a penalty term:

$$ \min_x || \mathbf{Wx} - \mathbf{y} ||_2^2 + \lambda ||\mathbf{x}||_2^2 $$

let's break this down:
1.  **$||\mathbf{Wx} - \mathbf{y}||_2^2$:** the data fidelity term. "make the image match the sensor data."
2.  **$||\mathbf{x}||_2^2$:** the regularisation penalty term. "keep the voxel attenuation values as small and smooth as possible."
3.  **$\lambda$ (lambda):** the regularisation parameter. this acts as a tuning knob between the two competing goals.

if $\lambda = 0$, we just have standard least squares (noisy junk).
if $\lambda$ is extremely large, the maths ignores the sensor data entirely and sets the whole room to 0 (an empty grey box).

we have to tune $\lambda$ experimentally to find the perfect physical balance between sharp edges and smooth noise.

### the mathematical solution

by taking the gradient of this new, penalised error function with respect to $\mathbf{x}$ and setting it to zero, we arrive at the Tikhonov solution:

$$ \mathbf{x} = (\mathbf{W}^T \mathbf{W} + \lambda \mathbf{I})^{-1} \mathbf{W}^T \mathbf{y} $$

*   $\mathbf{I}$ is the Identity Matrix (a square matrix with 1s on the main diagonal and 0s everywhere else).

this single addition ($+ \lambda \mathbf{I}$) is physically profound. it adds a tiny constant value to the diagonal elements of the $(\mathbf{W}^T \mathbf{W})$ matrix.

by boosting the diagonal, the matrix is no longer singular. its condition number drops drastically. the inverse operation is now stable, and the noise amplification is mathematically suppressed.

## the c++ implementation

using the `Eigen` library in C++, this terrifying mathematical equation translates into highly readable code:

```cpp
// from test_scene.cpp / Reconstructor
Eigen::SparseMatrix<float> Wt = W.transpose();
Eigen::SparseMatrix<float> WtW = Wt * W;

// construct the identity matrix penalty term
Eigen::SparseMatrix<float> I(N, N);
I.setIdentity();

// WtW + lambda*I
Eigen::SparseMatrix<float> A = WtW + lambda * I;

// Wt * y
Eigen::VectorXf b = Wt * y;

// Using Eigen's sparse iterative solver (Conjugate Gradient)
// solving Ax = b for x
Eigen::ConjugateGradient<Eigen::SparseMatrix<float>> solver;
solver.compute(A);
Eigen::VectorXf x = solver.solve(b);
```

this is the core engine of the entire simulation. the ESP32s feed in $\mathbf{y}$, the physics engine builds $\mathbf{W}$, and the Eigen solver crunches the Tikhonov equation to output a 3D image of the human ($\mathbf{x}$).