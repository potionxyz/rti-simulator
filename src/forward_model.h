// ============================================================================
// forward_model.h - Signal Propagation Simulation
// ============================================================================
// the forward model simulates what would happen in real life: radio signals
// travel between sensor nodes and get attenuated (weakened) by anything
// in their path.
//
// mathematically, this is the "forward problem":
//   given the true attenuation image x, calculate measurements y
//   y = Wx + noise
//
// where:
//   y = measurement vector (signal drop for each link, in dB)
//   W = weight matrix (which voxels each link passes through)
//   x = attenuation image (dB/m value for each voxel)
//   noise = Gaussian measurement noise
//
// the weight matrix W is the key data structure. each row corresponds
// to one radio link, and each column corresponds to one voxel. the
// value W[link][voxel] is the length of the link segment that passes
// through that voxel (in metres).
//
// to calculate W, i use Siddon's algorithm - a method from medical CT
// imaging that efficiently finds the intersection of a ray with a 3D
// voxel grid. it's O(nx + ny + nz) per ray, which is much faster than
// checking every voxel.
// ============================================================================

#ifndef FORWARD_MODEL_H
#define FORWARD_MODEL_H

#include "voxel_grid.h"
#include "node.h"
#include <Eigen/Dense>    // for matrix operations
#include <Eigen/Sparse>   // for sparse weight matrix
#include <vector>
#include <random>         // for noise generation

class ForwardModel {
public:
    // -----------------------------------------------------------------------
    // constructor
    // -----------------------------------------------------------------------
    // takes the voxel grid (defines the space) and node network (defines
    // where sensors are). computes the weight matrix on construction.
    // -----------------------------------------------------------------------
    ForwardModel(const VoxelGrid& grid, const NodeNetwork& network);

    // -----------------------------------------------------------------------
    // compute_weight_matrix - build the W matrix
    // -----------------------------------------------------------------------
    // uses Siddon's algorithm to trace each link through the voxel grid
    // and record how much of the link passes through each voxel.
    //
    // the result is a sparse matrix because most links only pass through
    // a small fraction of the total voxels (a line through a 20x20x20 grid
    // might only hit ~30-40 voxels out of 8000).
    // -----------------------------------------------------------------------
    void compute_weight_matrix();

    // -----------------------------------------------------------------------
    // simulate - generate synthetic RSS measurements
    // -----------------------------------------------------------------------
    // takes a voxel grid with obstacles and produces what the sensors
    // would measure. optionally adds Gaussian noise.
    //
    // noise_stddev: standard deviation of the noise in dB
    //   0.0 = perfect measurements (unrealistic)
    //   0.5 = low noise (good conditions)
    //   1.0 = moderate noise (typical indoor)
    //   2.0 = high noise (challenging environment)
    // -----------------------------------------------------------------------
    Eigen::VectorXd simulate(const VoxelGrid& scene, double noise_stddev = 0.0);

    // -----------------------------------------------------------------------
    // generate_baseline - simulate empty room measurements
    // -----------------------------------------------------------------------
    // records what the RSS would be with no obstacles.
    // in the simplified model, baseline is all zeros (no attenuation).
    // -----------------------------------------------------------------------
    Eigen::VectorXd generate_baseline();

    // -----------------------------------------------------------------------
    // accessors
    // -----------------------------------------------------------------------
    const Eigen::SparseMatrix<double>& get_weight_matrix() const { return W_; }

    // dense version of the weight matrix (needed for some reconstruction methods)
    Eigen::MatrixXd get_weight_matrix_dense() const;

    size_t num_links() const { return num_links_; }
    size_t num_voxels() const { return num_voxels_; }

private:
    const VoxelGrid& grid_;
    const NodeNetwork& network_;

    Eigen::SparseMatrix<double> W_;  // the weight matrix (links x voxels)
    size_t num_links_;
    size_t num_voxels_;

    std::mt19937 rng_;  // random number generator for noise

    // -----------------------------------------------------------------------
    // siddon_trace - trace a single ray through the voxel grid
    // -----------------------------------------------------------------------
    // implements Siddon's algorithm to find which voxels a line from
    // point (x1,y1,z1) to (x2,y2,z2) passes through, and for what
    // length.
    //
    // returns a list of (voxel_index, intersection_length) pairs.
    //
    // Siddon's algorithm works by finding where the ray crosses each
    // grid plane (x-planes, y-planes, z-planes), sorting these
    // crossings, and stepping through them to determine voxel-by-voxel
    // intersection lengths.
    //
    // reference: Siddon, R.L. (1985) "Fast calculation of the exact
    // radiological path for a three-dimensional CT array"
    // -----------------------------------------------------------------------
    std::vector<std::pair<size_t, double>> siddon_trace(
        double x1, double y1, double z1,
        double x2, double y2, double z2) const;
};

#endif // FORWARD_MODEL_H
