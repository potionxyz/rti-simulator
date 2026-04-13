# 05-cpp-control-flow: from maths to executable

all the previous modules have been maths. module 01 set up voxel memory, 02 derived the forward model, 03 built $\mathbf{W}$, 04 killed the ill-posed inverse problem with Tikhonov. but none of that actually *runs* on its own. the C++ has to tie every stage together into a single executable pipeline.

this module traces a single run of the simulator end-to-end, so you can look at `main.cpp` and know exactly which mathematical stage you are standing in at every line. if that makes sense.

---

## the five classes (quick recap)

| class | responsibility | key state |
| --- | --- | --- |
| `VoxelGrid` | discretised 3D room | flat `std::vector<double>` of attenuation |
| `NodeNetwork` | the 8 ESP32 node positions | list of `(x,y,z)` coordinates |
| `Scene` | ground truth builder | stacks voxel blocks (humans, walls) into the grid |
| `ForwardModel` | physics engine | holds the weight matrix $\mathbf{W}$ |
| `Reconstructor` | inverse solver | holds $\mathbf{W}$, $\mathbf{W}^T$, $\mathbf{W}^T\mathbf{W}$ (pre-computed) |
| `Visualiser` | output layer | writes ASCII slices, PPM images, Three.js HTML |

the dependency graph is strictly one-way: `Scene` only knows about `VoxelGrid`, `ForwardModel` only knows about `VoxelGrid` and `NodeNetwork`, and `Reconstructor` only knows about `ForwardModel`. the solver never sees the ground truth. that's by design. if i let the inverse solver peek at the scene, the whole experiment would be cheating.

---

## the execution pipeline

here is what actually happens when you run `./rti-simulator`, in order.

### stage 1: construct the spatial container

```cpp
double room_x = 2.0, room_y = 2.0, room_z = 2.0;
double voxel_size = 0.2;

VoxelGrid grid(room_x, room_y, room_z, voxel_size);
auto network = NodeNetwork::create_3d_cube(room_x, room_y, room_z);
```

this allocates the flat 1D voxel vector (1000 floats for a 10×10×10 grid) in one contiguous heap block using `std::vector<double>`, and places 8 nodes at the corners of the room. total RAM so far: roughly 8KB for the grid, nothing for the network. the room is empty.

### stage 2: build the forward model (the physics)

```cpp
ForwardModel forward(grid, network);
```

this is the heavy step, but it only runs **once**. the constructor loops over every one of the 28 links ($\binom{8}{2} = 28$), runs Siddon's algorithm between the two node endpoints, and fills in one row of $\mathbf{W}$ per link. the result is a $28 \times 1000$ dense `Eigen::MatrixXd`.

the matrix is actually mostly zeros (a straight line only hits about 15-20 voxels in a 10×10×10 grid), but in this project it's stored dense because 1000 voxels is small. if i scaled up to $30^3 = 27{,}000$ voxels, i would swap to `Eigen::SparseMatrix` and CSR format (see `03-linear-algebra.md`).

**critical:** $\mathbf{W}$ depends only on the *geometry* (room dimensions, voxel size, node positions). it has zero knowledge of what's actually inside the room. this is why it only needs to be built once, even if i later run 50 different scenes through it.

### stage 3: pre-compute the reconstructor matrices

```cpp
Reconstructor recon(forward, grid);
```

the Tikhonov solution $\mathbf{x} = (\mathbf{W}^T\mathbf{W} + \lambda\mathbf{I})^{-1}\mathbf{W}^T\mathbf{y}$ has two expensive terms that also depend only on geometry: $\mathbf{W}^T$ and $\mathbf{W}^T\mathbf{W}$. the reconstructor constructor computes both once and caches them, so every subsequent `tikhonov()` call only has to:

1. add $\lambda\mathbf{I}$ to the cached $\mathbf{W}^T\mathbf{W}$
2. invert
3. multiply by the cached $\mathbf{W}^T\mathbf{y}$

this is the most important architectural choice in the whole project. it turns a 100ms operation into a 1ms operation, which is what makes λ sweeps (running 4 different λ values on the same data) actually tolerable.

### stage 4: run a scene

```cpp
auto scene = Scene::create_single_person_centre();
scene.apply_to_grid(grid);

Eigen::VectorXd y = forward.simulate(grid, 0.0);
```

