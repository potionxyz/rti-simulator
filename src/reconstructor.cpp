// ============================================================================
// reconstructor.cpp - Implementation of the Reconstruction Engine
// ============================================================================
// this solves the inverse problem: recovering a 3D attenuation image
// from a set of link measurements.
//
// the challenge is that we have far fewer measurements (28 links) than
// unknowns (8000 voxels), making the system heavily underdetermined.
// regularisation is essential to get a useful result.
// ============================================================================

#include "reconstructor.h"
#include <cmath>
#include <iostream>
#include <algorithm>

// ============================================================================
// constructor
// ============================================================================
// pre-computes W^T, W^T*W, and the dense weight matrix since these
// are reused across multiple reconstruction calls. computing them once
// saves significant time when testing different parameters.
// ============================================================================
Reconstructor::Reconstructor(const ForwardModel& forward, const VoxelGrid& grid)
    : forward_(forward)
    , grid_(grid)
{
    // convert sparse weight matrix to dense for the reconstruction maths
    // this is fine for our problem size (28 x 8000 = 224KB)
    W_dense_ = forward.get_weight_matrix_dense();

    // pre-compute the transpose (used by all methods)
    // W^T is 8000 x 28
    Wt_ = W_dense_.transpose();

    // pre-compute W^T * W (used by least-squares and tikhonov)
    // this is 8000 x 8000 which is ~512MB in double precision
    // for larger grids we'd need iterative methods, but for 20x20x20
    // this is manageable
    WtW_ = Wt_ * W_dense_;
}

// ============================================================================
// back_projection - simplest reconstruction method
// ============================================================================
// for each link that shows attenuation, we "spray" that attenuation
// value back along the link's path through the voxel grid.
//
// mathematically: x_bp = W^T * y
//
// this is equivalent to saying: "every voxel on a blocked link gets
// credit proportional to how much of the link passes through it."
//
// the result is blurry because a single link affects many voxels,
// but voxels where multiple blocked links intersect will have higher
// values - that's where the object most likely is.
//
// this is the same principle as "filtered back-projection" in CT scans,
// but without the filtering step (which needs more measurements than
// we have).
// ============================================================================
Eigen::VectorXd Reconstructor::back_projection(const Eigen::VectorXd& measurements) {
    // x = W^T * y
    // W^T is (num_voxels x num_links), y is (num_links x 1)
    // result is (num_voxels x 1)
    Eigen::VectorXd x = Wt_ * measurements;

    // clamp negatives - attenuation can't be negative
    return clamp_non_negative(x);
}

// ============================================================================
// least_squares - pseudoinverse reconstruction
// ============================================================================
// finds the minimum-norm solution that best fits the measurements.
//
// mathematically: x = (W^T W)^{-1} W^T y
//
// the problem: W^T W is 8000x8000 but has rank at most 28 (the number
// of links). that means it's singular - you can't invert it directly.
//
// instead, we use Eigen's LDLT decomposition which handles the
// near-singular case by computing a pseudoinverse. this gives a
// least-squares solution, but it will be noisy and unreliable.
//
// this method exists mainly for comparison with tikhonov to show
// why regularisation is needed.
// ============================================================================
Eigen::VectorXd Reconstructor::least_squares(const Eigen::VectorXd& measurements) {
    // W^T * y
    Eigen::VectorXd Wt_y = Wt_ * measurements;

    // solve (W^T W) x = W^T y using LDLT decomposition
    // LDLT is a robust Cholesky-like decomposition that handles
    // positive semi-definite matrices (which W^T W is)
    Eigen::LDLT<Eigen::MatrixXd> solver(WtW_);

    // check if the decomposition succeeded
    if (solver.info() != Eigen::Success) {
        std::cerr << "warning: LDLT decomposition failed, "
                  << "falling back to back-projection" << std::endl;
        return back_projection(measurements);
    }

    Eigen::VectorXd x = solver.solve(Wt_y);

    // check if the solve was numerically stable
    if (solver.info() != Eigen::Success) {
        std::cerr << "warning: solve failed, falling back to back-projection"
                  << std::endl;
        return back_projection(measurements);
    }

    return clamp_non_negative(x);
}

