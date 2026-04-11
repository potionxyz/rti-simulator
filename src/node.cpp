// ============================================================================
// node.cpp - Implementation of Node Network
// ============================================================================

#include "node.h"
#include <stdexcept>

// ============================================================================
// add_node - register a new sensor at a position
// ============================================================================
void NodeNetwork::add_node(int id, double x, double y, double z) {
    nodes_.emplace_back(id, x, y, z);
}

// ============================================================================
// generate_links - create all unique pairs
// ============================================================================
// for N nodes, we iterate through every pair (i, j) where i < j
// to avoid duplicates. this gives us exactly N*(N-1)/2 links.
//
// example with 4 nodes (0,1,2,3):
//   links: 0-1, 0-2, 0-3, 1-2, 1-3, 2-3 = 6 links
// ============================================================================
void NodeNetwork::generate_links() {
    links_.clear();

    for (size_t i = 0; i < nodes_.size(); ++i) {
        for (size_t j = i + 1; j < nodes_.size(); ++j) {
            links_.emplace_back(nodes_[i].id, nodes_[j].id);
        }
    }
}

// ============================================================================
// get_node - look up a node by its ID
// ============================================================================
const Node& NodeNetwork::get_node(int id) const {
    for (const auto& node : nodes_) {
        if (node.id == id) {
            return node;
        }
    }
    throw std::out_of_range("node ID not found: " + std::to_string(id));
}

// ============================================================================
// create_2d_square - 4 nodes in corners at a fixed height
// ============================================================================
// layout (top-down view):
//
//   node 2 -------- node 3
//     |                |
//     |     room       |
//     |                |
//   node 0 -------- node 1
//
// all nodes are at the same z height
// this gives 6 links: 4 edges + 2 diagonals
// ============================================================================
NodeNetwork NodeNetwork::create_2d_square(double room_x, double room_y, double height) {
    NodeNetwork network;

    // four corners, all at the same height
    network.add_node(0, 0.0,    0.0,    height);  // bottom-left
    network.add_node(1, room_x, 0.0,    height);  // bottom-right
    network.add_node(2, 0.0,    room_y, height);  // top-left
    network.add_node(3, room_x, room_y, height);  // top-right

    network.generate_links();
    return network;
}

// ============================================================================
// create_3d_cube - 8 nodes at corners of a cuboid
// ============================================================================
// this is the main configuration for 3D tomography.
// 8 corners give 28 links: 12 edges + 12 face diagonals + 4 space diagonals
//
// layout:
//
//        node 6 -------- node 7     (top layer, z = room_z)
//       /|              /|
//      / |             / |
//   node 4 -------- node 5
//     |  node 2 ------|-- node 3    (bottom layer, z = 0)
//     | /             | /
//     |/              |/
//   node 0 -------- node 1
//
// the space diagonals (0-7, 1-6, 2-5, 3-4) are the most important
// for detecting height - they cross the full volume diagonally
// ============================================================================
NodeNetwork NodeNetwork::create_3d_cube(double room_x, double room_y, double room_z) {
    NodeNetwork network;

    // bottom layer (z = 0)
    network.add_node(0, 0.0,    0.0,    0.0);      // bottom-left-front
    network.add_node(1, room_x, 0.0,    0.0);      // bottom-right-front
    network.add_node(2, 0.0,    room_y, 0.0);      // bottom-left-back
    network.add_node(3, room_x, room_y, 0.0);      // bottom-right-back

    // top layer (z = room_z)
    network.add_node(4, 0.0,    0.0,    room_z);   // top-left-front
    network.add_node(5, room_x, 0.0,    room_z);   // top-right-front
    network.add_node(6, 0.0,    room_y, room_z);   // top-left-back
    network.add_node(7, room_x, room_y, room_z);   // top-right-back

    network.generate_links();
    return network;
}

// ============================================================================
// create_perimeter - custom node count distributed around the room
// ============================================================================
// distributes nodes evenly around the perimeter of the room at multiple
// height layers. this is for testing how node count affects accuracy.
//
// nodes_per_layer: how many nodes on each horizontal ring
// num_layers: how many vertical layers (minimum 2 for 3D)
// ============================================================================
NodeNetwork NodeNetwork::create_perimeter(double room_x, double room_y, double room_z,
                                           int nodes_per_layer, int num_layers) {
    NodeNetwork network;
    int id = 0;

    for (int layer = 0; layer < num_layers; ++layer) {
        // height of this layer - evenly spaced from bottom to top
        double z = (num_layers == 1) ? room_z / 2.0
                   : room_z * layer / (num_layers - 1);

        // distribute nodes around the perimeter of the rectangle
        // total perimeter length
        double perimeter = 2.0 * (room_x + room_y);
        double spacing = perimeter / nodes_per_layer;

        for (int n = 0; n < nodes_per_layer; ++n) {
            double dist = n * spacing;  // distance along perimeter
            double x, y;

            if (dist < room_x) {
                // bottom edge (y = 0)
                x = dist;
                y = 0.0;
            } else if (dist < room_x + room_y) {
                // right edge (x = room_x)
                x = room_x;
                y = dist - room_x;
            } else if (dist < 2.0 * room_x + room_y) {
                // top edge (y = room_y)
                x = room_x - (dist - room_x - room_y);
                y = room_y;
            } else {
                // left edge (x = 0)
                x = 0.0;
                y = room_y - (dist - 2.0 * room_x - room_y);
            }

            network.add_node(id++, x, y, z);
        }
    }

    network.generate_links();
    return network;
}
