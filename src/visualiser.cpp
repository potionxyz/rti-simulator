// ============================================================================
// visualiser.cpp - Implementation of Output and Visualisation
// ============================================================================

#include "visualiser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

// ============================================================================
// heat_map - convert a normalised value [0,1] to an RGB colour
// ============================================================================
// colour gradient: black -> blue -> cyan -> green -> yellow -> red -> white
// this is a standard scientific colour map that makes it easy to see
// the difference between low and high values.
//
// the gradient is split into 5 segments, each transitioning between
// two colours. the input value selects which segment and where within it.
// ============================================================================
Visualiser::Colour Visualiser::heat_map(double value) {
    // clamp to [0, 1]
    value = std::max(0.0, std::min(1.0, value));

    unsigned char r, g, b;

    if (value < 0.2) {
        // black to blue
        double t = value / 0.2;
        r = 0; g = 0; b = static_cast<unsigned char>(t * 255);
    } else if (value < 0.4) {
        // blue to cyan
        double t = (value - 0.2) / 0.2;
        r = 0; g = static_cast<unsigned char>(t * 255); b = 255;
    } else if (value < 0.6) {
        // cyan to green
        double t = (value - 0.4) / 0.2;
        r = 0; g = 255; b = static_cast<unsigned char>((1 - t) * 255);
    } else if (value < 0.8) {
        // green to yellow
        double t = (value - 0.6) / 0.2;
        r = static_cast<unsigned char>(t * 255); g = 255; b = 0;
    } else {
        // yellow to red
        double t = (value - 0.8) / 0.2;
        r = 255; g = static_cast<unsigned char>((1 - t) * 255); b = 0;
    }

    return {r, g, b};
}

// ============================================================================
// print_slice_z - ASCII art of a horizontal slice
// ============================================================================
// shows a top-down view at a specific height.
// uses different characters for different attenuation levels:
//   ' ' = empty (0)
//   '.' = very low
//   ':' = low
//   '+' = medium
//   '#' = high
//   '@' = very high
// ============================================================================
void Visualiser::print_slice_z(const VoxelGrid& grid, int z_index,
                                double threshold) {
    // find the maximum value in this slice for normalisation
    double max_val = 0.0;
    for (int j = 0; j < grid.get_ny(); ++j) {
        for (int i = 0; i < grid.get_nx(); ++i) {
            max_val = std::max(max_val, grid.get(i, j, z_index));
        }
    }

    // character set ordered by "density"
    const char chars[] = " .:-=+*#%@";
    const int num_chars = 10;

    std::cout << "z=" << z_index << " (height="
              << std::fixed << std::setprecision(1)
              << (z_index + 0.5) * grid.get_voxel_size() << "m):" << std::endl;

    // print top border
    std::cout << "+" << std::string(grid.get_nx(), '-') << "+" << std::endl;

    // print rows (y goes top to bottom in the display)
    for (int j = grid.get_ny() - 1; j >= 0; --j) {
        std::cout << "|";
        for (int i = 0; i < grid.get_nx(); ++i) {
            double val = grid.get(i, j, z_index);
            if (val <= threshold || max_val <= 0.0) {
                std::cout << ' ';
            } else {
                double normalised = val / max_val;
                int idx = static_cast<int>(normalised * (num_chars - 1));
                idx = std::min(idx, num_chars - 1);
                std::cout << chars[idx];
            }
        }
        std::cout << "|" << std::endl;
    }

    // print bottom border
    std::cout << "+" << std::string(grid.get_nx(), '-') << "+" << std::endl;
}

// ============================================================================
// print_slice_y - ASCII art of a vertical front-view slice
// ============================================================================
void Visualiser::print_slice_y(const VoxelGrid& grid, int y_index,
                                double threshold) {
    double max_val = 0.0;
    for (int k = 0; k < grid.get_nz(); ++k) {
        for (int i = 0; i < grid.get_nx(); ++i) {
            max_val = std::max(max_val, grid.get(i, y_index, k));
        }
    }

    const char chars[] = " .:-=+*#%@";
    const int num_chars = 10;

    std::cout << "y=" << y_index << " (depth="
              << std::fixed << std::setprecision(1)
              << (y_index + 0.5) * grid.get_voxel_size() << "m):" << std::endl;

    std::cout << "+" << std::string(grid.get_nx(), '-') << "+" << std::endl;

    // z goes top to bottom (ceiling at top)
    for (int k = grid.get_nz() - 1; k >= 0; --k) {
        std::cout << "|";
        for (int i = 0; i < grid.get_nx(); ++i) {
            double val = grid.get(i, y_index, k);
            if (val <= threshold || max_val <= 0.0) {
                std::cout << ' ';
            } else {
                double normalised = val / max_val;
                int idx = static_cast<int>(normalised * (num_chars - 1));
                idx = std::min(idx, num_chars - 1);
                std::cout << chars[idx];
            }
        }
        std::cout << "|" << std::endl;
    }

    std::cout << "+" << std::string(grid.get_nx(), '-') << "+" << std::endl;
}

