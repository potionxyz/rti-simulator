// ============================================================================
// voxel_grid.cpp - Implementation of the 3D Voxel Grid
// ============================================================================
// this file implements all the methods declared in voxel_grid.h.
// the voxel grid is essentially a 3D array stored as a flat 1D vector,
// which makes the maths cleaner when we need to treat it as a column
// vector for the reconstruction algorithm.
// ============================================================================

#include "voxel_grid.h"

// ============================================================================
// constructor
// ============================================================================
// takes the room dimensions and voxel size, calculates how many voxels
// fit along each axis, then allocates the flat array initialised to zero.
//
// std::ceil is used so we always round up - if the room is 2.15m and voxels
// are 0.1m, we get 22 voxels (not 21) so nothing gets cut off at the edges.
// ============================================================================
VoxelGrid::VoxelGrid(double room_x, double room_y, double room_z, double voxel_size)
    : room_x_(room_x)
    , room_y_(room_y)
    , room_z_(room_z)
    , voxel_size_(voxel_size)
{
    // calculate grid dimensions by dividing room size by voxel size
    // ceil ensures we cover the full room even if it doesn't divide evenly
    nx_ = static_cast<int>(std::ceil(room_x / voxel_size));
    ny_ = static_cast<int>(std::ceil(room_y / voxel_size));
    nz_ = static_cast<int>(std::ceil(room_z / voxel_size));

    // total number of voxels in the grid
    total_ = static_cast<size_t>(nx_) * ny_ * nz_;

    // allocate the flat array, all values start at 0.0 (empty room)
    voxels_.resize(total_, 0.0);
}

// ============================================================================
// to_flat_index - convert (i, j, k) to a single array index
// ============================================================================
// the mapping is: index = i + (j * nx) + (k * nx * ny)
//
// this is the standard row-major layout:
//   - i (x) changes fastest (stride 1)
//   - j (y) changes next (stride nx)
//   - k (z) changes slowest (stride nx * ny)
//
// bounds checking is included - if you pass invalid coordinates,
// it throws an exception rather than silently corrupting memory
// ============================================================================
size_t VoxelGrid::to_flat_index(int i, int j, int k) const {
    // check bounds before accessing
    if (i < 0 || i >= nx_ || j < 0 || j >= ny_ || k < 0 || k >= nz_) {
        throw std::out_of_range("voxel index out of bounds");
    }

    return static_cast<size_t>(i) + static_cast<size_t>(j) * nx_
           + static_cast<size_t>(k) * nx_ * ny_;
}

// ============================================================================
// to_3d_index - convert flat index back to (i, j, k)
// ============================================================================
// reverse of to_flat_index. uses integer division and modulo:
//   k = index / (nx * ny)        -> which "layer" (z slice)
//   j = (remainder) / nx         -> which "row" in that layer
//   i = remainder after that     -> which "column"
// ============================================================================
void VoxelGrid::to_3d_index(size_t flat, int& i, int& j, int& k) const {
    if (flat >= total_) {
        throw std::out_of_range("flat index out of bounds");
    }

    // integer division to peel off each dimension
    size_t layer_size = static_cast<size_t>(nx_) * ny_;  // voxels per z-layer
    k = static_cast<int>(flat / layer_size);             // z coordinate
    size_t remainder = flat % layer_size;                 // what's left
    j = static_cast<int>(remainder / nx_);               // y coordinate
    i = static_cast<int>(remainder % nx_);               // x coordinate
}

// ============================================================================
// get/set by 3D coordinates
// ============================================================================
double VoxelGrid::get(int i, int j, int k) const {
    return voxels_[to_flat_index(i, j, k)];
}

void VoxelGrid::set(int i, int j, int k, double value) {
    voxels_[to_flat_index(i, j, k)] = value;
}

// ============================================================================
// get/set by flat index (used by reconstruction algorithms)
// ============================================================================
// the reconstruction engine works with flat vectors (Eigen::VectorXd),
// so it needs direct flat index access without converting to 3D first
// ============================================================================
double VoxelGrid::get_flat(size_t index) const {
    if (index >= total_) {
        throw std::out_of_range("flat index out of bounds");
    }
    return voxels_[index];
}

void VoxelGrid::set_flat(size_t index, double value) {
    if (index >= total_) {
        throw std::out_of_range("flat index out of bounds");
    }
    voxels_[index] = value;
}

// ============================================================================
// reset - clear the grid back to empty room
// ============================================================================
// sets every voxel to 0.0 attenuation (air)
// used when switching between scenes or running a new reconstruction
// ============================================================================
void VoxelGrid::reset() {
    std::fill(voxels_.begin(), voxels_.end(), 0.0);
}

// ============================================================================
// get_voxel_centre - world-space position of a voxel's centre
// ============================================================================
// each voxel spans from (i * voxel_size) to ((i+1) * voxel_size) along x,
// so the centre is at (i + 0.5) * voxel_size
//
// this is needed for:
//   - computing where radio links pass through the grid
//   - placing the 3D visualisation cubes at the right positions
//   - calculating distance from a detected object to ground truth
// ============================================================================
void VoxelGrid::get_voxel_centre(int i, int j, int k,
                                  double& cx, double& cy, double& cz) const {
    cx = (i + 0.5) * voxel_size_;
    cy = (j + 0.5) * voxel_size_;
    cz = (k + 0.5) * voxel_size_;
}
