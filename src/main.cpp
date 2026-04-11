// ============================================================================
// main.cpp - RTI 3D Volumetric Simulator
// ============================================================================
// full simulation pipeline:
//   1. create voxel grid and node network
//   2. define test scenes
//   3. run forward model (simulate measurements)
//   4. run reconstruction (recover the scene from measurements)
//   5. evaluate accuracy
// ============================================================================

#include <iostream>
#include <iomanip>
#include "voxel_grid.h"
#include "node.h"
#include "scene.h"
#include "forward_model.h"
#include "reconstructor.h"
#include "visualiser.h"

// helper function to print reconstruction results cleanly
void print_result(const ReconstructionResult& r) {
    std::cout << "    method: " << r.method << std::endl;
    std::cout << "    peak value: " << std::fixed << std::setprecision(4)
              << r.peak_value << std::endl;
    std::cout << "    localisation error: " << std::setprecision(3)
              << r.localisation_error << "m" << std::endl;
    std::cout << "    MSE: " << std::scientific << std::setprecision(3)
              << r.mse << std::endl;
    std::cout << "    true positives: " << r.true_positives << std::endl;
    std::cout << "    false positives: " << r.false_positives << std::endl;
    std::cout << "    false negatives: " << r.false_negatives << std::endl;
}

int main() {
    std::cout << "=== RTI 3D Volumetric Simulator ===" << std::endl;
    std::cout << std::endl;

    // ---- setup ----
    double room_x = 2.0, room_y = 2.0, room_z = 2.0;
    // 0.2m voxels = 10x10x10 = 1000 voxels (manageable for matrix solve)
    // can increase resolution later with iterative solvers
    double voxel_size = 0.2;

    VoxelGrid grid(room_x, room_y, room_z, voxel_size);
    VoxelGrid recon_grid(room_x, room_y, room_z, voxel_size);  // for output
    auto network = NodeNetwork::create_3d_cube(room_x, room_y, room_z);

    std::cout << "grid: " << grid.get_nx() << "x" << grid.get_ny()
              << "x" << grid.get_nz() << " (" << grid.get_total()
              << " voxels)" << std::endl;
    std::cout << "network: " << network.num_nodes() << " nodes, "
              << network.num_links() << " links" << std::endl;
    std::cout << std::endl;

    // ---- build forward model ----
    std::cout << "building forward model..." << std::endl;
    ForwardModel forward(grid, network);
    std::cout << "  weight matrix: " << forward.num_links() << " x "
              << forward.num_voxels() << std::endl;
    std::cout << std::endl;

    // ---- build reconstructor ----
    std::cout << "building reconstructor..." << std::endl;
    Reconstructor recon(forward, grid);
    std::cout << "  pre-computed W^T W (" << forward.num_voxels() << " x "
              << forward.num_voxels() << ")" << std::endl;
    std::cout << std::endl;

    // ====================================================================
    // test 1: single person in centre (clean measurements)
    // ====================================================================
    std::cout << "========================================" << std::endl;
    std::cout << "TEST 1: single person centre (no noise)" << std::endl;
    std::cout << "========================================" << std::endl;
    {
        auto scene = Scene::create_single_person_centre();
        scene.apply_to_grid(grid);

        auto y = forward.simulate(grid, 0.0);  // no noise
        std::cout << "  measurements: " << y.size() << " links" << std::endl;

        int affected = 0;
        for (int i = 0; i < y.size(); ++i) {
            if (y(i) > 0.01) ++affected;
        }
        std::cout << "  links affected: " << affected << std::endl;
        std::cout << std::endl;

        // method 1: back-projection
        std::cout << "  [back-projection]" << std::endl;
        auto x_bp = recon.back_projection(y);
        auto r_bp = recon.evaluate_auto_threshold(x_bp, grid, "back-projection");
        print_result(r_bp);
        std::cout << std::endl;

        // method 2: tikhonov with different lambda values
        double lambdas[] = {0.01, 0.1, 1.0, 10.0};
        for (double lam : lambdas) {
            std::cout << "  [tikhonov lambda=" << lam << "]" << std::endl;
            auto x_tik = recon.tikhonov(y, lam);
            auto r_tik = recon.evaluate_auto_threshold(
                x_tik, grid, "tikhonov(lambda=" + std::to_string(lam) + ")");
            print_result(r_tik);
            std::cout << std::endl;
        }
    }

    // ====================================================================
    // test 2: single person with noise
    // ====================================================================
    std::cout << "========================================" << std::endl;
    std::cout << "TEST 2: single person centre (noisy)" << std::endl;
    std::cout << "========================================" << std::endl;
    {
        auto scene = Scene::create_single_person_centre();
        scene.apply_to_grid(grid);

        double noise_levels[] = {0.5, 1.0, 2.0};
        for (double noise : noise_levels) {
            auto y = forward.simulate(grid, noise);
            std::cout << "  noise stddev: " << noise << " dB" << std::endl;

            auto x_tik = recon.tikhonov(y, 1.0);
            auto r = recon.evaluate_auto_threshold(x_tik, grid, "tikhonov(1.0)");
            std::cout << "    localisation error: " << std::fixed
                      << std::setprecision(3) << r.localisation_error << "m"
                      << std::endl;
            std::cout << "    true positives: " << r.true_positives
                      << "  false positives: " << r.false_positives
                      << std::endl;
            std::cout << std::endl;
        }
    }

    // ====================================================================
    // test 3: two people
    // ====================================================================
    std::cout << "========================================" << std::endl;
    std::cout << "TEST 3: two people" << std::endl;
    std::cout << "========================================" << std::endl;
    {
        auto scene = Scene::create_two_people();
        scene.apply_to_grid(grid);
        auto y = forward.simulate(grid, 0.0);

        auto x_bp = recon.back_projection(y);
        auto r_bp = recon.evaluate_auto_threshold(x_bp, grid, "back-projection");
        std::cout << "  [back-projection]" << std::endl;
        print_result(r_bp);
        std::cout << std::endl;

        auto x_tik = recon.tikhonov(y, 1.0);
        auto r_tik = recon.evaluate_auto_threshold(x_tik, grid, "tikhonov(1.0)");
        std::cout << "  [tikhonov lambda=1.0]" << std::endl;
        print_result(r_tik);
        std::cout << std::endl;
    }

    // ====================================================================
    // test 4: person behind wall
    // ====================================================================
    std::cout << "========================================" << std::endl;
    std::cout << "TEST 4: person behind wall" << std::endl;
    std::cout << "========================================" << std::endl;
    {
        auto scene = Scene::create_person_with_wall();
        scene.apply_to_grid(grid);
        auto y = forward.simulate(grid, 0.0);

        auto x_tik = recon.tikhonov(y, 1.0);
        auto r = recon.evaluate_auto_threshold(x_tik, grid, "tikhonov(1.0)");
        std::cout << "  [tikhonov lambda=1.0]" << std::endl;
        print_result(r);
        std::cout << std::endl;
    }

    // ====================================================================
    // test 5: person in corner (edge case)
    // ====================================================================
    std::cout << "========================================" << std::endl;
    std::cout << "TEST 5: person in corner" << std::endl;
    std::cout << "========================================" << std::endl;
    {
        auto scene = Scene::create_single_person_corner();
        scene.apply_to_grid(grid);
        auto y = forward.simulate(grid, 0.0);

        auto x_tik = recon.tikhonov(y, 1.0);
        auto r = recon.evaluate_auto_threshold(x_tik, grid, "tikhonov(1.0)");
        std::cout << "  [tikhonov lambda=1.0]" << std::endl;
        print_result(r);
        std::cout << std::endl;
    }

    // ====================================================================
    // visualisation output
    // ====================================================================
    std::cout << "========================================" << std::endl;
    std::cout << "GENERATING VISUALISATIONS" << std::endl;
    std::cout << "========================================" << std::endl;
    {
        // set up scene and run reconstruction for visualisation
        auto scene = Scene::create_single_person_centre();
        scene.apply_to_grid(grid);

        auto y = forward.simulate(grid, 0.0);
        auto x_tik = recon.tikhonov(y, 1.0);

        // apply reconstruction to a grid for visualisation
        VoxelGrid recon_output(room_x, room_y, room_z, voxel_size);
        recon.apply_to_grid(x_tik, recon_output);

        // 1. terminal ASCII output
        std::cout << std::endl;
        std::cout << "--- ground truth (mid-height slice) ---" << std::endl;
        Visualiser::print_slice_z(grid, grid.get_nz() / 2);
        std::cout << std::endl;

        std::cout << "--- reconstruction (mid-height slice) ---" << std::endl;
        Visualiser::print_slice_z(recon_output, grid.get_nz() / 2);
        std::cout << std::endl;

        std::cout << "--- ground truth (front view, mid-depth) ---" << std::endl;
        Visualiser::print_slice_y(grid, grid.get_ny() / 2);
        std::cout << std::endl;

        // 2. PPM image exports
        Visualiser::export_slice_ppm(grid, grid.get_nz() / 2,
                                      "output_truth_z5.ppm");
        Visualiser::export_slice_ppm(recon_output, grid.get_nz() / 2,
                                      "output_recon_z5.ppm");
        Visualiser::export_comparison_ppm(grid, recon_output, grid.get_nz() / 2,
                                           "output_comparison_z5.ppm");
        std::cout << "exported PPM images: output_truth_z5.ppm, "
                  << "output_recon_z5.ppm, output_comparison_z5.ppm" << std::endl;

        // 3. 3D HTML viewers
        Visualiser::export_3d_html(grid, network,
                                    "output_truth_3d.html",
                                    "RTI Ground Truth - Person Centre");
        Visualiser::export_3d_html(recon_output, network,
                                    "output_recon_3d.html",
                                    "RTI Reconstruction - Tikhonov lambda=1.0");
        Visualiser::export_comparison_3d_html(grid, recon_output, network,
                                               "output_comparison_3d.html");
        std::cout << "exported 3D viewers: output_truth_3d.html, "
                  << "output_recon_3d.html, output_comparison_3d.html" << std::endl;
    }

    // also generate visualisations for two-people scene
    {
        auto scene = Scene::create_two_people();
        scene.apply_to_grid(grid);

        auto y = forward.simulate(grid, 0.0);
        auto x_tik = recon.tikhonov(y, 1.0);

        VoxelGrid recon_output(room_x, room_y, room_z, voxel_size);
        recon.apply_to_grid(x_tik, recon_output);

        Visualiser::export_comparison_3d_html(grid, recon_output, network,
                                               "output_two_people_3d.html");
        std::cout << "exported: output_two_people_3d.html" << std::endl;
    }

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "all phases complete." << std::endl;
    std::cout << "open the .html files in a browser for interactive 3D views."
              << std::endl;

    return 0;
}
