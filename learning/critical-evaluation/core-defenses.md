# 05-core-defenses: defending the architecture

if you present this project, senior reviewers will not ask you "did you write this?" they will ask probing technical questions to see if you understand the fundamental limits of the mathematical model and the C++ architecture.

this document is your defensive roadmap. it contains the most likely questions an engineer will ask, why they are asking them, and how you should answer.

---

## 1. The Physics & Sparsity

**Q: "You have a 1,000-voxel grid with only 28 measurements. How is this matrix structurally different from a typical image processing matrix, and how did you handle that in memory?"**

*   **Why they ask:** they want to see if you understand the concept of a "sparse matrix" and its memory implications.
*   **Your Answer:** "The weight matrix $W$ maps every link to every voxel. Since radio waves travel in narrow lines of sight, a single link only intersects roughly 15-20 out of 1,000 voxels in my current grid — about 98% of the matrix is zero. At full-scale deployment (3m room, 10cm voxels → 27,000 voxels) that sparsity climbs to over 99.8%. For the committed simulator, 1,000 voxels is small enough that I store $W$ as a dense `Eigen::MatrixXd` and rely on contiguous memory layout for L1/L2 cache locality during the matrix-vector products. When I scale to 27,000+ voxels for a real deployment, I'll switch to `Eigen::SparseMatrix` in Compressed Sparse Row (CSR) format, which stores only the non-zero values and skips the zero multiplications entirely."

**Q: "What happens if a radio wave perfectly grazes the boundary between two voxels in your forward model?"**

*   **Why they ask:** they are testing your understanding of Siddon's algorithm and numerical precision.
*   **Your Answer:** "Siddon's algorithm calculates exact ray-plane intersections using a parametric parameter $\alpha$. If a ray hits a boundary perfectly, the difference between the two $\alpha$ values is 0. mathematically, the chord length through that voxel boundary is 0, so the weight $W_{ij}$ is 0. Floating-point precision (epsilon) handles the rounding. It doesn't break the model."

---

## 2. The Inverse Problem & Linear Algebra

**Q: "Why can't you just use standard Linear Least Squares to solve for the voxel attenuation?"**

*   **Why they ask:** this is the core mathematics question. they want you to explain ill-posed inverse problems.
*   **Your Answer:** "Because the system is severely underdetermined. I have $M=28$ equations and $N=1{,}000$ unknowns at the current scale (27,000 at full theoretical deployment). Either way $M \ll N$, so the least squares matrix $(\mathbf{W}^T\mathbf{W})$ is rank-deficient and near-singular — its condition number is astronomically high. If I inverted it, any tiny amount of noise in the $\mathbf{y}$ vector would be amplified enormously, resulting in an image with massive oscillating positive and negative attenuation values. The math just panics."

**Q: "Explain how Tikhonov Regularisation fixes that mathematical panic."**

*   **Why they ask:** testing if you understand *why* the equation works, not just what it is.
*   **Your Answer:** "Tikhonov adds a penalty term: $\lambda ||\mathbf{x}||^2$ to the error function. We want to fit the data, but we also want to heavily penalise extreme voxel values. Mathematically, this adds $\lambda$ to the diagonal of the $(\mathbf{W}^T\mathbf{W})$ matrix (an identity matrix $\mathbf{I}$). Adding value to the diagonal makes the matrix invertible and drastically lowers the condition number. Physically, it forces the solver to prefer smooth, mathematically stable solutions over noisy ones."

**Q: "How did you choose the value of Lambda ($\lambda$)?"**

*   **Why they ask:** tuning regularisation is notoriously difficult. they want to see if you did it analytically or empirically.
*   **Your Answer:** "I tuned it empirically through parameter sweeps. If $\lambda$ is too small, the image is overwhelmed by noise. If it's too large, the penalty dominates the sensor data and the entire room blurs into an empty grey mass. I found a sweet spot where the localisation error (the distance between the true human's centre and the reconstructed mass) was minimised."

---

## 3. C++ Architecture & Performance

**Q: "Why did you build this in C++ instead of Python/NumPy?"**

*   **Why they ask:** python is standard for this. C++ is a deliberate engineering choice.
*   **Your Answer:** "Three reasons. First, memory contiguity. Inverting large matrices causes RAM spikes. C++ allowed me to strictly manage heap allocations and ensure my voxel grid was a single flat 1D vector mapped linearly to $i = x+yN_x+zN_xN_y$ to guarantee L1/L2 cache locality. Second, performance. Ray-tracing 28 links through 1,000 voxels (or 27,000 at production scale) happens orders of magnitude faster in compiled C++ using Siddon's method than it would in interpreted Python. Third, the eventual goal is hardware scale on physical ESP32 microcontrollers, which run C/C++. Building the maths engine in C++ now means it's portable later."

**Q: "What C++ standards or tools did you rely on?"**

*   **Your Answer:** "I built it using standard C++17, managing the build pipeline with CMake. The only external dependencies were header-only: Eigen for the sparse linear algebra, and a small JSON parser to load test scenes dynamically so I didn't have to recompile the binaries every time I wanted to move the simulated human."

---

## 4. The Future (Hardware Limits)

**Q: "You simulated Gaussian noise. But in the real world, 2.4GHz Wi-Fi doesn't behave perfectly like a Gaussian distribution. What physical problems will break this simulator when you move to real ESP32 nodes?"**

*   **Why they ask:** this is the "kill question". simulators lie. hardware is messy. if you know the limits of your own code, you pass.
*   **Your Answer:** "Multipath fading. My forward model assumes a perfect line-of-sight ray. In a real room, the 2.4GHz signal bounces off the floor, ceiling, and metal objects. The receiver measures the constructive and destructive interference of all those bounced rays, not just the direct path. If the multipath interference is strong enough, the measured attenuation $\Delta y$ will be completely wrong. In the physical deployment, I will likely need to use multiple frequency channels and average them to smooth out the severe multipath fading before passing the vector into the Tikhonov solver."