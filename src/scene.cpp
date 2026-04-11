// ============================================================================
// scene.cpp - Implementation of Scene Builder
// ============================================================================

#include "scene.h"
#include "json.hpp"
#include <fstream>    // for file I/O
#include <algorithm>  // for std::max
#include <cmath>      // for mathematical operations

// use the nlohmann json namespace for cleaner code
using json = nlohmann::json;

// ============================================================================
// constructor
// ============================================================================
Scene::Scene(const std::string& name) : name_(name) {}

// ============================================================================
// add_obstacle - register an obstacle in the scene
// ============================================================================
void Scene::add_obstacle(const Obstacle& obstacle) {
    obstacles_.push_back(obstacle);
}

// ============================================================================
// add_person - convenience method for a human-shaped obstacle
// ============================================================================
// a person is modelled as a rectangular prism:
//   width:  0.4m (shoulder to shoulder)
//   depth:  0.3m (front to back)
//   height: 1.7m (feet to head)
//   attenuation: 4.0 dB/m (water content of human body at 2.4GHz)
//
// centre_x, centre_y define where the person is standing on the floor.
// the person always stands from z=0 (floor) upwards.
// ============================================================================
void Scene::add_person(const std::string& name, double centre_x, double centre_y) {
    double width = 0.4;   // metres
    double depth = 0.3;   // metres
    double height = 1.7;  // metres
    double atten = 4.0;   // dB/m

    add_obstacle(Obstacle(
        name,
        centre_x - width / 2.0,   // min_x
        centre_y - depth / 2.0,   // min_y
        0.0,                       // min_z (floor)
        centre_x + width / 2.0,   // max_x
        centre_y + depth / 2.0,   // max_y
        height,                    // max_z
        atten
    ));
}

// ============================================================================
// add_wall - convenience method for a wall obstacle
// ============================================================================
// walls are defined by two endpoints (x1,y1) to (x2,y2), a thickness,
// height, and attenuation value.
//
// for simplicity, walls are always axis-aligned (horizontal or vertical).
// the thickness extends equally on both sides of the line.
// ============================================================================
void Scene::add_wall(const std::string& name,
                     double x1, double y1, double x2, double y2,
                     double thickness, double height, double attenuation) {
    // calculate bounding box from the wall line + thickness
    double half_t = thickness / 2.0;
    double min_x = std::min(x1, x2) - half_t;
    double min_y = std::min(y1, y2) - half_t;
    double max_x = std::max(x1, x2) + half_t;
    double max_y = std::max(y1, y2) + half_t;

    add_obstacle(Obstacle(name, min_x, min_y, 0.0, max_x, max_y, height, attenuation));
}

// ============================================================================
// apply_to_grid - stamp obstacles onto the voxel grid
// ============================================================================
// for each voxel, we check if its centre point falls inside any obstacle.
// if it does, we set the voxel's attenuation to the obstacle's value.
//
// if a voxel falls inside multiple obstacles (overlapping), we take the
// maximum attenuation - the densest material "wins". this makes physical
// sense because the signal gets absorbed by whatever is most opaque.
// ============================================================================
void Scene::apply_to_grid(VoxelGrid& grid) const {
    // first, clear the grid to all zeros (empty room)
    grid.reset();

    // iterate through every voxel in the grid
    for (int k = 0; k < grid.get_nz(); ++k) {
        for (int j = 0; j < grid.get_ny(); ++j) {
            for (int i = 0; i < grid.get_nx(); ++i) {
                // get the world-space centre of this voxel
                double cx, cy, cz;
                grid.get_voxel_centre(i, j, k, cx, cy, cz);

                // check against every obstacle
                double max_atten = 0.0;
                for (const auto& obs : obstacles_) {
                    // is the voxel centre inside this obstacle's bounding box?
                    if (cx >= obs.min_x && cx <= obs.max_x &&
                        cy >= obs.min_y && cy <= obs.max_y &&
                        cz >= obs.min_z && cz <= obs.max_z) {
                        // take the maximum if multiple obstacles overlap
                        max_atten = std::max(max_atten, obs.attenuation);
                    }
                }

                // set the voxel value (0.0 if no obstacles hit it)
                if (max_atten > 0.0) {
                    grid.set(i, j, k, max_atten);
                }
            }
        }
    }
}

// ============================================================================
// save_json - export scene definition to JSON
// ============================================================================
// saves the scene in a format that can be loaded back later.
// useful for sharing test configurations and reproducing results.
// ============================================================================
void Scene::save_json(const std::string& filepath) const {
    json j;
    j["name"] = name_;
    j["obstacles"] = json::array();

    for (const auto& obs : obstacles_) {
        j["obstacles"].push_back({
            {"name", obs.name},
            {"min", {obs.min_x, obs.min_y, obs.min_z}},
            {"max", {obs.max_x, obs.max_y, obs.max_z}},
            {"attenuation", obs.attenuation}
        });
    }

    // write to file with pretty printing (4-space indent)
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("could not open file for writing: " + filepath);
    }
    file << j.dump(4) << std::endl;
}

// ============================================================================
// load_json - import scene from JSON file
// ============================================================================
Scene Scene::load_json(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("could not open scene file: " + filepath);
    }

    json j;
    file >> j;

    Scene scene(j.value("name", "loaded_scene"));

    for (const auto& obs_json : j["obstacles"]) {
        scene.add_obstacle(Obstacle(
            obs_json["name"].get<std::string>(),
            obs_json["min"][0].get<double>(),
            obs_json["min"][1].get<double>(),
            obs_json["min"][2].get<double>(),
            obs_json["max"][0].get<double>(),
            obs_json["max"][1].get<double>(),
            obs_json["max"][2].get<double>(),
            obs_json["attenuation"].get<double>()
        ));
    }

    return scene;
}

// ============================================================================
// preset scenes
// ============================================================================

Scene Scene::create_empty(const std::string& name) {
    // no obstacles - used as baseline for calibration
    return Scene(name);
}

// ============================================================================
// single person in the centre of a 2m x 2m room
// ============================================================================
Scene Scene::create_single_person_centre() {
    Scene scene("single_person_centre");
    scene.add_person("person_1", 1.0, 1.0);  // dead centre
    return scene;
}

// ============================================================================
// single person in the corner (harder to detect - fewer links blocked)
// ============================================================================
Scene Scene::create_single_person_corner() {
    Scene scene("single_person_corner");
    scene.add_person("person_1", 0.3, 0.3);  // near node 0
    return scene;
}

// ============================================================================
// two people at different positions
// ============================================================================
// this tests whether the reconstruction can resolve multiple objects
// or if they blur into one blob
// ============================================================================
Scene Scene::create_two_people() {
    Scene scene("two_people");
    scene.add_person("person_1", 0.5, 0.5);   // front-left area
    scene.add_person("person_2", 1.5, 1.5);   // back-right area
    return scene;
}

// ============================================================================
// person behind a wall
// ============================================================================
// tests through-wall detection capability.
// the wall partially blocks signals, but not completely, so the
// reconstruction should still detect the person behind it.
// ============================================================================
Scene Scene::create_person_with_wall() {
    Scene scene("person_with_wall");

    // wall across the middle of the room (y = 1.0)
    // concrete wall: 0.15m thick, 2m tall, 12 dB/m attenuation
    scene.add_wall("wall_centre", 0.0, 1.0, 2.0, 1.0, 0.15, 2.0, 12.0);

    // person standing behind the wall
    scene.add_person("person_1", 1.0, 1.5);

    return scene;
}
