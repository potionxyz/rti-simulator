# glossary: symbols, terms and architecture overview

when you come back to this project in six months (or in an interview, under pressure), you will forget what half the variables stand for. this page is the single reference for every mathematical symbol, term of art, and class name used anywhere in the project.

---

## mathematical symbols

| symbol | name | meaning |
| --- | --- | --- |
| $\mathbf{x}$ | attenuation vector | 1D flat vector of length $N$ holding the attenuation value of every voxel. this is what we want to reconstruct. |
| $\hat{\mathbf{x}}$ | reconstructed $\mathbf{x}$ | the solver's estimate of $\mathbf{x}$. with a "hat" = computed, not true. |
| $\mathbf{y}$ | measurement vector | 1D vector of length $M$. one entry per radio link. $y_i$ is the signal loss on link $i$ in dB. |
| $\Delta y$ | signal change | same as $\mathbf{y}$ in this project — we always work in differential measurements, baseline subtracted. |
| $\mathbf{W}$ | weight matrix | $M \times N$ matrix. $W_{ij}$ = distance link $i$ travels through voxel $j$, in metres. |
| $\mathbf{W}^T$ | transpose | $N \times M$ matrix. used in normal equations and back-projection. |
| $\mathbf{W}^T\mathbf{W}$ | normal matrix | $N \times N$ symmetric positive-semi-definite matrix. required by least squares. |
| $\mathbf{I}$ | identity matrix | $N \times N$. ones on the diagonal, zeros everywhere else. |
| $\lambda$ | regularisation strength | scalar. tuning knob for Tikhonov. higher $\lambda$ = smoother, lower $\lambda$ = closer to raw least squares. |
| $\kappa$ | condition number | scalar. measures how much a small change in $\mathbf{y}$ gets amplified in $\mathbf{x}$. we want it low. |
| $n_i$ | noise on link $i$ | scalar. Gaussian with $\mu = 0$ and $\sigma = $ hardware stddev in dB. |
| $\alpha$ | parametric line variable | scalar in $[0, 1]$. used by Siddon's algorithm — $\alpha = 0$ is the start of the link, $\alpha = 1$ is the end. |
| $N$ | number of voxels | total voxel count = $N_x \times N_y \times N_z$. e.g. $10^3 = 1000$. |
| $N_x, N_y, N_z$ | per-axis voxel counts | number of voxels along each axis. |
| $M$ | number of link measurements | for $n$ nodes, $M = \binom{n}{2} = n(n-1)/2$. for 8 nodes, $M = 28$. |
| $L_i$ | link path | the straight line from node $a$ to node $b$ that defines link $i$. |
| $a(r)$ | continuous attenuation field | the idealised, infinite-precision attenuation at point $r$ in space. we discretise it into $\mathbf{x}$. |
| $\|\mathbf{v}\|_2$ | L2 norm | euclidean length of vector $\mathbf{v}$. $\sqrt{\sum v_i^2}$. |
| $\|\mathbf{v}\|_2^2$ | squared L2 norm | $\sum v_i^2$. used in least squares because the square root is monotonic and squaring removes it. |
| $\nabla_{\mathbf{x}}$ | gradient with respect to $\mathbf{x}$ | vector of partial derivatives. setting it to $\mathbf{0}$ finds a minimum. |
| $i$ | voxel linear index | $i = x + yN_x + zN_xN_y$. integer in $[0, N)$. |

---

## physics terms

| term | meaning |
| --- | --- |
| RSS / RSSI | Received Signal Strength (Indicator). the power of a received radio signal in dBm. what the ESP32 reports. |
| dB / dBm | Decibel (absolute milliwatts). logarithmic power unit. $-30$ dBm means strong, $-90$ dBm means barely-there. |
| Line of Sight (LOS) | a straight unbroken path between two nodes. the forward model assumes all signals travel along LOS only. |
| Path Loss | total power lost along a link, measured in dB. the sum of free-space loss + attenuation from obstacles. |
| Free-Space Path Loss | power loss due purely to distance, following the inverse square law. cancels out in differential measurements. |
| Attenuation | power loss caused by physical matter in the path (bodies, walls, furniture). this is what we care about. |
| Multipath Fading | the chaos caused by a signal bouncing off walls, floors and ceilings and arriving at the receiver in multiple copies that interfere with each other. the biggest real-world enemy of this simulator. |
| Fresnel Zone | the ellipsoidal region of space around a link that actually contributes to signal propagation. strict LOS is a 1D approximation of this. |
| Shadow Fading | signal loss from a large obstacle blocking the direct path. the linear model we use assumes all loss is shadow fading. |

