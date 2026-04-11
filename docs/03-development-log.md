# Development Log

## 11 april 2026

### project setup and core data structures

so first thing i needed to figure out was representing 3d space in code. i went with a voxel grid - basically dividing the room into small cubes and storing an attenuation value for each one.

the key decision was using a flat `std::vector<double>` instead of a 3D array. this might seem counterintuitive since we're modelling a 3D space, but it actually makes everything cleaner:
- it's cache-friendly because all the data sits contiguously in memory
- it maps directly to the column vector **x** in the reconstruction equation **y = Wx**
- Eigen (the linear algebra library) works with flat vectors, so no conversion needed

for the grid dimensions, i'm using `std::ceil` when dividing room size by voxel size. this means if the room doesn't divide evenly (say 2.15m with 0.1m voxels), we get 22 voxels instead of 21 - nothing gets cut off at the edges.

then i set up the node network. each node is an ESP32 sensor at a known position. the important bit is the link generation - with N nodes you get N*(N-1)/2 unique links. so 8 nodes gives 28 links, which is a lot of data to work with for reconstruction.

i built three preset configurations:
- 4-node 2D square (for basic testing)
- 8-node 3D cube (the main one, matches the physical hardware plan)
- custom perimeter layout (for testing how node count affects accuracy)

tested it by placing a simulated person in the centre of a 2m cube. 204 voxels occupied out of 8000 total, which looks about right for a human-shaped region (0.4m x 0.3m x 1.7m).

**next:** forward model - simulating how signals actually travel between nodes and get attenuated by objects.
