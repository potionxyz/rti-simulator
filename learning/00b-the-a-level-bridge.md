# 00b-the-a-level-bridge: mastering the normal equations

if you have advanced mathematics (further maths, matrices, $L_2$-norms), you are mathematically prepared for the linear algebra inside the RTI simulator. 

however, the final barrier between A-Level Further Maths and university-level signal processing is **multivariable matrix differentiation.** 

an undergraduate student takes an entire module to learn this. you just need the following translation guide.

---

## 1. The Matrix Calculus Cheat Code

**Where you are:**
you know how to differentiate a polynomial: 
*   $\frac{d}{dx} (10x) = 10$
*   $\frac{d}{dx} (5x^2) = 10x$

**The Bridge:**
in `04-tikhonov-regularisation.md`, we take the derivative of a massive matrix equation with respect to a vector $\mathbf{x}$ to find the least-squares minimum. 

you do not need to derive matrix calculus from scratch. the rules are identical to the A-Level rules, just written with bold matrix variables. 

*   **Rule 1:** $\frac{d}{dx} (ax) = a$
    *   **Matrix Translation:** $\nabla_{\mathbf{x}} (\mathbf{a}^T \mathbf{x}) = \mathbf{a}$
    *   (Differentiating a linear term just leaves the constant coefficient).
*   **Rule 2:** $\frac{d}{dx} (bx^2) = 2bx$
    *   **Matrix Translation:** $\nabla_{\mathbf{x}} (\mathbf{x}^T \mathbf{B} \mathbf{x}) = 2\mathbf{B}\mathbf{x}$
    *   (Differentiating a quadratic term brings down a 2, and leaves one $x$).

## 2. Deriving the Normal Equations

**Where you are:**
you understand the $L_2$-norm. the squared error of a system is $||\mathbf{e}||_2^2$, which mathematically expands to $\mathbf{e}^T\mathbf{e}$ (the dot product of the error with itself).

**The Bridge:**
we want to minimise the error of our simulation: $E = ||\mathbf{Wx} - \mathbf{y}||_2^2$.

1.  **Expand the error function:**
    $$ E = (\mathbf{Wx} - \mathbf{y})^T (\mathbf{Wx} - \mathbf{y}) $$
    $$ E = (\mathbf{x}^T\mathbf{W}^T - \mathbf{y}^T) (\mathbf{Wx} - \mathbf{y}) $$
    $$ E = \mathbf{x}^T\mathbf{W}^T\mathbf{Wx} - \mathbf{x}^T\mathbf{W}^T\mathbf{y} - \mathbf{y}^T\mathbf{Wx} + \mathbf{y}^T\mathbf{y} $$

2.  **Simplify the scalars:**
    the two middle terms are identical scalars. $(x \times y = y \times x)$.
    $$ E = \mathbf{x}^T(\mathbf{W}^T\mathbf{W})\mathbf{x} - 2\mathbf{y}^T\mathbf{Wx} + \mathbf{y}^T\mathbf{y} $$

3.  **Take the derivative and set to zero ($\nabla = 0$):**
    using the cheat code from Section 1:
    *   the first term is a quadratic (like $5x^2$). the derivative is $2(\mathbf{W}^T\mathbf{W})\mathbf{x}$.
    *   the second term is linear (like $10x$). the derivative is $2\mathbf{W}^T\mathbf{y}$.
    *   the third term ($\mathbf{y}^T\mathbf{y}$) has no $\mathbf{x}$ in it. it's a constant. the derivative is $\mathbf{0}$.

    $$ \nabla_{\mathbf{x}} E = 2\mathbf{W}^T\mathbf{Wx} - 2\mathbf{W}^T\mathbf{y} = \mathbf{0} $$

4.  **The Result:**
    divide by 2, and move the negative term to the other side:
    $$ \mathbf{W}^T\mathbf{Wx} = \mathbf{W}^T\mathbf{y} $$

these are the **Normal Equations**. you have just derived the core of linear least squares in 4 lines, bridging A-Level calculus directly to machine learning.

---

## 3. The Condition Number Trap

**Where you are:**
you know how to invert a $2 \times 2$ or $3 \times 3$ matrix. if the determinant is exactly 0, the matrix is singular and has no inverse.

**The Bridge:**
in the real world, the determinant is almost never exactly `0.00000000000`. 
in RTI, the matrix $(\mathbf{W}^T\mathbf{W})$ is rank-deficient. due to floating-point noise on a CPU, the determinant might be `0.00000000001`. 

it is technically invertible, but it is **ill-conditioned**.

the condition number $\kappa(\mathbf{A})$ measures how close a matrix is to being singular.
*   if $\kappa = 1$ (an identity matrix), a $1\%$ error in sensor data gives a $1\%$ error in the final image.
*   if $\kappa = 10^7$ (our $\mathbf{W}^T\mathbf{W}$ matrix), a tiny $0.001$ change in the ESP32 noise will be multiplied by ten million when inverted, ripping the mathematical image of the room apart with extreme values.

this is why we cannot solve the Normal Equations directly, and why `04-tikhonov-regularisation.md` introduces Ridge Regression to artificially lower the condition number using $\lambda\mathbf{I}$.