// ============================================================================
// print_all_slices - compact overview of every z layer
// ============================================================================
void Visualiser::print_all_slices(const VoxelGrid& grid, double threshold) {
    for (int k = 0; k < grid.get_nz(); ++k) {
        // check if this slice has any content
        bool has_content = false;
        for (int j = 0; j < grid.get_ny() && !has_content; ++j) {
            for (int i = 0; i < grid.get_nx() && !has_content; ++i) {
                if (grid.get(i, j, k) > threshold) has_content = true;
            }
        }
        if (has_content) {
            print_slice_z(grid, k, threshold);
            std::cout << std::endl;
        }
    }
}

// ============================================================================
// export_slice_ppm - write a z-slice as a PPM image
// ============================================================================
// each voxel is rendered as a (scale x scale) block of pixels.
// the colour comes from the heat_map function.
// ============================================================================
void Visualiser::export_slice_ppm(const VoxelGrid& grid, int z_index,
                                   const std::string& filepath, int scale) {
    int nx = grid.get_nx();
    int ny = grid.get_ny();
    int img_w = nx * scale;
    int img_h = ny * scale;

    // find max value for normalisation
    double max_val = 0.0;
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            max_val = std::max(max_val, grid.get(i, j, z_index));
        }
    }

    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "error: could not open " << filepath << std::endl;
        return;
    }

    // PPM header
    file << "P6\n" << img_w << " " << img_h << "\n255\n";

    // write pixels (top to bottom, y inverted so y=max is at top)
    for (int py = img_h - 1; py >= 0; --py) {
        for (int px = 0; px < img_w; ++px) {
            int i = px / scale;
            int j = py / scale;
            double val = grid.get(i, j, z_index);
            double norm = (max_val > 0.0) ? val / max_val : 0.0;
            Colour c = heat_map(norm);
            file.put(static_cast<char>(c.r));
            file.put(static_cast<char>(c.g));
            file.put(static_cast<char>(c.b));
        }
    }
}

// ============================================================================
// export_comparison_ppm - side by side ground truth vs reconstruction
// ============================================================================
void Visualiser::export_comparison_ppm(const VoxelGrid& ground_truth,
                                        const VoxelGrid& reconstruction,
                                        int z_index,
                                        const std::string& filepath,
                                        int scale) {
    int nx = ground_truth.get_nx();
    int ny = ground_truth.get_ny();
    int gap = 2;  // pixel gap between the two images
    int img_w = nx * scale * 2 + gap;
    int img_h = ny * scale;

    // find max across both for consistent colour mapping
    double max_val = 0.0;
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            max_val = std::max(max_val, ground_truth.get(i, j, z_index));
            max_val = std::max(max_val, reconstruction.get(i, j, z_index));
        }
    }

    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "error: could not open " << filepath << std::endl;
        return;
    }

    file << "P6\n" << img_w << " " << img_h << "\n255\n";

    for (int py = img_h - 1; py >= 0; --py) {
        for (int px = 0; px < img_w; ++px) {
            int half_w = nx * scale;

            if (px >= half_w && px < half_w + gap) {
                // gap between images (white line)
                file.put(static_cast<char>(100));
                file.put(static_cast<char>(100));
                file.put(static_cast<char>(100));
                continue;
            }

            int local_px = (px < half_w) ? px : px - half_w - gap;
            int i = local_px / scale;
            int j = py / scale;

            double val;
            if (px < half_w) {
                val = ground_truth.get(i, j, z_index);
            } else {
                val = reconstruction.get(i, j, z_index);
            }

            double norm = (max_val > 0.0) ? val / max_val : 0.0;
            Colour c = heat_map(norm);
            file.put(static_cast<char>(c.r));
            file.put(static_cast<char>(c.g));
            file.put(static_cast<char>(c.b));
        }
    }
}

// ============================================================================
// export_all_slices_ppm - batch export every z layer
// ============================================================================
void Visualiser::export_all_slices_ppm(const VoxelGrid& grid,
                                        const std::string& prefix, int scale) {
    for (int k = 0; k < grid.get_nz(); ++k) {
        std::ostringstream filename;
        filename << prefix << "_z" << std::setw(2) << std::setfill('0') << k << ".ppm";
        export_slice_ppm(grid, k, filename.str(), scale);
    }
}