// ============================================================================
// tikhonov - regularised reconstruction (the main method)
// ============================================================================
// this is the method i'd actually use in practice. it adds a penalty
// term that keeps the solution stable even when the system is
// underdetermined.
//
// instead of minimising just the data fit:
//   min ||Wx - y||^2
//
// we minimise:
//   min ||Wx - y||^2 + lambda * ||x||^2
//
// the second term (lambda * ||x||^2) penalises large values in x,
// which prevents the solution from blowing up due to noise.
//
// the closed-form solution is:
//   x = (W^T W + lambda * I)^{-1} W^T y
//
// adding lambda * I to W^T W makes the matrix positive definite
// (guaranteed invertible), which is the key insight.
//
// lambda controls the trade-off:
//   small lambda (0.01): trusts the data more, sharper but noisier
//   large lambda (10.0): trusts the prior more, smoother but blurrier
//
// in practice, you tune lambda by trying different values and picking
// the one that gives the best localisation accuracy.
// ============================================================================
Eigen::VectorXd Reconstructor::tikhonov(const Eigen::VectorXd& measurements,
                                         double lambda) {
    // W^T * y
    Eigen::VectorXd Wt_y = Wt_ * measurements;

    // build the regularised matrix: W^T W + lambda * I
    // I is the identity matrix (diagonal of ones)
    int n = static_cast<int>(forward_.num_voxels());
    Eigen::MatrixXd regularised = WtW_ + lambda * Eigen::MatrixXd::Identity(n, n);

    // solve using LDLT decomposition
    // the regularisation guarantees this matrix is positive definite,
    // so the decomposition will always succeed
    Eigen::LDLT<Eigen::MatrixXd> solver(regularised);
    Eigen::VectorXd x = solver.solve(Wt_y);

    return clamp_non_negative(x);
}

