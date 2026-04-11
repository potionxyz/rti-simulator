// ============================================================================
// voxel_grid.h - 3D Voxel Grid Representation
// ============================================================================
// this is the foundation of the whole simulator. a voxel grid divides a 3D
// room into small cubes (voxels), each storing an attenuation value that
// represents how much radio signal gets absorbed at that point.
//
// think of it like 3D pixels - if a person is standing somewhere, the voxels
// where their body is will have high attenuation values, and empty space
// will have zero.
//
// i'm using a flat std::vector instead of a 3D array because:
//   1. it's cache-friendly (all data is contiguous in memory)
//   2. it maps directly to the column vector x in the equation y = Wx
//   3. easier to pass to Eigen for matrix operations
// ============================================================================

#ifndef VOXEL_GRID_H
#define VOXEL_GRID_H

#include <vector>    // for std::vector - dynamic array container
#include <cstddef>   // for size_t - unsigned integer type for sizes
#include <stdexcept> // for std::out_of_range - exception handling
#include <cmath>     // for std::ceil - rounding up division results

class VoxelGrid {
public:
    // -----------------------------------------------------------------------
    // constructor
    // -----------------------------------------------------------------------
    // room_x/y/z: physical dimensions of the room in metres
    // voxel_size: side length of each voxel cube in metres
    //
    // example: a 2m x 2m x 2m room with 0.1m voxels = 20x20x20 = 8000 voxels
    // -----------------------------------------------------------------------
    VoxelGrid(double room_x, double room_y, double room_z, double voxel_size);

    // -----------------------------------------------------------------------
    // index conversion
    // -----------------------------------------------------------------------
    // convert between 3D coordinates (i, j, k) and the flat array index
    // the formula is: index = i + (j * nx) + (k * nx * ny)
    // this is row-major ordering - x changes fastest
    // -----------------------------------------------------------------------
    size_t to_flat_index(int i, int j, int k) const;

    // reverse: flat index back to 3D coordinates
    void to_3d_index(size_t flat, int& i, int& j, int& k) const;

    // -----------------------------------------------------------------------
    // get/set attenuation values
    // -----------------------------------------------------------------------
    // attenuation is measured in dB/m - how much signal a voxel absorbs
    // air ~= 0.0, human body ~= 3-5 dB/m, concrete wall ~= 10-15 dB/m
    // -----------------------------------------------------------------------
    double get(int i, int j, int k) const;
    void set(int i, int j, int k, double value);

    // access by flat index (used by reconstruction algorithms)
    double get_flat(size_t index) const;
    void set_flat(size_t index, double value);

    // -----------------------------------------------------------------------
    // utility
    // -----------------------------------------------------------------------
    void reset();  // set all voxels back to zero (empty room)

    // get the world-space centre position of a voxel in metres
    // useful for visualisation and for computing link-voxel intersections
    void get_voxel_centre(int i, int j, int k,
                          double& cx, double& cy, double& cz) const;

    // -----------------------------------------------------------------------
    // accessors
    // -----------------------------------------------------------------------
    int get_nx() const { return nx_; }           // voxels along x axis
    int get_ny() const { return ny_; }           // voxels along y axis
    int get_nz() const { return nz_; }           // voxels along z axis
    size_t get_total() const { return total_; }  // total number of voxels
    double get_voxel_size() const { return voxel_size_; }
    double get_room_x() const { return room_x_; }
    double get_room_y() const { return room_y_; }
    double get_room_z() const { return room_z_; }

    // direct access to the underlying data (for Eigen interop)
    const std::vector<double>& data() const { return voxels_; }
    std::vector<double>& data() { return voxels_; }

private:
    double room_x_, room_y_, room_z_;  // room dimensions in metres
    double voxel_size_;                 // voxel side length in metres
    int nx_, ny_, nz_;                  // number of voxels along each axis
    size_t total_;                      // total voxel count (nx * ny * nz)
    std::vector<double> voxels_;        // the actual data - flat array
};

#endif // VOXEL_GRID_H
