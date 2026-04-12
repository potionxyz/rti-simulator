# trade-off-questions: defending engineering choices

an imperial eie (or any senior systems architecture) interview will push past the theory and focus heavily on trade-offs. in software engineering, every choice is awful. an architecture is successful because you chose the *least awful* option for your specific goals.

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
1.  **Cache Locality:** python abstracts memory placement. in a $27000 \times 27000$ inverse solver, python's garbage collection and object overhead cause massive ram spikes. C++ allowed you to construct a `std::vector<float>` and guarantee contiguous L1/L2 cache hits during Siddon's algorithm.
2.  **Embedded Target:** the final stage of RTI is physical deployment on low-power ESP32 microcontrollers. python cannot run performantly on edge devices. Writing the core solver in C++ now means the backend is inherently portable to edge-computing scenarios later.

---

## 2. Voxel Size: 10cm vs. 5cm vs. 20cm

**The Choice:**
you discretised the physical $3m^3$ room into $0.1m$ (10cm) voxels, resulting in $30 \times 30 \times 30 = 27,000$ voxels.

**The Trade-Off (Why 10cm):**
*   if you use **5cm voxels ($216,000$ voxels)**: the physical resolution is incredible, but the math explodes. your matrix $\mathbf{W}$ becomes $28 \times 216000$. the least squares term $(\mathbf{W}^T\mathbf{W})$ tries to allocate a matrix with $46$ billion floats ($186$ GB of RAM). your computer crashes before the solver even starts calculating.
*   if you use **20cm voxels ($4,000$ voxels)**: the math runs instantly, but the physics breaks down. a human torso is roughly 40cm wide. in a 20cm grid, the entire human body is represented by just 4 voxels. the spatial resolution is so poor the localisation error metric becomes completely meaningless.

**The Defense:**
"10cm is the sweet spot. it provides enough physical fidelity to capture a human's 3D profile across 20-30 voxels, while keeping the inverse problem small enough ($27,000$ unknowns) that the Eigen sparse matrix solver can run Conjugate Gradients in real-time without blowing out system RAM."

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
Because of this, I could not use the simple, incredibly fast Normal Equations to find the gradient minimum. $L_1$ requires complex, computationally expensive iterative solvers like ADMM (Alternating Direction Method of Multipliers) or Interior Point Methods. For a real-time simulator aiming for embedded ESP32 hardware, the computational cost of solving an $L_1$ problem on $27,000$ voxels was too high. The Tikhonov $L_2$ norm keeps the matrix equation differentiable and invertible instantly with `Eigen`."