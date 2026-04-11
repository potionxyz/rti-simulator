// ============================================================================
// main.cpp - RTI 3D Volumetric Simulator Entry Point
// ============================================================================
// this runs the full simulation pipeline:
//   1. create voxel grid (the room)
//   2. place nodes (the sensors)
//   3. define a scene (where objects are)
//   4. run forward model (simulate signal measurements)
//   5. display the results
//
// later phases will add reconstruction and visualisation.
// ============================================================================

#include <iostream>
#include <iomanip>   // for std::setprecision, std::setw
#include "voxel_grid.h"
#include "node.h"
#include "scene.h"
#include "forward_model.h"

int main() {
    std::cout << "=== RTI 3D Volumetric Simulator ===" << std::endl;
    std::cout << std::endl;

    // ---- step 1: create the voxel grid ----
    double room_x = 2.0;
    double room_y = 2.0;
    double room_z = 2.0;
    double voxel_size = 0.1;  // 10cm cubes

    VoxelGrid grid(room_x, room_y, room_z, voxel_size);

    std::cout << "voxel grid: " << grid.get_nx() << "x" << grid.get_ny()
              << "x" << grid.get_nz() << " (" << grid.get_total()
              << " voxels, " << voxel_size * 100 << "cm resolution)"
              << std::endl;

    // ---- step 2: create the node network ----
    auto network = NodeNetwork::create_3d_cube(room_x, room_y, room_z);

    std::cout << "node network: " << network.num_nodes() << " nodes, "
              << network.num_links() << " links (3D cube)" << std::endl;
    std::cout << std::endl;

    // ---- step 3: build the forward model ----
    std::cout << "computing weight matrix (Siddon's algorithm)..." << std::endl;
    ForwardModel forward(grid, network);

    // print weight matrix stats
    const auto& W = forward.get_weight_matrix();
    std::cout << "  weight matrix: " << W.rows() << " x " << W.cols() << std::endl;
    std::cout << "  non-zero entries: " << W.nonZeros()
              << " (" << std::fixed << std::setprecision(1)
              << (100.0 * W.nonZeros() / (W.rows() * W.cols())) << "% dense)"
              << std::endl;
    std::cout << std::endl;

    // ---- step 4: test with different scenes ----
    std::cout << "--- testing scenes ---" << std::endl;
    std::cout << std::endl;

    // scene A: empty room (baseline)
    {
        auto scene = Scene::create_empty();
        scene.apply_to_grid(grid);
        auto y = forward.simulate(grid, 0.0);

        std::cout << "[empty room]" << std::endl;
        std::cout << "  max attenuation: " << y.maxCoeff() << " dB" << std::endl;
        std::cout << "  all links clear (expected: 0.0)" << std::endl;
        std::cout << std::endl;
    }

    // scene B: single person in centre
    {
        auto scene = Scene::create_single_person_centre();
        scene.apply_to_grid(grid);

        // count occupied voxels
        int occupied = 0;
        for (size_t i = 0; i < grid.get_total(); ++i) {
            if (grid.get_flat(i) > 0.0) ++occupied;
        }

        // simulate with no noise
        auto y_clean = forward.simulate(grid, 0.0);

        // simulate with moderate noise
        auto y_noisy = forward.simulate(grid, 1.0);

        std::cout << "[single person centre]" << std::endl;
        std::cout << "  occupied voxels: " << occupied << " / "
                  << grid.get_total() << std::endl;
        std::cout << "  clean measurements:" << std::endl;
        std::cout << "    links affected: ";
        int affected = 0;
        for (int i = 0; i < y_clean.size(); ++i) {
            if (y_clean(i) > 0.01) ++affected;
        }
        std::cout << affected << " / " << y_clean.size() << std::endl;
        std::cout << "    max attenuation: " << std::setprecision(2)
                  << y_clean.maxCoeff() << " dB" << std::endl;
        std::cout << "    mean attenuation: " << std::setprecision(2)
                  << y_clean.mean() << " dB" << std::endl;

        std::cout << "  noisy measurements (1.0 dB stddev):" << std::endl;
        std::cout << "    max attenuation: " << std::setprecision(2)
                  << y_noisy.maxCoeff() << " dB" << std::endl;
        std::cout << "    mean attenuation: " << std::setprecision(2)
                  << y_noisy.mean() << " dB" << std::endl;
        std::cout << std::endl;

        // print per-link measurements for the clean case
        std::cout << "  per-link attenuation (clean):" << std::endl;
        const auto& links = network.get_links();
        for (int i = 0; i < y_clean.size(); ++i) {
            if (y_clean(i) > 0.01) {
                std::cout << "    link " << links[i].node_a << "-"
                          << links[i].node_b << ": "
                          << std::setprecision(3) << y_clean(i) << " dB"
                          << std::endl;
            }
        }
        std::cout << std::endl;
    }

    // scene C: two people
    {
        auto scene = Scene::create_two_people();
        scene.apply_to_grid(grid);
        auto y = forward.simulate(grid, 0.0);

        int affected = 0;
        for (int i = 0; i < y.size(); ++i) {
            if (y(i) > 0.01) ++affected;
        }

        std::cout << "[two people]" << std::endl;
        std::cout << "  links affected: " << affected << " / "
                  << y.size() << std::endl;
        std::cout << "  max attenuation: " << std::setprecision(2)
                  << y.maxCoeff() << " dB" << std::endl;
        std::cout << std::endl;
    }

    // scene D: person behind wall
    {
        auto scene = Scene::create_person_with_wall();
        scene.apply_to_grid(grid);
        auto y = forward.simulate(grid, 0.0);

        std::cout << "[person behind wall]" << std::endl;
        std::cout << "  max attenuation: " << std::setprecision(2)
                  << y.maxCoeff() << " dB" << std::endl;
        std::cout << "  mean attenuation: " << std::setprecision(2)
                  << y.mean() << " dB" << std::endl;
        std::cout << std::endl;
    }

    // ---- step 5: save a test scene to JSON ----
    {
        auto scene = Scene::create_single_person_centre();
        scene.save_json("scenes/test_person_centre.json");
        std::cout << "saved test scene to scenes/test_person_centre.json" << std::endl;

        // verify we can load it back
        auto loaded = Scene::load_json("scenes/test_person_centre.json");
        std::cout << "loaded scene: " << loaded.get_name()
                  << " (" << loaded.get_obstacles().size() << " obstacles)"
                  << std::endl;
    }

    std::cout << std::endl;
    std::cout << "phases 1-4 complete - forward model working." << std::endl;
    std::cout << "next: reconstruction engine (solving the inverse problem)."
              << std::endl;

    return 0;
}