// ============================================================================
// export_3d_html - interactive Three.js 3D viewer
// ============================================================================
// generates a complete standalone HTML file that renders the voxel grid
// in 3D using Three.js (loaded from CDN). no build step needed - just
// open the file in a browser.
//
// the viewer shows:
//   - coloured cubes for voxels above the threshold
//   - opacity proportional to attenuation value
//   - blue spheres at sensor node positions
//   - grey wireframe outline of the room
//   - orbit controls for rotation/zoom
// ============================================================================
void Visualiser::export_3d_html(const VoxelGrid& grid,
                                 const NodeNetwork& network,
                                 const std::string& filepath,
                                 const std::string& title,
                                 double threshold) {
    // find max value for normalisation
    double max_val = 0.0;
    for (size_t i = 0; i < grid.get_total(); ++i) {
        max_val = std::max(max_val, grid.get_flat(i));
    }

    // auto-threshold: 10% of peak if not specified
    double thresh = (threshold > 0.0) ? threshold : max_val * 0.1;

    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "error: could not open " << filepath << std::endl;
        return;
    }

    file << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>)" << title << R"(</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { background: #111; overflow: hidden; font-family: monospace; }
  #info {
    position: absolute; top: 15px; left: 15px; color: #aaa;
    font-size: 12px; z-index: 10; line-height: 1.6;
  }
  #info h1 { font-size: 14px; color: #eee; margin-bottom: 5px; }
</style>
</head>
<body>
<div id="info">
  <h1>)" << title << R"(</h1>
  grid: )" << grid.get_nx() << "x" << grid.get_ny() << "x" << grid.get_nz() << R"(<br>
  voxel size: )" << grid.get_voxel_size() << R"(m<br>
  drag to rotate, scroll to zoom
</div>

<script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js"></script>
<script>
// scene setup
const scene = new THREE.Scene();
scene.background = new THREE.Color(0x111111);

const camera = new THREE.PerspectiveCamera(50, window.innerWidth / window.innerHeight, 0.1, 100);
camera.position.set()" << grid.get_room_x() * 1.8 << ", "
                       << grid.get_room_z() * 1.5 << ", "
                       << grid.get_room_y() * 1.8 << R"();

const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.setPixelRatio(window.devicePixelRatio);
document.body.appendChild(renderer.domElement);

const controls = new THREE.OrbitControls(camera, renderer.domElement);
controls.target.set()" << grid.get_room_x() / 2 << ", "
                       << grid.get_room_z() / 2 << ", "
                       << grid.get_room_y() / 2 << R"();
controls.enableDamping = true;
controls.dampingFactor = 0.05;

// lighting
scene.add(new THREE.AmbientLight(0x404040, 0.6));
const dirLight = new THREE.DirectionalLight(0xffffff, 0.8);
dirLight.position.set(5, 10, 5);
scene.add(dirLight);

// room wireframe
const roomGeo = new THREE.BoxGeometry()" << grid.get_room_x() << ", "
                                          << grid.get_room_z() << ", "
                                          << grid.get_room_y() << R"();
const roomEdges = new THREE.EdgesGeometry(roomGeo);
const roomLine = new THREE.LineSegments(roomEdges, new THREE.LineBasicMaterial({ color: 0x333333 }));
roomLine.position.set()" << grid.get_room_x() / 2 << ", "
                          << grid.get_room_z() / 2 << ", "
                          << grid.get_room_y() / 2 << R"();
scene.add(roomLine);

// grid helper on floor
const gridHelper = new THREE.GridHelper()" << std::max(grid.get_room_x(), grid.get_room_y()) << ", "
                                            << std::max(grid.get_nx(), grid.get_ny()) << R"(, 0x222222, 0x1a1a1a);
gridHelper.position.set()" << grid.get_room_x() / 2 << ", 0, "
                            << grid.get_room_y() / 2 << R"();
scene.add(gridHelper);

// voxel data
const vs = )" << grid.get_voxel_size() << R"(;
const voxelGeo = new THREE.BoxGeometry(vs * 0.9, vs * 0.9, vs * 0.9);

)";

    // write voxel data
    for (size_t idx = 0; idx < grid.get_total(); ++idx) {
        double val = grid.get_flat(idx);
        if (val <= thresh) continue;

        int i, j, k;
        grid.to_3d_index(idx, i, j, k);
        double cx, cy, cz;
        grid.get_voxel_centre(i, j, k, cx, cy, cz);

        double norm = (max_val > 0.0) ? val / max_val : 0.0;
        Colour c = heat_map(norm);

        // opacity: scale from 0.3 (low) to 0.95 (high)
        double opacity = 0.3 + 0.65 * norm;

        file << "{\n"
             << "  const mat = new THREE.MeshPhongMaterial({"
             << "color: new THREE.Color(" << c.r / 255.0 << "," << c.g / 255.0 << "," << c.b / 255.0 << "),"
             << "transparent: true, opacity: " << std::fixed << std::setprecision(2) << opacity
             << ", depthWrite: false});\n"
             << "  const mesh = new THREE.Mesh(voxelGeo, mat);\n"
             << "  mesh.position.set(" << cx << "," << cz << "," << cy << ");\n"
             << "  scene.add(mesh);\n"
             << "}\n";
    }

    // write sensor nodes as blue spheres
    file << "\n// sensor nodes\n";
    file << "const nodeMat = new THREE.MeshPhongMaterial({color: 0x4488ff, emissive: 0x224488});\n";
    file << "const nodeGeo = new THREE.SphereGeometry(0.05, 16, 16);\n";
    for (const auto& node : network.get_nodes()) {
        file << "{\n"
             << "  const mesh = new THREE.Mesh(nodeGeo, nodeMat);\n"
             << "  mesh.position.set(" << node.x << "," << node.z << "," << node.y << ");\n"
             << "  scene.add(mesh);\n"
             << "}\n";
    }

    // write link lines
    file << "\n// radio links\n";
    file << "const linkMat = new THREE.LineBasicMaterial({color: 0x224466, transparent: true, opacity: 0.15});\n";
    for (const auto& link : network.get_links()) {
        const auto& a = network.get_node(link.node_a);
        const auto& b = network.get_node(link.node_b);
        file << "{\n"
             << "  const geo = new THREE.BufferGeometry().setFromPoints(["
             << "new THREE.Vector3(" << a.x << "," << a.z << "," << a.y << "),"
             << "new THREE.Vector3(" << b.x << "," << b.z << "," << b.y << ")]);\n"
             << "  scene.add(new THREE.Line(geo, linkMat));\n"
             << "}\n";
    }

    // animation loop and resize handler
    file << R"(
