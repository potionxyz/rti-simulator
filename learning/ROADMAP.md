# syllabus: imperial eie rti mastery

this is the master syllabus for understanding the radio tomographic imaging (rti) simulator. it is structured like a university module specification, designed specifically to bridge your a-level further maths and physics knowledge into applied c++ software engineering.

to defend this at an imperial interview, you cannot just know the code. you must understand the mathematical limits of the physical model.

work through these modules in order.

---

## module 1: mathematical foundations
**a-level crossover:** further pure maths (matrices, vectors), core maths (calculus, geometry).
**objective:** build the mental scaffolding required to understand n-dimensional linear algebra and multivariate calculus before writing any code.

*   **topics covered:**
    *   3d vector spaces and scalar/dot products ($\mathbf{a} \cdot \mathbf{b}$).
    *   matrix multiplication constraints (why an $m \times p$ matrix can only multiply a $p \times n$ matrix).
    *   the $l_2$-norm (euclidean distance/magnitude) and squared residuals.
    *   partial derivatives and gradients (finding the minimum of a scalar field).
*   **reading:** `00-prerequisites.md`
*   **checkpoint:** can you explain why minimizing a squared function requires setting its first derivative to zero?

## module 2: spatial discretisation & memory architecture
**a-level crossover:** computer science (data structures, memory), maths (coordinate geometry).
**objective:** understand how continuous physical space is handled by discrete computer memory, and why c++ cache locality dictates algorithm design.

*   **topics covered:**
    *   discretizing continuous volumes into cubic metric voxels (finite integration).
    *   the catastrophic memory fragmentation (cpu l1/l2 cache misses) of nested 3d arrays (`vector<vector<vector>>`).
    *   bijective mapping: squashing 3d coordinates $(x,y,z)$ into a 1d linear memory block.
    *   the spatial index formula: $i = x + yN_x + zN_xN_y$.
    *   reverse mapping using integer division and modulo operators.
*   **reading:** `01-voxel-maths.md`
*   **checkpoint:** without looking at the formula, can you derive the 1d index for a voxel given the room dimensions?

## module 3: the physics of rf attenuation (forward model)
**a-level crossover:** physics (waves, attenuation, inverse-square law), maths (parametric lines).
**objective:** simulate the physical propagation of 2.4ghz wi-fi signals through physical matter using exact geometric ray-tracing.

*   **topics covered:**
    *   line integrals: $\Delta y = \int a(r)dr$ (continuous loss across a path).
    *   discrete summations: translating the integral into $\Delta y = \sum W_{ij}x_j$.
    *   parametric 3d line equations ($x(\alpha), y(\alpha), z(\alpha)$).
    *   siddon's exact ray-tracing algorithm (calculating intersection lengths without iterative ray-marching).
    *   empirical hardware constraints: injecting normal/gaussian white noise to simulate thermal variance and multipath fading.
*   **reading:** `02-forward-model.md`
*   **checkpoint:** why is stepping 1mm at a time along a ray computationally worse than siddon's parametric intersection method?

## module 4: linear algebra & sparse systems
**a-level crossover:** further maths (matrices, systems of linear equations).
**objective:** cast the physical ray-tracing data into a massive linear system ($\mathbf{y} = \mathbf{Wx}$) and handle the memory architecture of large-scale matrices.

*   **topics covered:**
    *   constructing the weight matrix $\mathbf{W}$ (rows = links, columns = voxels).
    *   dimensionality traps: why an underdetermined system ($28 \times 27000$) has no exact inverse.
    *   sparse vs dense matrices: why storing $99.8\%$ zeros destroys ram.
    *   compressed sparse row (csr) format in the c++ `eigen` library.
*   **reading:** `03-linear-algebra.md`
*   **checkpoint:** what is the exact physical meaning of a single numerical value stored at $W_{row=5, col=1024}$?

## module 5: inverse problems & regularisation
**a-level crossover:** further maths (data fitting, regression), statistics.
**objective:** solve the unsolvable matrix. mathematically reverse the physics engine to calculate the 3d room given only noisy 1d sensor data.

*   **topics covered:**
    *   least squares minimization: $\min ||\mathbf{Wx} - \mathbf{y}||_2^2$.
    *   ill-posed matrices and high condition numbers: why the normal equations $(\mathbf{W}^T\mathbf{W})^{-1}$ amplify 1db of hardware noise into infinity.
    *   tikhonov regularisation (ridge regression): adding a mathematical penalty to force physical smoothness.
    *   the regularized equation: $\mathbf{x} = (\mathbf{W}^T\mathbf{W} + \lambda \mathbf{I})^{-1}\mathbf{W}^T\mathbf{y}$.
    *   tuning the $\lambda$ parameter to balance data fidelity against noise suppression.
*   **reading:** `04-tikhonov-regularisation.md`
*   **checkpoint:** physically, what happens to the output 3d image if you set lambda ($\lambda$) to $0.0$? what happens if you set it to $1,000,000$?

## module 6: software engineering & visualisation
**a-level crossover:** computer science (architecture, data formats).
**objective:** pipeline the floating-point matrix outputs into human-readable 3d graphics by decoupling the c++ backend from a web frontend.

*   **topics covered:**
    *   normalising float scalars into rgb integer bytes ($0-255$) for 2d heatmaps.
    *   thresholding attenuation values to locate solid matter (humans).
    *   exporting 1d vectors back into 3d spatial point clouds.
    *   json serialization and decoupling c++ math libraries from javascript/three.js renderers.
*   **reading:** `06-visualisation.md`
*   **checkpoint:** why is it bad architectural design to write an opengl renderer directly inside the c++ solver class?

## module 7: interview defense & critical evaluation
**objective:** subject the entire project to extreme scrutiny. understand its physical limitations and alternative engineering choices.

*   **topics covered:**
    *   defending c++ over python for inverse sparse problems.
    *   the threat of multipath fading in true enclosed physical spaces.
    *   computational trade-offs: voxel resolution vs conjugate gradient solver execution time.
*   **reading:** 
    *   `critical-evaluation/core-defenses.md`
    *   `critical-evaluation/maths-you-must-know.md` 
    *   `critical-evaluation/trade-off-questions.md` 
    *   `critical-evaluation/worst-case-scenarios.md` 
*   **checkpoint:** if an imperial professor says "your model assumes a direct line of sight, but wifi bounces. why didn't you model the bounces?", what is your exact, technical defense?

---
*study discipline:* do not move to the next module until you can answer the checkpoint question cleanly, out loud, without reading the notes.