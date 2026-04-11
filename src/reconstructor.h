// ============================================================================
// reconstructor.h - Reconstruction Engine (Inverse Problem Solver)
// ============================================================================
// this is the most important part of the whole project. the forward model
// tells us "given obstacles, what would the sensors measure?" but the
// reconstruction does the reverse: "given sensor measurements, where are
// the obstacles?"
//
// mathematically, we have:
//   y = Wx + noise
//
// and we want to find x (the attenuation image) given y (measurements)
// and W (weight matrix).
//
// this is an "inverse problem" and it's ill-posed, meaning:
//   - there are far more unknowns (8000 voxels) than equations (28 links)
//   - the system is heavily underdetermined
//   - small noise in y can cause huge errors in x
//
// to handle this, we use regularisation - adding extra constraints that
// push the solution towards something physically reasonable.
//
// i've implemented three methods, from simplest to most sophisticated:
//
// 1. back-projection: fast and simple, but blurry
//    just "spray" each measurement back along its link path
//
// 2. least-squares (pseudoinverse): mathematically optimal for no noise
//    x = (W^T W)^{-1} W^T y
//    but unstable when W^T W is nearly singular
//
// 3. tikhonov regularisation: the best method for this problem
//    x = (W^T W + lambda * I)^{-1} W^T y
//    the lambda parameter controls the trade-off between fitting the
//    data and keeping the solution smooth
// ============================================================================

#ifndef RECONSTRUCTOR_H
#define RECONSTRUCTOR_H

#include "forward_model.h"
#include "voxel_grid.h"
#include <Eigen/Dense>
#include <string>

// ============================================================================
// ReconstructionResult - holds the output of a reconstruction
// ============================================================================
// bundles the reconstructed image together with accuracy metrics
// so we can easily compare different methods and parameters
// ============================================================================
struct ReconstructionResult {
    Eigen::VectorXd image;         // the reconstructed attenuation vector
    double mse;                    // mean squared error vs ground truth
    double localisation_error;     // distance from true to detected peak (metres)
    double peak_value;             // maximum reconstructed attenuation
    int true_positives;            // voxels correctly identified as occupied
    int false_positives;           // empty voxels incorrectly marked as occupied
    int false_negatives;           // occupied voxels that were missed
    std::string method;            // which algorithm was used
};

class Reconstructor {
public:
    // -----------------------------------------------------------------------
    // constructor
    // -----------------------------------------------------------------------
    // takes the forward model (which contains the weight matrix W)
    // and the voxel grid dimensions (needed to convert results back to 3D)
    // -----------------------------------------------------------------------
    Reconstructor(const ForwardModel& forward, const VoxelGrid& grid);

    // -----------------------------------------------------------------------
    // reconstruction methods
    // -----------------------------------------------------------------------

    // method 1: back-projection
    // the simplest approach. for each link with attenuation y[i], add
    // y[i] to every voxel that link passes through, weighted by the
    // intersection length.
    //
    // mathematically: x = W^T * y
    //
    // pros: fast, always works, no matrix inversion
    // cons: blurry, doesn't give true attenuation values
    Eigen::VectorXd back_projection(const Eigen::VectorXd& measurements);

    // method 2: least-squares (Moore-Penrose pseudoinverse)
    // finds the x that minimises ||Wx - y||^2
    //
    // mathematically: x = (W^T W)^{-1} W^T y
    //
    // pros: mathematically optimal for noiseless data
    // cons: W^T W is often singular or near-singular (can't invert it)
    //       very sensitive to noise
    Eigen::VectorXd least_squares(const Eigen::VectorXd& measurements);

    // method 3: tikhonov regularisation
    // adds a penalty term to keep the solution stable:
    //   minimise ||Wx - y||^2 + lambda * ||x||^2
    //
    // mathematically: x = (W^T W + lambda * I)^{-1} W^T y
    //
    // lambda controls the regularisation strength:
    //   lambda too small: noisy, spiky result (overfitting)
    //   lambda too large: over-smoothed, washes out the signal
    //   lambda just right: clean reconstruction with good localisation
    //
    // typical values: 0.01 to 10.0 depending on noise level
    Eigen::VectorXd tikhonov(const Eigen::VectorXd& measurements, double lambda);

    // -----------------------------------------------------------------------
    // evaluation
    // -----------------------------------------------------------------------

    // compare a reconstruction against the ground truth scene
    // computes MSE, localisation error, and detection stats
    //
    // threshold: attenuation value above which a voxel is considered
    // "occupied" (for true/false positive calculations)
    ReconstructionResult evaluate(const Eigen::VectorXd& reconstruction,
                                  const VoxelGrid& ground_truth,
                                  const std::string& method_name,
                                  double threshold = 0.0) const;

    // auto-threshold: use a percentage of the peak value
    // e.g. threshold_pct = 0.3 means voxels above 30% of the max
    // are considered occupied
    ReconstructionResult evaluate_auto_threshold(
        const Eigen::VectorXd& reconstruction,
        const VoxelGrid& ground_truth,
        const std::string& method_name,
        double threshold_pct = 0.3) const;

    // -----------------------------------------------------------------------
    // utility
    // -----------------------------------------------------------------------

    // apply the reconstruction result back to a voxel grid for visualisation
    void apply_to_grid(const Eigen::VectorXd& reconstruction,
                       VoxelGrid& output_grid) const;

    // clamp negative values to zero (attenuation can't be negative)
    static Eigen::VectorXd clamp_non_negative(const Eigen::VectorXd& x);

private:
    const ForwardModel& forward_;
    const VoxelGrid& grid_;
    Eigen::MatrixXd W_dense_;    // dense copy of weight matrix
    Eigen::MatrixXd WtW_;        // pre-computed W^T * W
    Eigen::MatrixXd Wt_;         // pre-computed W^T
};

#endif // RECONSTRUCTOR_H
