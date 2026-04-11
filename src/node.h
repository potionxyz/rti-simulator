// ============================================================================
// node.h - Sensor Node and Radio Link Definitions
// ============================================================================
// a "node" is one ESP32 sensor mounted at a known position in the room.
// a "link" is a radio signal path between two nodes.
//
// in the real hardware version, each node transmits WiFi packets and
// measures the received signal strength (RSS) from every other node.
// in this simulator, we calculate what the RSS would be based on what
// objects are blocking the path between each pair of nodes.
//
// with N nodes, there are N*(N-1)/2 unique links:
//   4 nodes  =  6 links
//   6 nodes  = 15 links
//   8 nodes  = 28 links
//  12 nodes  = 66 links
//
// more links = more data = better reconstruction accuracy
// ============================================================================

#ifndef NODE_H
#define NODE_H

#include <vector>
#include <string>

// ============================================================================
// Node - a single sensor in the network
// ============================================================================
// stores the 3D position and an ID for identification.
// positions are in metres, relative to the room origin (0, 0, 0) which
// is the bottom-left-front corner.
// ============================================================================
struct Node {
    int id;        // unique identifier (0, 1, 2, ...)
    double x;      // position along room width (metres)
    double y;      // position along room depth (metres)
    double z;      // position along room height (metres)

    // constructor - create a node at a specific position
    Node(int id, double x, double y, double z)
        : id(id), x(x), y(y), z(z) {}
};

// ============================================================================
// Link - a radio signal path between two nodes
// ============================================================================
// each link has a baseline RSS (signal strength when room is empty)
// and a current RSS (signal strength right now). the difference
// (attenuation) tells us if something is blocking the path.
//
// attenuation = baseline_rss - current_rss
//   positive attenuation = something is blocking the signal
//   zero attenuation = path is clear
//   negative attenuation = noise (shouldn't happen in clean data)
// ============================================================================
struct Link {
    int node_a;         // ID of the first node
    int node_b;         // ID of the second node
    double baseline_rss; // RSS when room is empty (dBm)
    double current_rss;  // RSS right now (dBm)

    // computed attenuation - how much signal was lost
    double attenuation() const {
        return baseline_rss - current_rss;
    }

    // constructor
    Link(int a, int b)
        : node_a(a), node_b(b), baseline_rss(0.0), current_rss(0.0) {}
};

// ============================================================================
// NodeNetwork - manages all nodes and their links
// ============================================================================
// handles creating nodes, generating all possible links between them,
// and providing access to the link data for the forward model and
// reconstruction algorithms.
// ============================================================================
class NodeNetwork {
public:
    NodeNetwork() = default;

    // add a node at a specific position
    void add_node(int id, double x, double y, double z);

    // generate all unique links between nodes
    // called after all nodes are added
    // creates N*(N-1)/2 links for N nodes
    void generate_links();

    // ---- accessors ----
    const std::vector<Node>& get_nodes() const { return nodes_; }
    const std::vector<Link>& get_links() const { return links_; }
    std::vector<Link>& get_links() { return links_; }
    size_t num_nodes() const { return nodes_.size(); }
    size_t num_links() const { return links_.size(); }

    // get a specific node by ID
    const Node& get_node(int id) const;

    // ---- preset configurations ----
    // these set up nodes in standard arrangements for testing

    // 4 nodes in corners of a 2D square (all at same height)
    static NodeNetwork create_2d_square(double room_x, double room_y, double height);

    // 8 nodes at corners of a 3D cube
    static NodeNetwork create_3d_cube(double room_x, double room_y, double room_z);

    // custom number of nodes distributed around the perimeter
    static NodeNetwork create_perimeter(double room_x, double room_y, double room_z,
                                         int nodes_per_layer, int num_layers);

private:
    std::vector<Node> nodes_;   // all sensor nodes
    std::vector<Link> links_;   // all radio links between nodes
};

#endif // NODE_H
