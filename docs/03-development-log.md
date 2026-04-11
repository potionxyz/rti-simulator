# Development Log

## 11 april 2026

### session 1: project setup and core data structures

so first thing i needed to figure out was representing 3d space in code. i went with a voxel grid - basically dividing the room into small cubes and storing an attenuation value for each one.

the key decision was using a flat `std::vector<double>` instead of a 3D array. this might seem counterintuitive since we're modelling a 3D space, but it actually makes everything cleaner:
- it's cache-friendly because all the data sits contiguously in memory
- it maps directly to the column vector x in the reconstruction equation y = Wx
- Eigen (the linear algebra library) works with flat vectors, so no conversion needed

for the grid dimensions, i'm using `std::ceil` when dividing room size by voxel size. this means if the room doesn't divide evenly (say 2.15m with 0.1m voxels), we get 22 voxels instead of 21, so nothing gets cut off at the edges.

then i set up the node network. each node is an ESP32 sensor at a known position. the important bit is the link generation - with N nodes you get N*(N-1)/2 unique links. so 8 nodes gives 28 links, which is a lot of data to work with for reconstruction.

i built three preset configurations:
- 4-node 2D square (for basic testing)
- 8-node 3D cube (the main one, matches the physical hardware plan)
- custom perimeter layout (for testing how node count affects accuracy)

tested it by placing a simulated person in the centre of a 2m cube. 204 voxels occupied out of 8000 total at 0.1m resolution, which looks about right for a human-shaped region (0.4m x 0.3m x 1.7m).

### session 2: scene builder and forward model

next step was building the physics engine. i needed two things: a way to define test scenes (where are the people and walls?) and a way to simulate what the sensors would measure.

for scenes, i created an Obstacle struct with a bounding box and attenuation value. typical values at 2.4GHz are about 4 dB/m for a human body (all that water content absorbs RF), 2-3 dB/m for wood, and 10-15 dB/m for concrete. built five preset scenes for testing: empty room, person in centre, person in corner, two people, and person behind a wall.

the forward model is where the real maths comes in. i implemented Siddon's algorithm from a 1985 medical physics paper. it traces a ray from one node to another through the voxel grid, finding exactly which voxels the ray passes through and for what length. it's O(nx + ny + nz) per ray, which is way faster than checking every voxel.

i manually verified the maths: for the space diagonal link 0-7 through a person at centre, the calculated attenuation was 2.078 dB. working it out by hand:
- diagonal length = sqrt(4+4+4) = 3.464m
- path through person limited by y-constraint: t range 0.425 to 0.575
- path length = 0.15 * 3.464 = 0.520m
- attenuation = 0.520 * 4.0 = 2.078 dB ✓

the weight matrix came out as 28x8000 with only 560 non-zero entries (0.2% dense). storing it as a sparse matrix saves significant memory.

### session 3: reconstruction engine

this was the hardest part. the problem is fundamentally underdetermined: 28 measurements, 1000+ unknowns. you can't just invert the matrix.

i implemented three methods:
1. **back-projection** (x = W^T * y): fast and always works, but produces a blurry result because it "sprays" each measurement across all voxels on that link's path
2. **least-squares**: mathematically optimal but numerically unstable because W^T W is singular (rank 28, size 1000x1000)
3. **Tikhonov regularisation** (x = (W^T W + lambda*I)^{-1} W^T y): the best method. adding lambda*I makes the matrix invertible and controls the noise-resolution trade-off

key findings from testing:
- person in centre: localisation error 0.025m (back-proj) to 0.061m (Tikhonov lambda=1.0)
- noise degrades gracefully: 0.5dB noise gives 0.014m error, 2dB gives 0.43m
- corner detection is harder (0.66m error) because fewer links cross the corners
- through-wall detection fails with concrete (12 dB/m), which makes sense as the wall dominates the attenuation

one issue: the 8000x8000 matrix (W^T W at 0.1m resolution) was too large for direct solve on this machine. i reduced to 0.2m voxels (1000 total) which keeps the matrix at 1000x1000, manageable for LDLT decomposition. for higher resolution, i'd need iterative solvers like conjugate gradient.

### session 4: visualisation

built three output modes:
1. ASCII terminal art using density characters (' ', '.', ':', '+', '#', '@')
2. PPM image export with a heat map colour gradient (black -> blue -> cyan -> green -> yellow -> red)
3. interactive 3D HTML viewer using Three.js

the 3D viewer was the most satisfying part. it generates a standalone HTML file that renders voxel cubes with opacity proportional to attenuation, blue spheres for sensor nodes, and faint grey lines for radio links. you can orbit around the reconstruction with mouse controls.

the comparison viewer shows ground truth on the left and reconstruction on the right, which makes it immediately obvious where the reconstruction succeeds and where it struggles.

deployed the viewers to vercel so they're accessible in a browser without needing to build the project.
