// ============================================================================
// main.cpp - RTI 3D Volumetric Simulator Entry Point
// ============================================================================
// this is the starting point of the simulator. right now it just sets up
// a basic voxel grid and node network to verify everything compiles and
// the core data structures work correctly.
//
// as i build out more modules (forward model, reconstruction, visualisation),
// this file will grow into the full simulation pipeline:
//   1. create voxel grid (the room)
//   2. place nodes (the sensors)
//   3. define a scene (where objects are)
//   4. run forward model (simulate signal measurements)
//   5. run reconstruction (recover the scene from measurements)
//   6. visualise the result
// ============================================================================

#include <iostream>   // for std::cout - printing to terminal
#include "voxel_grid.h"
#include "node.h"

int main() {
    std::cout << "=== RTI 3D Volumetric Simulator ===" << std::endl;
    std::cout << std::endl;

    // ---- step 1: create the voxel grid ----
    // 2m x 2m x 2m room, 0.1m voxel size = 20x20x20 = 8000 voxels
    double room_x = 2.0;  // metres
    double room_y = 2.0;
    double room_z = 2.0;
    double voxel_size = 0.1;  // 10cm cubes

    VoxelGrid grid(room_x, room_y, room_z, voxel_size);

    std::cout << "voxel grid created:" << std::endl;
    std::cout << "  room: " << room_x << "m x " << room_y << "m x " << room_z << "m" << std::endl;
    std::cout << "  voxel size: " << voxel_size << "m" << std::endl;
    std::cout << "  grid: " << grid.get_nx() << " x " << grid.get_ny()
              << " x " << grid.get_nz() << std::endl;
    std::cout << "  total voxels: " << grid.get_total() << std::endl;
    std::cout << std::endl;

    // ---- step 2: create the node network ----
    // 8-node cube configuration - same as the physical ESP32 setup
    auto network = NodeNetwork::create_3d_cube(room_x, room_y, room_z);

    std::cout << "node network created (3D cube):" << std::endl;
    std::cout << "  nodes: " << network.num_nodes() << std::endl;
    std::cout << "  links: " << network.num_links() << std::endl;
    std::cout << std::endl;

    // print node positions
    std::cout << "node positions:" << std::endl;
    for (const auto& node : network.get_nodes()) {
        std::cout << "  node " << node.id << ": ("
                  << node.x << ", " << node.y << ", " << node.z << ")" << std::endl;
    }
    std::cout << std::endl;

    // ---- step 3: quick test of voxel operations ----
    // simulate a person standing in the centre of the room
    // human body attenuation is roughly 3-5 dB/m for 2.4GHz WiFi
    double human_atten = 4.0;  // dB/m

    // place a "person" in the centre (around voxels 9-10 in each axis)
    // a person is roughly 0.4m wide, 0.3m deep, 1.7m tall
    for (int i = 8; i <= 11; ++i) {      // 0.4m width
        for (int j = 9; j <= 11; ++j) {  // 0.3m depth
            for (int k = 0; k <= 16; ++k) {  // 1.7m height
                grid.set(i, j, k, human_atten);
            }
        }
    }

    // count how many voxels are occupied
    int occupied = 0;
    for (size_t idx = 0; idx < grid.get_total(); ++idx) {
        if (grid.get_flat(idx) > 0.0) {
            ++occupied;
        }
    }

    std::cout << "test scene (person in centre):" << std::endl;
    std::cout << "  occupied voxels: " << occupied
              << " / " << grid.get_total() << std::endl;

    // verify a specific voxel
    double centre_val = grid.get(10, 10, 8);  // middle of the person
    std::cout << "  centre voxel (10,10,8) attenuation: "
              << centre_val << " dB/m" << std::endl;

    // get world position of that voxel
    double cx, cy, cz;
    grid.get_voxel_centre(10, 10, 8, cx, cy, cz);
    std::cout << "  centre voxel world position: ("
              << cx << "m, " << cy << "m, " << cz << "m)" << std::endl;

    std::cout << std::endl;
    std::cout << "phase 1 complete - core data structures working." << std::endl;

    return 0;
}
