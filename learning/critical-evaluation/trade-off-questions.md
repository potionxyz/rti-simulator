# trade-off-questions: defending engineering choices

a senior systems architecture review will push past the theory and focus heavily on trade-offs. in software engineering, every choice is awful. an architecture is successful because you chose the *least awful* option for your specific goals.

here is the defensive analysis of the fundamental trade-offs inside your rti simulator.

---

## 1. C++ vs. Python (NumPy)

**The Choice:**
you built a massively mathematical machine learning/inverse problem simulator in C++ (`Eigen`). the industry standard for this is python (`numpy` and `scipy`).

**The Trade-Off (Why C++ was a pain):**
*   python is incredibly fast to write for matrix algebra.
*   python visualises tensors natively in three lines using `matplotlib`.
*   C++ required manual `CMakeLists.txt` build configuration, manual memory management (`new`, `delete`, or `std::vector`), and building an entire separate node/vercel stack just to see the 3d output images.

**The Defense (Why C++ was correct):**
1.  **Cache Locality:** python abstracts memory placement. in a large inverse solver — $1000 \times 1000$ at current scale, scaling up to $27000 \times 27000$ for a production deployment — python's garbage collection and object overhead cause unpredictable ram spikes. C++ allowed you to construct a `std::vector<double>` and guarantee contiguous L1/L2 cache hits during Siddon's algorithm.
2.  **Embedded Target:** the final stage of RTI is physical deployment on low-power ESP32 microcontrollers. python cannot run performantly on edge devices. Writing the core solver in C++ now means the backend is inherently portable to edge-computing scenarios later.

---

## 2. Voxel Size: 10cm vs. 5cm vs. 20cm

**The Choice:**
the committed simulator runs on a $2m^3$ room with $0.2m$ voxels, giving $10 \times 10 \times 10 = 1{,}000$ voxels — small enough for direct LDLT solving. the full-scale deployment target is a $3m^3$ room at $0.1m$ resolution, giving $30 \times 30 \times 30 = 27{,}000$ voxels, which would require swapping to an iterative Conjugate Gradient solver.

**The Trade-Off (resolution vs tractability):**
*   **5cm voxels ($216{,}000$ in a 3m room):** the physical resolution is incredible, but the math explodes. the matrix $\mathbf{W}^T\mathbf{W}$ holds $46{,}000{,}000{,}000$ floats ($\approx$ 370 GB of RAM if stored dense, still many GB sparse). your machine crashes before the solver even starts. the only path forward is a sparse iterative solver plus preconditioning, and probably out-of-core memory.
*   **10cm voxels ($27{,}000$ in a 3m room):** the physical sweet spot for a real deployment. a human torso spans 20-30 voxels, giving good localisation fidelity. the dense solver is impractical but a sparse `ConjugateGradient` runs in seconds.
*   **20cm voxels ($1{,}000$ in a 2m room, what i actually built):** math runs in milliseconds with dense LDLT, but each human body is only about 4 voxels across. localisation accuracy is limited by the voxel edge itself.
*   **40cm voxels:** too coarse — a human could fit inside a single voxel and lose all shape information.

**The Defense:**
"I picked 20cm for the committed version because it keeps the inverse problem fully tractable with a direct Cholesky solver and lets me iterate on algorithms quickly. For a production ESP32 deployment, 10cm resolution is the real target because it captures enough of the human torso to actually be useful for localisation, but that requires moving to an iterative sparse solver (Conjugate Gradient or LSQR) and pre-computing the Tikhonov pseudo-inverse on a host machine rather than on the microcontroller. The maths and architecture don't change — only the solver primitive does."

---

## 3. Siddon's Algorithm vs. Basic Ray-Marching

**The Choice:**
you implemented Siddon's exact parametric ray-tracing algorithm for the forward model to generate the physics matrix $\mathbf{W}$.

**The Trade-Off (Why Siddon's):**
*   basic ray-marching (stepping along the line vector $\vec{v}$ by $1mm$ increments and checking the current 3d coordinate) is incredibly easy to code and mathematically intuitive.
*   Siddon's algorithm is confusing to implement, requires complex intersection checks across X, Y, and Z planes, and needs sorting algorithms to organise the $\alpha$ vector parameters.

**The Defense:**
"Ray-marching is $O(d/\Delta t)$, where $\Delta t$ is the step size. If my step size is $1mm$ across a $3$ metre room, that's $3000$ loop iterations per radio link. Since the grid is mostly empty space, 95% of those iterations are wasted checking empty voxels. Siddon's algorithm is $O(N_x + N_y + N_z)$. It only computes exact boundary intersections mathematically using a single line parameter $\alpha$. It generates the exact path lengths required for the line integral instantly with zero wasted loop iterations. For generating a massive system matrix $\mathbf{W}$, ray-marching is fundamentally unscalable."

---

## 4. Tikhonov vs. L1-Norm (Lasso) Regularisation

**The Choice:**
you stabilised the Least Squares inverse problem using Tikhonov Regularisation ($L_2$-norm penalty: $\lambda||\mathbf{x}||_2^2$).

**The Trade-Off:**
*   Tikhonov ($L_2$) heavily penalises massive numbers (e.g. $100^2$ is a much bigger penalty than $10^2$), forcing the voxel values to stay small and smooth out like a Gaussian blur.
*   Lasso / Compressive Sensing ($L_1$-norm penalty: $\lambda||\mathbf{x}||_1$) forces the actual number of non-zero voxels to be as small as possible. This makes physical sense (the room is mostly empty, so push most voxels to exactly `0.0` attenuation).

**The Defense (Why you didn't use L1):**
"L1 regularisation (Compressive Sensing) is physically sound because the true attenuation field is highly sparse—most of the room is air. However, the $L_1$ norm uses an absolute value ($|x|$). It is not mathematically differentiable at $x=0$.
Because of this, I could not use the simple, incredibly fast Normal Equations to find the gradient minimum. $L_1$ requires complex, computationally expensive iterative solvers like ADMM (Alternating Direction Method of Multipliers) or Interior Point Methods. For a real-time simulator aiming for embedded ESP32 hardware, the computational cost of solving an $L_1$ problem on even 1,000 voxels (let alone 27,000 at full deployment) was too high. The Tikhonov $L_2$ norm keeps the matrix equation differentiable and solvable instantly with Eigen's `LDLT`."