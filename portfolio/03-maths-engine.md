# 03-the-maths-engine

so once the physics engine was working and generating fake RSSI loss measurements ($\mathbf{y}$), i had to build the solver. this is the actual core of Radio Tomographic Imaging (RTI).

the ESP32 nodes give us a column vector $\mathbf{y}$ (the signal changes). the physics engine built the weight matrix $\mathbf{W}$ (which voxels the signals passed through). now i have to solve for $\mathbf{x}$ (the 3D image of the room).

## the inverse problem: why a-level maths breaks

the equation is just $\mathbf{Wx} = \mathbf{y}$.

in normal maths, you just find the inverse of $\mathbf{W}$ and multiply. $\mathbf{x} = \mathbf{W}^{-1}\mathbf{y}$. easy.

but in RTI, this is completely impossible.

1.  **the matrix isn't square:** for an 8-node setup, i only have 28 unique cross-links (measurements). but my room is carved into 27,000 voxels (unknowns). $\mathbf{W}$ is a $28 \times 27000$ matrix. you mathematically cannot invert a rectangular matrix. it's an underdetermined system; there are infinitely many ways those 28 signals could have been blocked by 27,000 voxels.
2.  **hardware noise is fatal:** even if i used a pseudo-inverse, real-world data contains noise. the inverse of a severely ill-posed matrix acts like an amplifier. a 1 dBm noise fluctuation from the ESP32 will be multiplied by millions, resulting in a reconstructed image that is just violent mathematical static swinging between $+10,000$ and $-10,000$ attenuation.

## forcing a solution: least squares

if i can't solve it exactly, i have to guess the *best physical fit*. i need the vector $\mathbf{x}$ that minimises the difference between what the simulator predicts, and what the sensors read.

this is the Least Squares method from statistics. you minimise the squared error: $|| \mathbf{Wx} - \mathbf{y} ||_2^2$.

taking the derivative and setting it to zero gives the standard linear least squares formula:
$$ \mathbf{x} = (\mathbf{W}^T \mathbf{W})^{-1} \mathbf{W}^T \mathbf{y} $$

### hitting the computational wall

i implemented this in C++ using the `Eigen` linear algebra library. i stored $\mathbf{W}$ as a `SparseMatrix` because 99.8% of the voxels are completely empty (the radio waves only pass through a tiny fraction of the room). storing that as a dense 2D vector would blow my RAM instantly.

but least squares failed immediately. 

the new matrix $(\mathbf{W}^T \mathbf{W})$ is massive ($27000 \times 27000$). because the problem is so underdetermined, this new matrix is near-singular (its condition number is astronomically high). when Eigen tried to calculate the inverse, the solver either crashed or returned pure noise. 

## the fix: tikhonov regularisation

how do you stop the maths from panicking? you add a physical rule.

i know that a human body is solid. the attenuation should be relatively smooth across adjacent voxels. it shouldn't jump from $+5000$ to $-5000$ instantly. the mathematical solver doesn't know that; it’s just trying to fit the noisy numbers perfectly.

so i implemented **Tikhonov Regularisation** (also known as Ridge Regression). you add a penalty term to the error function: "find a solution that matches the data, but penalise any solution where the voxel values are huge."

$$ \min_x || \mathbf{Wx} - \mathbf{y} ||_2^2 + \lambda ||\mathbf{x}||_2^2 $$

$\lambda$ (lambda) is a tuning parameter. if it's 0, it's just noisy least squares. if it's huge, the room is just a flat grey box because all values are penalised to 0.

the brilliant part of Tikhonov is how it changes the matrix formula:

$$ \mathbf{x} = (\mathbf{W}^T \mathbf{W} + \lambda \mathbf{I})^{-1} \mathbf{W}^T \mathbf{y} $$

that tiny $+ \lambda \mathbf{I}$ adds a constant value to the diagonal of the main matrix. physically, this makes the matrix non-singular. the condition number plummets. the matrix is now mathematically stable to invert.

### putting it into eigen

translating this horrifying equation into C++ with Eigen was shockingly clean. running this on an underdetermined sparse matrix using a Conjugate Gradient solver is basically instant.

```cpp
// from src/reconstructor.cpp
Eigen::SparseMatrix<float> W_transpose = W_.transpose();
Eigen::SparseMatrix<float> WtW = W_transpose * W_;

// the regularisation penalty (lambda * Identity Matrix)
Eigen::SparseMatrix<float> I(nx_ * ny_ * nz_, nx_ * ny_ * nz_);
I.setIdentity();

// stabilise the matrix (WtW + lambda*I)
Eigen::SparseMatrix<float> A = WtW + lambda * I;
Eigen::VectorXf b = W_transpose * y;

// solve Ax = b iteratively
Eigen::ConjugateGradient<Eigen::SparseMatrix<float>> cg_solver;
cg_solver.compute(A);
VectorXf resolved_x = cg_solver.solve(b);
```

this is the core engine of the whole project. the mathematical capability to take 28 noisy cross-links from cheap ESP32 sensors, penalise the noise mathematically using Tikhonov, and spit out a stable 27,000-voxel 3D image of a human.