// ============================================================================
// evaluate - compare reconstruction against ground truth
// ============================================================================
// computes several accuracy metrics:
//
// MSE (mean squared error): average of (reconstructed - true)^2
//   lower is better, 0.0 is perfect
//
// localisation error: euclidean distance between the peak of the
//   reconstruction and the centroid of the ground truth
//   measures "how far off is our best guess?"
//
// true/false positives/negatives: binary classification metrics
//   did we correctly identify which voxels are occupied?
// ============================================================================
ReconstructionResult Reconstructor::evaluate(
    const Eigen::VectorXd& reconstruction,
    const VoxelGrid& ground_truth,
    const std::string& method_name,
    double threshold) const
{
    ReconstructionResult result;
    result.method = method_name;
    result.image = reconstruction;

    int n = static_cast<int>(ground_truth.get_total());

    // ---- MSE ----
    // normalise the reconstruction to the same scale as ground truth
    // for fair comparison (back-projection values are in different units)
    double recon_max = reconstruction.maxCoeff();
    double truth_max = 0.0;
    for (size_t i = 0; i < ground_truth.get_total(); ++i) {
        truth_max = std::max(truth_max, ground_truth.get_flat(i));
    }

    Eigen::VectorXd normalised = reconstruction;
    if (recon_max > 1e-10) {
        normalised = reconstruction * (truth_max / recon_max);
    }

    double sum_sq_err = 0.0;
    for (int i = 0; i < n; ++i) {
        double diff = normalised(i) - ground_truth.get_flat(static_cast<size_t>(i));
        sum_sq_err += diff * diff;
    }
    result.mse = sum_sq_err / n;

    // ---- peak value ----
    result.peak_value = recon_max;

    // ---- localisation error ----
    // find the centroid of the ground truth (weighted by attenuation)
    double gt_cx = 0, gt_cy = 0, gt_cz = 0, gt_total = 0;
    for (size_t idx = 0; idx < ground_truth.get_total(); ++idx) {
        double val = ground_truth.get_flat(idx);
        if (val > 0.0) {
            int i, j, k;
            ground_truth.to_3d_index(idx, i, j, k);
            double cx, cy, cz;
            ground_truth.get_voxel_centre(i, j, k, cx, cy, cz);
            gt_cx += cx * val;
            gt_cy += cy * val;
            gt_cz += cz * val;
            gt_total += val;
        }
    }
    if (gt_total > 0) {
        gt_cx /= gt_total;
        gt_cy /= gt_total;
        gt_cz /= gt_total;
    }

    // find the centroid of the reconstruction (weighted by value)
    double rc_cx = 0, rc_cy = 0, rc_cz = 0, rc_total = 0;
    for (int idx = 0; idx < n; ++idx) {
        double val = reconstruction(idx);
        if (val > threshold) {
            int i, j, k;
            grid_.to_3d_index(static_cast<size_t>(idx), i, j, k);
            double cx, cy, cz;
            grid_.get_voxel_centre(i, j, k, cx, cy, cz);
            rc_cx += cx * val;
            rc_cy += cy * val;
            rc_cz += cz * val;
            rc_total += val;
        }
    }
    if (rc_total > 0) {
        rc_cx /= rc_total;
        rc_cy /= rc_total;
        rc_cz /= rc_total;
    }

    // euclidean distance between centroids
    double dx = gt_cx - rc_cx;
    double dy = gt_cy - rc_cy;
    double dz = gt_cz - rc_cz;
    result.localisation_error = std::sqrt(dx * dx + dy * dy + dz * dz);

    // ---- true/false positives/negatives ----
    // use auto-threshold if threshold is 0
    double actual_threshold = threshold;
    if (actual_threshold <= 0.0 && recon_max > 0.0) {
        actual_threshold = recon_max * 0.3;  // 30% of peak
    }

    result.true_positives = 0;
    result.false_positives = 0;
    result.false_negatives = 0;

    for (int idx = 0; idx < n; ++idx) {
        bool is_occupied = ground_truth.get_flat(static_cast<size_t>(idx)) > 0.0;
        bool detected = reconstruction(idx) > actual_threshold;

        if (is_occupied && detected) result.true_positives++;
        else if (!is_occupied && detected) result.false_positives++;
        else if (is_occupied && !detected) result.false_negatives++;
    }

    return result;
}

// ============================================================================
// evaluate_auto_threshold - use percentage of peak as threshold
// ============================================================================
ReconstructionResult Reconstructor::evaluate_auto_threshold(
    const Eigen::VectorXd& reconstruction,
    const VoxelGrid& ground_truth,
    const std::string& method_name,
    double threshold_pct) const
{
    double peak = reconstruction.maxCoeff();
    double threshold = peak * threshold_pct;
    return evaluate(reconstruction, ground_truth, method_name, threshold);
}

// ============================================================================
// apply_to_grid - write reconstruction result into a voxel grid
// ============================================================================
// this is for visualisation - takes the flat reconstruction vector and
// writes it back into a VoxelGrid object so it can be rendered.
// ============================================================================
void Reconstructor::apply_to_grid(const Eigen::VectorXd& reconstruction,
                                   VoxelGrid& output_grid) const {
    output_grid.reset();
    for (int i = 0; i < reconstruction.size(); ++i) {
        if (reconstruction(i) > 0.0) {
            output_grid.set_flat(static_cast<size_t>(i), reconstruction(i));
        }
    }
}

// ============================================================================
// clamp_non_negative - zero out negative values
// ============================================================================
// attenuation physically cannot be negative (you can't "add" signal
// by having something in the way). noise and numerical errors can
// produce negative values, so we clamp them to zero.
// ============================================================================
Eigen::VectorXd Reconstructor::clamp_non_negative(const Eigen::VectorXd& x) {
    Eigen::VectorXd result = x;
    for (int i = 0; i < result.size(); ++i) {
        if (result(i) < 0.0) result(i) = 0.0;
    }
    return result;
}
