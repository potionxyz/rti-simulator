# RTI 3D Volumetric Simulator

a C++ simulator for 3D radio tomographic imaging (RTI) - reconstructing images of objects in a room by analysing how radio signals attenuate between pairs of wireless sensor nodes.

think of it like a CT scan, but with WiFi signals instead of X-rays.

## what it does

- simulates a network of wireless sensor nodes placed around a 3D space
- models how radio signals attenuate when passing through objects (people, walls)
- reconstructs a 3D voxel image of the environment from signal measurements
- visualises the reconstruction vs ground truth

## the maths

the core problem is an inverse problem: given signal measurements **y** and a weight matrix **W** that maps voxel attenuations to link measurements, recover the attenuation image **x**:

```
y = Wx + noise
```

where:
- **y** is the vector of signal attenuation measurements (one per link)
- **W** is the weight matrix (which voxels each link passes through)
- **x** is the voxel attenuation image we want to recover
- **noise** is measurement noise

## building

requires CMake 3.16+ and a C++17 compiler.

```bash
mkdir build && cd build
cmake ..
make
./rti-simulator
```

## dependencies

all header-only (included in `include/`):
- [Eigen](https://eigen.tuxfamily.org/) - linear algebra
- [nlohmann/json](https://github.com/nlohmann/json) - scene configuration
- stb_image_write - image export

## project structure

```
├── src/             # source code
├── include/         # third-party headers (Eigen, json, stb)
├── docs/            # project documentation (NEA-style)
├── tests/           # unit and integration tests
├── scenes/          # scene configuration files (JSON)
├── data/            # output data and results
└── assets/          # images, diagrams
```

## documentation

full project documentation is in [`docs/`](docs/), structured as:
1. [analysis](docs/01-analysis.md) - problem definition, research, requirements
2. [design](docs/02-design.md) - architecture, algorithms, class diagrams
3. [development log](docs/03-development-log.md) - iterative build diary
4. [testing](docs/04-testing.md) - test plans and results
5. [evaluation](docs/05-evaluation.md) - did it work, what i learned

## author

nathan pang
