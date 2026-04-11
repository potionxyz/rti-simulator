# RTI 3D Volumetric Simulator

a C++ simulator for 3D radio tomographic imaging (RTI) - reconstructing images of objects in a room by analysing how radio signals attenuate between pairs of wireless sensor nodes.

think of it like a CT scan, but with WiFi signals instead of X-rays.

## what it does

- simulates a network of wireless sensor nodes placed around a 3D space
- models how radio signals attenuate when passing through objects (people, walls)
- reconstructs a 3D voxel image of the environment from signal measurements
- visualises the reconstruction vs ground truth in terminal, 2D images, and interactive 3D

## results

| scene | localisation error | method |
|---|---|---|
| person in centre | 0.025m | back-projection |
| person in centre | 0.061m | Tikhonov (lambda=1.0) |
| two people | 0.114m | Tikhonov (lambda=1.0) |
| person in corner | 0.659m | Tikhonov (lambda=1.0) |

interactive 3D viewer: [0xnath-rti.vercel.app](https://0xnath-rti.vercel.app)

## the maths

the core problem is an inverse problem: given signal measurements **y** and a weight matrix **W**, recover the attenuation image **x**:

```
y = Wx + noise
```

solved using Tikhonov regularisation:

```
x = (W^T W + lambda * I)^{-1} W^T y
```

weight matrix W is computed using Siddon's ray-voxel intersection algorithm (Medical Physics, 1985).

## building

requires CMake 3.16+ and a C++17 compiler.

```bash
# download dependencies (header-only)
./setup.sh

# build
mkdir build && cd build
cmake ..
make

# run
./rti-simulator
```

## dependencies

all header-only (downloaded by setup.sh):
- [Eigen](https://eigen.tuxfamily.org/) - linear algebra (matrices, solvers)
- [nlohmann/json](https://github.com/nlohmann/json) - scene configuration files

## project structure

```
src/
  voxel_grid.cpp/h    - 3D voxel grid representation
  node.cpp/h          - sensor nodes and radio links
  scene.cpp/h         - test scene builder with JSON I/O
  forward_model.cpp/h - signal propagation (Siddon's algorithm)
  reconstructor.cpp/h - inverse problem solver (Tikhonov, back-projection)
  visualiser.cpp/h    - ASCII, PPM image, and 3D HTML output
  main.cpp            - simulation pipeline

docs/
  01-analysis.md      - problem definition, research, requirements
  02-design.md        - architecture, algorithms, class design
  03-development-log.md - iterative build diary
  04-testing.md       - test plans and results
  05-evaluation.md    - requirements check, performance, reflection
  references.md       - papers, books, resources
  math-appendix.md    - full mathematical derivations

scenes/               - JSON scene configuration files
```

## documentation

full NEA-style documentation is in [`docs/`](docs/).

## author

nathan pang