`Scene` writes attenuation values into the voxel grid (for a single person, that's a $0.4\text{m} \times 0.4\text{m} \times 1.8\text{m}$ block at the centre, filled with attenuation $= 2.0$). `forward.simulate()` then multiplies $\mathbf{W}$ by the flat voxel vector to get $\mathbf{y}$, and adds Gaussian noise if requested. this is the synthetic data the solver will see.

### stage 5: reconstruct

```cpp
Eigen::VectorXd x_bp  = recon.back_projection(y);
Eigen::VectorXd x_tik = recon.tikhonov(y, 1.0);
```

three reconstruction methods are implemented in `reconstructor.cpp` (see section below for internals). each one takes $\mathbf{y}$ and produces a new estimate $\hat{\mathbf{x}}$ of what the 1000-voxel grid should look like. the solver is blind to the ground truth at this stage.

### stage 6: evaluate

```cpp
auto r = recon.evaluate_auto_threshold(x_tik, grid, "tikhonov(1.0)");
```

this compares the reconstructed $\hat{\mathbf{x}}$ back against the true grid to compute:

- **MSE** — mean squared error across all 1000 voxels
- **localisation error** — euclidean distance between true centre of mass and reconstructed centre of mass
- **true positives / false positives / false negatives** — by thresholding "occupied" voxels at 30% of the reconstructed peak

this is the only place the solver's output ever sees the ground truth, and it's strictly post-hoc, so the maths is not contaminated.

### stage 7: visualise

```cpp
recon.apply_to_grid(x_tik, recon_output);
Visualiser::export_3d_html(recon_output, network, "output_recon_3d.html", "title");
```

the reconstructed vector gets mapped back into a `VoxelGrid`, and the visualiser writes three outputs: an ASCII slice to terminal, a `.ppm` image, and a self-contained Three.js HTML file.

---

## the three reconstruction methods

`Reconstructor` exposes three solvers so you can compare them against the same data. each is a single function in `reconstructor.cpp`.

### method 1: back-projection

```cpp
Eigen::VectorXd Reconstructor::back_projection(const Eigen::VectorXd& y) {
    return Wt_ * y;
}
```

one line. it's literally just $\hat{\mathbf{x}} = \mathbf{W}^T \mathbf{y}$. the intuition: for every link $i$, take its measured attenuation $y_i$, and smear it back along every voxel that the link passes through, weighted by the path length.

this is not the correct inverse. it's a rough "spray-back" estimate. but it's numerically bulletproof (no matrix inversion) and gives a sanity-check baseline. if back-projection can't even find the blob, something is wrong upstream in the forward model.

### method 2: least squares

$$\hat{\mathbf{x}} = (\mathbf{W}^T\mathbf{W})^{-1}\mathbf{W}^T\mathbf{y}$$

this is the "textbook" pseudoinverse. it's the theoretically optimal estimate if there's zero noise. in practice it explodes because $\mathbf{W}^T\mathbf{W}$ is near-singular — the condition number is astronomical, so any tiny noise in $\mathbf{y}$ gets amplified into $\pm 10{,}000$ spikes across the output. included for comparison only.

### method 3: tikhonov regularisation

```cpp
Eigen::VectorXd Reconstructor::tikhonov(const Eigen::VectorXd& y, double lambda) {
    Eigen::VectorXd Wt_y = Wt_ * y;
    int n = forward_.num_voxels();
    Eigen::MatrixXd regularised = WtW_ + lambda * Eigen::MatrixXd::Identity(n, n);
    Eigen::LDLT<Eigen::MatrixXd> solver(regularised);
    Eigen::VectorXd x = solver.solve(Wt_y);
    return clamp_non_negative(x);
}
```

the actual workhorse. same equation as module 04:

$$\hat{\mathbf{x}} = (\mathbf{W}^T\mathbf{W} + \lambda\mathbf{I})^{-1}\mathbf{W}^T\mathbf{y}$$

critically, i do **not** call `.inverse()` on the matrix. i use `LDLT` — the robust Cholesky variant which factorises $\mathbf{A} = \mathbf{L}\mathbf{D}\mathbf{L}^T$ (where $\mathbf{L}$ is unit lower-triangular and $\mathbf{D}$ is diagonal) and then solves by direct substitution.

why LDLT specifically? there are two related decompositions:

- **LLT (plain Cholesky)** — $\mathbf{A} = \mathbf{L}\mathbf{L}^T$. slightly faster, but strictly requires the matrix to be positive-*definite* (every eigenvalue $> 0$). if even one eigenvalue is effectively zero, LLT fails.
- **LDLT (robust Cholesky)** — $\mathbf{A} = \mathbf{L}\mathbf{D}\mathbf{L}^T$. handles positive-*semi-definite* matrices, including rank-deficient cases. the extra diagonal $\mathbf{D}$ absorbs zero eigenvalues without breaking the decomposition.

$\mathbf{W}^T\mathbf{W}$ is rank-deficient when the system is underdetermined (which it always is for us — 28 measurements, 1,000 unknowns). adding $\lambda\mathbf{I}$ mathematically makes it positive-definite, but if $\lambda$ is tiny or you're exploring edge cases with $\lambda = 0$, LLT can still numerically fail. LDLT handles all of that gracefully. this is exactly why Eigen's `ldlt()` is the textbook choice for regularised least squares.

why *any* Cholesky instead of a general inverse? two reasons:

1. **speed.** for an $N \times N$ symmetric positive-semi-definite matrix, Cholesky-family decomposition is about twice as fast as LU decomposition and roughly three times faster than forming `A.inverse()` explicitly.
2. **stability.** forming an explicit inverse and then multiplying it against $\mathbf{b}$ introduces extra floating-point rounding. Cholesky substitution does the same maths with fewer intermediate values, so the answer is numerically cleaner.

### why not ConjugateGradient?

module 04 references `Eigen::ConjugateGradient`. it's the textbook answer for large sparse systems because it iteratively solves $\mathbf{A}\mathbf{x} = \mathbf{b}$ without ever forming $\mathbf{A}^{-1}$, and each iteration is just a sparse matrix-vector multiply. it scales brilliantly to millions of voxels.

but at 1,000 voxels, the overhead of CG's iteration loop actually makes it slower than a direct LDLT on dense Eigen. i'd switch to CG the moment i moved to a $30^3 = 27{,}000$ or larger grid, because dense LDLT scales as $O(N^3)$ — at 27,000 voxels the solve would take minutes and eat gigabytes of RAM. for the current 1,000-voxel deployment, dense LDLT is the right call.

---

## the eigen library: what you're actually using

Eigen is a header-only C++ template library for linear algebra. "header-only" means no `libeigen.so` to link — the compiler inlines every templated operation at compile time. this is what lets `(WtW_ + lambda * Identity).llt().solve(Wt_ * y)` compile down to tightly-optimised assembly with no virtual function calls.

the three Eigen types you care about in this project:

- **`Eigen::VectorXd`** — a column vector of doubles with runtime-dynamic size. used for $\mathbf{x}$ (1000 elements) and $\mathbf{y}$ (28 elements).
- **`Eigen::MatrixXd`** — a dense matrix of doubles with runtime-dynamic size. used for $\mathbf{W}$, $\mathbf{W}^T$, and $\mathbf{W}^T\mathbf{W}$.
- **`Eigen::SparseMatrix<double>`** — CSR-format sparse matrix. currently unused (grid is too small) but would become essential at $27{,}000+$ voxels.

the three Eigen operations that matter most:

- **`.transpose()`** — returns a transposed view without copying data (it's a lazy expression).
- **`.ldlt()`** — robust Cholesky (LDL^T) decomposition that works even when the matrix is only positive semi-definite. this is what the tikhonov solver actually uses.
- **`.llt()`** — plain Cholesky. slightly faster than LDLT but requires strict positive-definiteness. acceptable alternative when $\lambda$ is safely large.

---

## end-to-end timing (rough, from my laptop)

on a $10 \times 10 \times 10$ grid with 8 nodes (28 links), one full pipeline run:

| stage | time |
| --- | --- |
| `ForwardModel` construction (Siddon's on 28 links) | ~2 ms |
| `Reconstructor` pre-compute ($\mathbf{W}^T\mathbf{W}$) | <1 ms |
| `forward.simulate()` per scene | ~0.1 ms |
| `recon.tikhonov()` per call | ~1 ms |
| `evaluate_auto_threshold()` | ~0.2 ms |

the whole five-scene test suite (TESTS 1 through 5 in `main.cpp`, including the λ sweep in test 1) finishes in under 50ms. this is the payoff for writing it in C++ with pre-computation and Cholesky instead of python with naive inversion. if that makes sense.

---

## what this gives you

when you look at `main.cpp` now, you should see it as a series of stages, not a wall of function calls:

1. build the room (grid + nodes)
2. build the physics (forward model + cached reconstructor matrices)
3. for each test scene: apply ground truth → simulate $\mathbf{y}$ → reconstruct → evaluate
4. generate visualisations from the last scene

every test case is just a different starting scene piped through the same unchanged physics and solver. that's why the architecture holds up: the heavy maths is built once, and scenes are cheap to swap in.

next module: `06-visualisation.md` — how the raw `Eigen::VectorXd` becomes an interactive 3D browser viewer.
