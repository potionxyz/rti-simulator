// ============================================================================
// forward_model.cpp - Implementation of Signal Propagation Simulation
// ============================================================================
// this is the physics engine of the simulator. it models how radio signals
// travel between nodes and how much they get attenuated by obstacles.
//
// the key algorithm here is Siddon's ray-voxel intersection, which comes
// from medical CT imaging. the idea is the same: trace a "ray" (radio link)
// through a grid of cells (voxels) and accumulate the attenuation along
// the path.
// ============================================================================

#include "forward_model.h"
#include <cmath>       // for std::abs, std::floor, std::ceil, std::min, std::max
#include <algorithm>   // for std::sort
#include <numeric>     // for std::iota
#include <chrono>      // for seeding the random number generator

// ============================================================================
// constructor
// ============================================================================
ForwardModel::ForwardModel(const VoxelGrid& grid, const NodeNetwork& network)
    : grid_(grid)
    , network_(network)
    , num_links_(network.num_links())
    , num_voxels_(grid.get_total())
{
    // seed the random number generator with current time
    // this means each run produces slightly different noise
    auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
    rng_.seed(static_cast<unsigned int>(seed));

    // pre-compute the weight matrix
    compute_weight_matrix();
}

// ============================================================================
// compute_weight_matrix - build the W matrix using Siddon's algorithm
// ============================================================================
// for each link in the network, trace a ray from node A to node B
// through the voxel grid. record how much of the ray passes through
// each voxel.
//
// W is sparse because each link only passes through a small number of
// voxels relative to the total. for an 8-node cube with 8000 voxels,
// each link might intersect ~30-40 voxels, so W is roughly 99.5% zeros.
// storing it as a sparse matrix saves memory and speeds up multiplication.
// ============================================================================
void ForwardModel::compute_weight_matrix() {
    // typedef for building sparse matrices efficiently
    // triplets are (row, column, value) entries
    typedef Eigen::Triplet<double> Triplet;
    std::vector<Triplet> triplets;

    const auto& links = network_.get_links();

    for (size_t link_idx = 0; link_idx < links.size(); ++link_idx) {
        const auto& link = links[link_idx];

        // get the 3D positions of both nodes
        const auto& node_a = network_.get_node(link.node_a);
        const auto& node_b = network_.get_node(link.node_b);

        // trace the ray through the voxel grid
        auto intersections = siddon_trace(
            node_a.x, node_a.y, node_a.z,
            node_b.x, node_b.y, node_b.z
        );

        // add each intersection to the triplet list
        for (const auto& [voxel_idx, length] : intersections) {
            if (length > 1e-10) {  // ignore negligible intersections
                triplets.emplace_back(
                    static_cast<int>(link_idx),
                    static_cast<int>(voxel_idx),
                    length
                );
            }
        }
    }

    // build the sparse matrix from triplets
    // this is the most efficient way to construct an Eigen sparse matrix
    W_.resize(static_cast<int>(num_links_), static_cast<int>(num_voxels_));
    W_.setFromTriplets(triplets.begin(), triplets.end());
    W_.makeCompressed();  // optimise storage
}

// ============================================================================
// simulate - generate synthetic measurements from a scene
// ============================================================================
// the measurement for each link is the integral of attenuation along
// the link path:
//   y[link] = sum over voxels: W[link][voxel] * x[voxel]
//
// in matrix form: y = W * x
//
// then we add Gaussian noise to simulate real sensor imperfections.
// ============================================================================
Eigen::VectorXd ForwardModel::simulate(const VoxelGrid& scene, double noise_stddev) {
    // convert the voxel grid data to an Eigen vector
    // Eigen::Map creates a "view" of the data without copying it
    Eigen::VectorXd x = Eigen::Map<const Eigen::VectorXd>(
        scene.data().data(),
        static_cast<int>(scene.get_total())
    );

    // compute measurements: y = W * x
    // this is a sparse matrix-vector multiply, very efficient
    Eigen::VectorXd y = W_ * x;

    // add Gaussian noise if requested
    if (noise_stddev > 0.0) {
        std::normal_distribution<double> noise_dist(0.0, noise_stddev);
        for (int i = 0; i < y.size(); ++i) {
            y(i) += noise_dist(rng_);
        }

        // clamp to non-negative (attenuation can't be negative in reality,
        // though noise might push it below zero)
        for (int i = 0; i < y.size(); ++i) {
            if (y(i) < 0.0) y(i) = 0.0;
        }
    }

    return y;
}

// ============================================================================
// generate_baseline - empty room measurements
// ============================================================================
// with no obstacles, all attenuation values are zero.
// baseline is simply a zero vector.
// ============================================================================
Eigen::VectorXd ForwardModel::generate_baseline() {
    return Eigen::VectorXd::Zero(static_cast<int>(num_links_));
}

// ============================================================================
// get_weight_matrix_dense - convert sparse W to dense format
// ============================================================================
// some reconstruction algorithms (like direct matrix inversion) need
// the dense form. this is memory-expensive for large grids but fine
// for our typical sizes (28 links x 8000 voxels = ~224KB).
// ============================================================================
Eigen::MatrixXd ForwardModel::get_weight_matrix_dense() const {
    return Eigen::MatrixXd(W_);
}

