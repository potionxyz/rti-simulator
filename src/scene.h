// ============================================================================
// scene.h - Scene Builder for Test Environments
// ============================================================================
// a scene defines what's inside the room - people, walls, furniture, etc.
// each object is represented as a region of voxels with a specific
// attenuation coefficient (how much it blocks radio signals).
//
// typical attenuation values at 2.4GHz:
//   air:           ~0.0 dB/m    (nothing blocks the signal)
//   human body:    ~3-5 dB/m    (water content absorbs RF)
//   wood:          ~2-3 dB/m
//   concrete:      ~10-15 dB/m  (very dense, heavy blocking)
//   metal:         ~30+ dB/m    (almost total reflection)
//
// scenes can be created programmatically or loaded from JSON files,
// which makes it easy to test different configurations without
// recompiling the code.
// ============================================================================

#ifndef SCENE_H
#define SCENE_H

#include "voxel_grid.h"
#include <string>
#include <vector>

// ============================================================================
// Obstacle - a single object in the scene
// ============================================================================
// defined by a bounding box (min/max coordinates in metres) and an
// attenuation value. when applied to the voxel grid, every voxel
// whose centre falls inside the bounding box gets set to this value.
// ============================================================================
struct Obstacle {
    std::string name;       // descriptive label (e.g. "person_1", "wall_north")
    double min_x, min_y, min_z;  // bounding box minimum corner (metres)
    double max_x, max_y, max_z;  // bounding box maximum corner (metres)
    double attenuation;     // signal absorption in dB/m

    Obstacle(const std::string& name,
             double min_x, double min_y, double min_z,
             double max_x, double max_y, double max_z,
             double attenuation)
        : name(name)
        , min_x(min_x), min_y(min_y), min_z(min_z)
        , max_x(max_x), max_y(max_y), max_z(max_z)
        , attenuation(attenuation) {}
};

// ============================================================================
// Scene - a complete test environment
// ============================================================================
class Scene {
public:
    Scene(const std::string& name = "unnamed");

    // ---- building the scene ----

    // add a rectangular obstacle (bounding box defined in metres)
    void add_obstacle(const Obstacle& obstacle);

    // convenience methods for common objects
    // person: roughly 0.4m x 0.3m x 1.7m, attenuation ~4.0 dB/m
    void add_person(const std::string& name, double centre_x, double centre_y);

    // wall: thin but tall and wide
    void add_wall(const std::string& name,
                  double x1, double y1, double x2, double y2,
                  double thickness, double height, double attenuation);

    // ---- applying to voxel grid ----

    // stamp all obstacles onto a voxel grid
    // any voxel whose centre falls inside an obstacle gets that
    // obstacle's attenuation value. overlapping obstacles use the max.
    void apply_to_grid(VoxelGrid& grid) const;

    // ---- scene I/O ----

    // save scene definition to a JSON file
    void save_json(const std::string& filepath) const;

    // load scene from a JSON file
    static Scene load_json(const std::string& filepath);

    // ---- accessors ----
    const std::string& get_name() const { return name_; }
    const std::vector<Obstacle>& get_obstacles() const { return obstacles_; }

    // ---- preset scenes for testing ----

    // empty room (baseline - no obstacles)
    static Scene create_empty(const std::string& name = "empty");

    // single person standing in the centre of the room
    static Scene create_single_person_centre();

    // single person in a corner
    static Scene create_single_person_corner();

    // two people at different positions
    static Scene create_two_people();

    // person behind a wall (tests through-wall detection)
    static Scene create_person_with_wall();

private:
    std::string name_;
    std::vector<Obstacle> obstacles_;
};

#endif // SCENE_H
