# maths-you-must-know: the theoretical bedrock

if a lead engineer or lead engineer queries this architecture, they will not ask about c++. they will ask about the underlying assumptions you made when mapping physical reality into finite mathematics.

you must be perfectly fluent in these four concepts. if asked to write them on a whiteboard, you cannot hesitate.

## 1. The Normal Equations ($\mathbf{W}^T\mathbf{Wx} = \mathbf{W}^T\mathbf{y}$)

**The Concept:**
you have a rectangular matrix equation $\mathbf{Wx} = \mathbf{y}$ (28 measurements; 1,000 voxels at the committed scale, 27,000 at full deployment). it has no direct inverse.
you want the Least Squares approximation.

**The Derivation:**
you want to minimise the squared residual error: $E = ||\mathbf{Wx} - \mathbf{y}||_2^2$.
expanding the square: $E = (\mathbf{Wx} - \mathbf{y})^T (\mathbf{Wx} - \mathbf{y})$.
expanding the terms: $E = \mathbf{x}^T\mathbf{W}^T\mathbf{Wx} - 2\mathbf{y}^T\mathbf{Wx} + \mathbf{y}^T\mathbf{y}$.

to find the minimum of this multi-dimensional "bowl", you take the gradient (vector derivative) with respect to $\mathbf{x}$ and set it exactly to $\mathbf{0}$:
$\nabla_x E = 2\mathbf{W}^T\mathbf{Wx} - 2\mathbf{W}^T\mathbf{y} = \mathbf{0}$.
dividing by 2 and rearranging gives the Normal Equations:
$$ \mathbf{W}^T\mathbf{Wx} = \mathbf{W}^T\mathbf{y} $$

**Why they ask this:**
it proves you didn't just type `solve()` into a library. it proves you understand how partial derivatives and vector gradients find the lowest point of a mathematical error function.

---

## 2. Condition Number & Ill-Conditioned Matrices

**The Concept:**
the condition number ($\kappa$) of a matrix measures how sensitive the solution of a linear equation ($\mathbf{Ax} = \mathbf{b}$) is to errors in the data ($\mathbf{b}$).

**The Relevance:**
in our case, the matrix $\mathbf{A}$ is $(\mathbf{W}^T\mathbf{W})$, and the data $\mathbf{b}$ is our noisy ESP32 signals $\mathbf{W}^T\mathbf{y}$.
because our system is massively underdetermined (rank-deficient), the matrix $(\mathbf{W}^T\mathbf{W})$ is nearly singular. its condition number is practically infinite.

**The Defense:**
"The condition number dictates the error bounds. If $\kappa$ is $10^6$, a tiny $1\%$ error in the sensor data $\mathbf{y}$ can cause a $1,000,000\%$ error in the reconstructed 3D image $\mathbf{x}$. The math becomes entirely unstable. This is the exact reason I could not use standard Least Squares and had to introduce regularisation mathematically suppress $\kappa$."

---

## 3. Tikhonov Regularisation (Ridge Regression)

**The Concept:**
stabilising an ill-posed matrix by mathematically penalising extreme outputs.

**The Math:**
we add a penalty term ($\lambda||\mathbf{x}||_2^2$) to the squared error function:
$$ E = ||\mathbf{Wx} - \mathbf{y}||_2^2 + \lambda ||\mathbf{x}||_2^2 $$

when you take the gradient and set it to zero (the same way we derived the normal equations above), the $+\lambda||\mathbf{x}||_2^2$ term differentiates to $+2\lambda\mathbf{x}$.
the new, stabilised Normal Equation becomes:
$$ (\mathbf{W}^T \mathbf{W} + \lambda \mathbf{I}) \mathbf{x} = \mathbf{W}^T \mathbf{y} $$

**The Defense:**
"Adding the identity matrix $\mathbf{I}$ scaled by $\lambda$ guarantees that the diagonal values of the matrix are non-zero and large enough to be inverted safely. Physically, it forces the solver to prefer solutions where the voxel attenuation values remain small and smooth, preventing the noise from ripping the image apart."

---

## 4. The Inverse-Square Law vs. Path Attenuation

**The Concept:**
radio wave power drops for two entirely separate physical reasons.

1.  **Free-Space Path Loss (Inverse-Square):** As the wave expands spherically, its energy is spread over a larger area. $P \propto \frac{1}{r^2}$.
2.  **Absorption (Attenuation):** When the wave hits water, concrete, or human flesh, the electrons in the material oscillate, turning RF energy into heat.

**The Relevance:**
our forward model ($\Delta y_i = \sum W_{ij}x_j$) *only* models absorption. it completely ignores the inverse-square law.

**The Defense:**
why is that mathematically valid?
"Because RTI is a differential measurement system. I record a baseline signal $y_{empty}$ before the human enters the room. The inverse-square loss is permanent; it exists in both the empty room and the occupied room. By subtracting the current measurement from the baseline ($\Delta y = y_{empty} - y_{current}$), the inverse-square geometric path loss mathematically cancels out entirely. The only thing left in $\Delta y$ is the physical absorption caused by the new obstacle."