// ============================================================================
// siddon_trace - Siddon's ray-voxel intersection algorithm
// ============================================================================
// this algorithm finds every voxel that a 3D line segment passes through,
// and computes the length of the intersection within each voxel.
//
// the core idea:
//   1. compute where the ray enters and exits the grid (parametric t values)
//   2. for each axis (x, y, z), compute the t values where the ray
//      crosses each grid plane
//   3. merge and sort all these t values
//   4. step through consecutive t values - each interval corresponds
//      to the ray being inside one specific voxel
//
// the parameter t goes from 0.0 (start point) to 1.0 (end point).
// for any t, the point on the ray is:
//   point = start + t * (end - start)
//
// reference: Siddon, R.L. (1985), Medical Physics 12(2):252-255
// ============================================================================
std::vector<std::pair<size_t, double>> ForwardModel::siddon_trace(
    double x1, double y1, double z1,
    double x2, double y2, double z2) const
{
    std::vector<std::pair<size_t, double>> result;

    // grid parameters
    int nx = grid_.get_nx();
    int ny = grid_.get_ny();
    int nz = grid_.get_nz();
    double vs = grid_.get_voxel_size();

    // direction vector of the ray
    double dx = x2 - x1;
    double dy = y2 - y1;
    double dz = z2 - z1;

    // total length of the ray (needed to convert t-intervals to metres)
    double ray_length = std::sqrt(dx * dx + dy * dy + dz * dz);

    // avoid division by zero for zero-length rays
    if (ray_length < 1e-10) return result;

    // small epsilon to handle floating point edge cases
    const double eps = 1e-10;

    // ---- step 1: compute parametric range where ray is inside the grid ----
    // the grid spans from (0,0,0) to (nx*vs, ny*vs, nz*vs)
    // we need to find t_min and t_max such that the ray segment
    // [t_min, t_max] is inside the grid

    double t_min = 0.0;
    double t_max = 1.0;

    // helper lambda: clip the parametric range to a slab [lo, hi] along one axis
    // if the ray is parallel to the slab (d == 0), check if it's inside
    auto clip_axis = [&](double d, double p1, double lo, double hi) -> bool {
        if (std::abs(d) < eps) {
            // ray is parallel to this axis
            // if the start point is outside the slab, no intersection
            return (p1 >= lo && p1 <= hi);
        }
        double t_lo = (lo - p1) / d;
        double t_hi = (hi - p1) / d;
        if (t_lo > t_hi) std::swap(t_lo, t_hi);
        t_min = std::max(t_min, t_lo);
        t_max = std::min(t_max, t_hi);
        return (t_min <= t_max + eps);
    };

    // clip against all three axes
    if (!clip_axis(dx, x1, 0.0, nx * vs)) return result;
    if (!clip_axis(dy, y1, 0.0, ny * vs)) return result;
    if (!clip_axis(dz, z1, 0.0, nz * vs)) return result;

    // ensure we're within [0, 1]
    t_min = std::max(t_min, 0.0);
    t_max = std::min(t_max, 1.0);
    if (t_min >= t_max - eps) return result;

    // ---- step 2: collect all t values where the ray crosses grid planes ----
    std::vector<double> t_values;
    t_values.push_back(t_min);
    t_values.push_back(t_max);

    // x-plane crossings: planes at x = i * vs for i = 0, 1, ..., nx
    if (std::abs(dx) > eps) {
        for (int i = 0; i <= nx; ++i) {
            double t = (i * vs - x1) / dx;
            if (t > t_min + eps && t < t_max - eps) {
                t_values.push_back(t);
            }
        }
    }

    // y-plane crossings
    if (std::abs(dy) > eps) {
        for (int j = 0; j <= ny; ++j) {
            double t = (j * vs - y1) / dy;
            if (t > t_min + eps && t < t_max - eps) {
                t_values.push_back(t);
            }
        }
    }

    // z-plane crossings
    if (std::abs(dz) > eps) {
        for (int k = 0; k <= nz; ++k) {
            double t = (k * vs - z1) / dz;
            if (t > t_min + eps && t < t_max - eps) {
                t_values.push_back(t);
            }
        }
    }

    // ---- step 3: sort the t values ----
    std::sort(t_values.begin(), t_values.end());

    // remove duplicates (planes can coincide at corners)
    auto last = std::unique(t_values.begin(), t_values.end(),
        [eps](double a, double b) { return std::abs(a - b) < eps; });
    t_values.erase(last, t_values.end());

    // ---- step 4: step through consecutive t values ----
    // each interval [t_values[i], t_values[i+1]] is inside one voxel
    for (size_t idx = 0; idx + 1 < t_values.size(); ++idx) {
        double t_mid = (t_values[idx] + t_values[idx + 1]) / 2.0;

        // compute the midpoint of this interval (this point is inside the voxel)
        double px = x1 + t_mid * dx;
        double py = y1 + t_mid * dy;
        double pz = z1 + t_mid * dz;

        // convert to voxel indices
        int vi = static_cast<int>(std::floor(px / vs));
        int vj = static_cast<int>(std::floor(py / vs));
        int vk = static_cast<int>(std::floor(pz / vs));

        // clamp to grid bounds (handles floating point edge cases)
        vi = std::max(0, std::min(vi, nx - 1));
        vj = std::max(0, std::min(vj, ny - 1));
        vk = std::max(0, std::min(vk, nz - 1));

        // compute the length of this interval in metres
        double segment_length = (t_values[idx + 1] - t_values[idx]) * ray_length;

        // convert to flat index and store
        size_t flat_idx = static_cast<size_t>(vi)
                        + static_cast<size_t>(vj) * nx
                        + static_cast<size_t>(vk) * nx * ny;

        result.emplace_back(flat_idx, segment_length);
    }

    return result;
}