---

## mathematical concepts

| concept | meaning |
| --- | --- |
| Ill-Posed Problem | a system where the solution either doesn't exist, isn't unique, or changes wildly with tiny changes in input. our inverse problem is all three. |
| Underdetermined System | more unknowns than equations. $28$ measurements, $1000$ voxels. infinitely many valid solutions. |
| Normal Equations | $\mathbf{W}^T\mathbf{W}\mathbf{x} = \mathbf{W}^T\mathbf{y}$. the minimum-squared-error solution to $\mathbf{Wx} = \mathbf{y}$. |
| Least Squares | the method of finding $\mathbf{x}$ that minimises $\|\mathbf{Wx} - \mathbf{y}\|_2^2$. |
| Pseudoinverse | the generalisation of matrix inverse to non-square matrices. $\mathbf{W}^+ = (\mathbf{W}^T\mathbf{W})^{-1}\mathbf{W}^T$. |
| Regularisation | adding a penalty term to an optimisation to force physically reasonable solutions. |
| Tikhonov Regularisation | aka Ridge Regression. adds $\lambda\|\mathbf{x}\|_2^2$ as the penalty. |
| L1 vs L2 Norm | L1 = sum of absolute values (promotes sparsity). L2 = sum of squares (promotes smoothness). we use L2. |
| Ridge Regression | the statistics name for Tikhonov regularisation. same equation. |
| LASSO | regression with L1 penalty. promotes sparse solutions. not used here because L1 is not differentiable at zero. |
| Conjugate Gradient | iterative solver for $\mathbf{A}\mathbf{x} = \mathbf{b}$. scales to millions of voxels. overkill for our 1000-voxel problem. |
| Cholesky Decomposition | factorise $\mathbf{A} = \mathbf{L}\mathbf{L}^T$. the fastest way to solve symmetric positive-definite systems. what we actually use. |

---

## algorithms

| algorithm | what it does | where it's used |
| --- | --- | --- |
| Siddon's Algorithm | finds the exact sequence of voxels a line crosses and the chord length inside each one, using parametric line equations. $O(N_x + N_y + N_z)$. | building the rows of $\mathbf{W}$ in `ForwardModel` |
| Ray Marching | steps along a line in fixed increments (e.g. 1mm) to find intersections. $O(d / \Delta t)$. | not used — too slow |
| Back-Projection | computes $\hat{\mathbf{x}} = \mathbf{W}^T\mathbf{y}$. the quick-and-dirty reconstruction. | `Reconstructor::back_projection()` |
| Tikhonov Solve | computes $\hat{\mathbf{x}} = (\mathbf{W}^T\mathbf{W} + \lambda\mathbf{I})^{-1}\mathbf{W}^T\mathbf{y}$ via Cholesky. | `Reconstructor::tikhonov()` |
| Centre-of-Mass | averages the 3D coordinates of all voxels above a threshold, weighted by their values. gives a single $(x,y,z)$ localisation. | `Reconstructor::evaluate_auto_threshold()` |
| Auto-Threshold | computes a threshold as a fraction (default 30%) of the peak reconstructed attenuation. | evaluation pipeline |

---

## software components

| class | file | role |
| --- | --- | --- |
| `VoxelGrid` | `voxel_grid.h/.cpp` | holds the discretised room as a flat 1D vector. provides `at(x,y,z)` and `get_index(x,y,z)` with bounds checking. |
| `Node` | `node.h/.cpp` | represents a single ESP32 transceiver at a given `(x,y,z)`. |
| `NodeNetwork` | `node.h/.cpp` | holds the list of nodes and generates the 28 unique link pairs. has a `create_3d_cube` factory for the default perimeter setup. |
| `Scene` | `scene.h/.cpp` | ground-truth builder. has factory methods like `create_single_person_centre()` and `create_two_people()` that stamp voxel blocks into a `VoxelGrid`. |
| `ForwardModel` | `forward_model.h/.cpp` | builds $\mathbf{W}$ using Siddon's algorithm, then runs `simulate()` to produce $\mathbf{y}$ from a populated grid. injects Gaussian noise. |
| `Reconstructor` | `reconstructor.h/.cpp` | implements the three reconstruction methods. caches $\mathbf{W}^T$ and $\mathbf{W}^T\mathbf{W}$. provides evaluation with MSE, localisation error, and TP/FP/FN. |
| `Visualiser` | `visualiser.h/.cpp` | writes ASCII slices, PPM images, and self-contained Three.js HTML from a `VoxelGrid`. |

