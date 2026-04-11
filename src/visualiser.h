// ============================================================================
// visualiser.h - Output and Visualisation
// ============================================================================
// provides multiple ways to see the reconstruction results:
//
// 1. terminal output: ASCII art slices through the voxel grid
//    quick and dirty, no dependencies, works anywhere
//
// 2. PPM image export: 2D slice images with colour mapping
//    PPM is a dead-simple image format (no library needed)
//    can be opened in any image viewer or converted to PNG
//
// 3. HTML/Three.js export: interactive 3D viewer in a browser
//    generates a standalone HTML file with embedded JavaScript
//    shows the voxel cube with colour-coded attenuation values
//    most impressive for demos and presentations
//
// all methods support both ground truth and reconstruction views,
// plus side-by-side comparisons.
// ============================================================================

#ifndef VISUALISER_H
#define VISUALISER_H

#include "voxel_grid.h"
#include "node.h"
#include <string>
#include <vector>

class Visualiser {
public:
    // -----------------------------------------------------------------------
    // terminal output
    // -----------------------------------------------------------------------

    // print a horizontal slice (constant z) of the voxel grid
    // z_index: which layer to show (0 = floor, nz-1 = ceiling)
    // threshold: values below this are shown as empty
    static void print_slice_z(const VoxelGrid& grid, int z_index,
                               double threshold = 0.0);

    // print a vertical slice (constant y) - front view
    static void print_slice_y(const VoxelGrid& grid, int y_index,
                               double threshold = 0.0);

    // print all z slices stacked (compact overview)
    static void print_all_slices(const VoxelGrid& grid, double threshold = 0.0);

    // -----------------------------------------------------------------------
    // PPM image export
    // -----------------------------------------------------------------------
    // PPM (Portable Pixel Map) is the simplest image format:
    //   header: "P6\nwidth height\n255\n"
    //   body: width*height pixels, each 3 bytes (R, G, B)
    //
    // no external libraries needed. can convert to PNG with:
    //   convert output.ppm output.png  (ImageMagick)

    // export a z-slice as a colour-mapped PPM image
    // uses a heat map: black -> blue -> red -> yellow -> white
    static void export_slice_ppm(const VoxelGrid& grid, int z_index,
                                  const std::string& filepath,
                                  int scale = 20);  // pixels per voxel

    // export side-by-side comparison (ground truth vs reconstruction)
    static void export_comparison_ppm(const VoxelGrid& ground_truth,
                                       const VoxelGrid& reconstruction,
                                       int z_index,
                                       const std::string& filepath,
                                       int scale = 20);

    // export all z-slices as separate PPM files
    // files are named: prefix_z00.ppm, prefix_z01.ppm, etc.
    static void export_all_slices_ppm(const VoxelGrid& grid,
                                       const std::string& prefix,
                                       int scale = 20);

    // -----------------------------------------------------------------------
    // HTML/Three.js 3D viewer
    // -----------------------------------------------------------------------
    // generates a standalone HTML file with an embedded Three.js scene
    // that renders the voxel grid as coloured cubes.
    //
    // features:
    //   - orbit controls (click and drag to rotate)
    //   - transparent cubes for low attenuation, opaque for high
    //   - sensor node positions shown as spheres
    //   - link lines shown as thin lines
    //   - ground truth and reconstruction shown side by side

    // export a single voxel grid as an interactive 3D HTML viewer
    static void export_3d_html(const VoxelGrid& grid,
                                const NodeNetwork& network,
                                const std::string& filepath,
                                const std::string& title = "RTI Reconstruction",
                                double threshold = 0.0);

    // export ground truth vs reconstruction side by side
    static void export_comparison_3d_html(const VoxelGrid& ground_truth,
                                           const VoxelGrid& reconstruction,
                                           const NodeNetwork& network,
                                           const std::string& filepath,
                                           double threshold = 0.0);

private:
    // colour mapping: value [0,1] -> RGB
    // uses a heat map inspired by scientific visualisation
    struct Colour { unsigned char r, g, b; };
    static Colour heat_map(double value);
};

#endif // VISUALISER_H