// animation loop
function animate() {
  requestAnimationFrame(animate);
  controls.update();
  renderer.render(scene, camera);
}
animate();

// handle window resize
window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});
</script>
</body>
</html>
)";
}

// ============================================================================
// export_comparison_3d_html - side by side 3D comparison
// ============================================================================
// renders ground truth on the left and reconstruction on the right,
// separated by a gap. both share the same camera controls.
// ============================================================================
void Visualiser::export_comparison_3d_html(const VoxelGrid& ground_truth,
                                            const VoxelGrid& reconstruction,
                                            const NodeNetwork& network,
                                            const std::string& filepath,
                                            double threshold) {
    // find max across both grids
    double max_val = 0.0;
    for (size_t i = 0; i < ground_truth.get_total(); ++i) {
        max_val = std::max(max_val, ground_truth.get_flat(i));
    }
    for (size_t i = 0; i < reconstruction.get_total(); ++i) {
        max_val = std::max(max_val, reconstruction.get_flat(i));
    }

    double thresh = (threshold > 0.0) ? threshold : max_val * 0.1;
    double offset_x = ground_truth.get_room_x() * 1.5;  // gap between the two

    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "error: could not open " << filepath << std::endl;
        return;
    }

    file << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>RTI Reconstruction - Comparison</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { background: #111; overflow: hidden; font-family: monospace; }
  #info {
    position: absolute; top: 15px; left: 15px; color: #aaa;
    font-size: 12px; z-index: 10; line-height: 1.6;
  }
  #info h1 { font-size: 14px; color: #eee; margin-bottom: 5px; }
  .label {
    position: absolute; bottom: 20px; color: #666;
    font-size: 13px; z-index: 10; text-transform: uppercase;
    letter-spacing: 2px;
  }
  #label-left { left: 25%; transform: translateX(-50%); }
  #label-right { right: 25%; transform: translateX(50%); }
</style>
</head>
<body>
<div id="info">
  <h1>RTI 3D Reconstruction - Ground Truth vs Reconstruction</h1>
  drag to rotate, scroll to zoom
</div>
<div class="label" id="label-left">ground truth</div>
<div class="label" id="label-right">reconstruction</div>