---

## library / tooling

| name | purpose |
| --- | --- |
| Eigen | header-only C++ linear algebra library. provides `VectorXd`, `MatrixXd`, `SparseMatrix`, `.llt()`, `.ldlt()`, `.transpose()`, etc. |
| nlohmann/json | header-only JSON parser. used to load scene definitions from `scenes/*.json`. |
| CMake | cross-platform build system. generates the Makefile. |
| C++17 | language standard. needed for `std::optional`, structured bindings, `<filesystem>`, etc. |
| Three.js | JavaScript 3D library. loaded from a CDN inside the generated HTML viewers. |
| OrbitControls | Three.js extension for mouse-driven camera rotation/zoom. |

---

## architecture diagram

here is the whole data pipeline at a glance.

```
                            ┌──────────────────────────────┐
                            │    main.cpp (entry point)    │
                            └──────────────┬───────────────┘
                                           │
                                           ▼
                      ┌──────────────────────────────────────┐
                      │ stage 1: spatial container           │
                      │ VoxelGrid + NodeNetwork              │
                      │ (1000 voxels, 8 nodes, 28 links)     │
                      └──────────────┬───────────────────────┘
                                     │
                                     ▼
                      ┌──────────────────────────────────────┐
                      │ stage 2: build physics (once)        │
                      │ ForwardModel uses Siddon's           │
                      │ to fill W  (28 × 1000 matrix)        │
                      └──────────────┬───────────────────────┘
                                     │
                                     ▼
                      ┌──────────────────────────────────────┐
                      │ stage 3: pre-compute (once)          │
                      │ Reconstructor caches Wᵀ and WᵀW      │
                      └──────────────┬───────────────────────┘
                                     │
       ┌─────────────────────────────┤
       │                             │
       ▼                             ▼
┌─────────────────┐        ┌──────────────────────┐
│ Scene factory   │        │ (per test scene loop)│
│ create_person…  │        └──────────┬───────────┘
│ create_two…     │                   │
└────────┬────────┘                   │
         │                            │
         │ apply_to_grid              │
         ▼                            │
┌─────────────────┐                   │
│   VoxelGrid     │                   │
│  (ground truth) │◀──────────────────┘
└────────┬────────┘
         │
         │  forward.simulate()
         │  y = Wx + noise
         ▼
┌─────────────────┐       ┌──────────────────────────────┐
│   y (28 × 1)    │──────▶│ Reconstructor solves for x̂ │
└─────────────────┘       │  • back_projection(y)        │
                          │  • least_squares(y)          │
                          │  • tikhonov(y, λ)            │
                          └──────────────┬───────────────┘
                                         │
                                         ▼
                            ┌──────────────────────────┐
                            │       x̂ (1000 × 1)      │
                            └──────────────┬───────────┘
                                           │
                    ┌──────────────────────┼───────────────────────┐
                    │                      │                       │
                    ▼                      ▼                       ▼
          ┌──────────────────┐   ┌──────────────────┐   ┌──────────────────┐
          │ evaluate vs      │   │ apply_to_grid    │   │ Visualiser       │
          │ ground truth     │   │ (for viewing)    │   │ ASCII / PPM /    │
          │ (MSE, loc err,   │   │                  │   │ Three.js HTML    │
          │  TP/FP/FN)       │   │                  │   │                  │
          └──────────────────┘   └──────────────────┘   └──────────────────┘
```

the critical dependency rule: **every arrow points forward.** nothing upstream of the solver (voxel grid, forward model, weight matrix) ever knows anything downstream (ground truth comparison, visualisation). the reconstructor is a strict black box that only sees $\mathbf{y}$ and $\mathbf{W}$, never the real scene.

---

## interview cheat sheet

if you only remember four equations walking into a viva:

1. **forward model:** $\mathbf{y} = \mathbf{Wx} + \mathbf{n}$
2. **normal equations:** $\mathbf{W}^T\mathbf{Wx} = \mathbf{W}^T\mathbf{y}$
3. **tikhonov solution:** $\hat{\mathbf{x}} = (\mathbf{W}^T\mathbf{W} + \lambda\mathbf{I})^{-1}\mathbf{W}^T\mathbf{y}$
4. **voxel index mapping:** $i = x + yN_x + zN_xN_y$

and one sentence: "i discretise the room into voxels, build $\mathbf{W}$ with Siddon's algorithm, simulate noisy measurements, then recover the voxel attenuation via Tikhonov-regularised least squares because the straight pseudoinverse is numerically unstable."

everything else is a variation on those. if that makes sense.