<script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js"></script>
<script>
const scene = new THREE.Scene();
scene.background = new THREE.Color(0x111111);

const midX = )" << offset_x / 2 << R"(;
const camera = new THREE.PerspectiveCamera(50, window.innerWidth / window.innerHeight, 0.1, 100);
camera.position.set(midX + 3, 3, 5);

const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.setPixelRatio(window.devicePixelRatio);
document.body.appendChild(renderer.domElement);

const controls = new THREE.OrbitControls(camera, renderer.domElement);
controls.target.set(midX, )" << ground_truth.get_room_z() / 2 << ", "
                               << ground_truth.get_room_y() / 2 << R"();
controls.enableDamping = true;

scene.add(new THREE.AmbientLight(0x404040, 0.6));
const dirLight = new THREE.DirectionalLight(0xffffff, 0.8);
dirLight.position.set(5, 10, 5);
scene.add(dirLight);

const vs = )" << ground_truth.get_voxel_size() << R"(;
const voxelGeo = new THREE.BoxGeometry(vs * 0.9, vs * 0.9, vs * 0.9);
const nodeGeo = new THREE.SphereGeometry(0.05, 16, 16);
const nodeMat = new THREE.MeshPhongMaterial({color: 0x4488ff, emissive: 0x224488});
const linkMat = new THREE.LineBasicMaterial({color: 0x224466, transparent: true, opacity: 0.15});

)";

    // helper lambda to write voxels for a grid with an x offset
    auto write_voxels = [&](const VoxelGrid& g, double x_off, const std::string& label) {
        file << "// " << label << "\n";

        // room wireframe
        file << "{\n"
             << "  const geo = new THREE.BoxGeometry(" << g.get_room_x() << ","
             << g.get_room_z() << "," << g.get_room_y() << ");\n"
             << "  const edges = new THREE.EdgesGeometry(geo);\n"
             << "  const line = new THREE.LineSegments(edges, new THREE.LineBasicMaterial({color: 0x333333}));\n"
             << "  line.position.set(" << x_off + g.get_room_x() / 2 << ","
             << g.get_room_z() / 2 << "," << g.get_room_y() / 2 << ");\n"
             << "  scene.add(line);\n}\n";

        // voxels
        for (size_t idx = 0; idx < g.get_total(); ++idx) {
            double val = g.get_flat(idx);
            if (val <= thresh) continue;

            int i, j, k;
            g.to_3d_index(idx, i, j, k);
            double cx, cy, cz;
            g.get_voxel_centre(i, j, k, cx, cy, cz);

            double norm = (max_val > 0.0) ? val / max_val : 0.0;
            Colour c = heat_map(norm);
            double opacity = 0.3 + 0.65 * norm;

            file << "{\n"
                 << "  const m = new THREE.MeshPhongMaterial({"
                 << "color:new THREE.Color(" << c.r / 255.0 << "," << c.g / 255.0 << "," << c.b / 255.0 << "),"
                 << "transparent:true,opacity:" << std::fixed << std::setprecision(2) << opacity
                 << ",depthWrite:false});\n"
                 << "  const mesh = new THREE.Mesh(voxelGeo,m);\n"
                 << "  mesh.position.set(" << x_off + cx << "," << cz << "," << cy << ");\n"
                 << "  scene.add(mesh);\n}\n";
        }

        // nodes
        for (const auto& node : network.get_nodes()) {
            file << "{\n"
                 << "  const m = new THREE.Mesh(nodeGeo, nodeMat);\n"
                 << "  m.position.set(" << x_off + node.x << "," << node.z << "," << node.y << ");\n"
                 << "  scene.add(m);\n}\n";
        }

        // links
        for (const auto& link : network.get_links()) {
            const auto& a = network.get_node(link.node_a);
            const auto& b = network.get_node(link.node_b);
            file << "{\n"
                 << "  const g=new THREE.BufferGeometry().setFromPoints(["
                 << "new THREE.Vector3(" << x_off + a.x << "," << a.z << "," << a.y << "),"
                 << "new THREE.Vector3(" << x_off + b.x << "," << b.z << "," << b.y << ")]);\n"
                 << "  scene.add(new THREE.Line(g,linkMat));\n}\n";
        }
    };

    write_voxels(ground_truth, 0.0, "ground truth");
    write_voxels(reconstruction, offset_x, "reconstruction");

    file << R"(
function animate() {
  requestAnimationFrame(animate);
  controls.update();
  renderer.render(scene, camera);
}
animate();

window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});
</script>
</body>
</html>
)";
}
