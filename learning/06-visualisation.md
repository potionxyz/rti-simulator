# 06-visualisation: c++ vectors to 3d browser viewer

at the end of the pipeline we have a solved $\hat{\mathbf{x}}$ vector from the Tikhonov engine. it is a `std::vector<double>` (or more accurately, an `Eigen::VectorXd`) representing a 10×10×10 room. 1000 floating-point numbers. completely unreadable on its own.

this module covers how those 1000 numbers get turned into three different human-readable outputs, and why the viewer is a decoupled web frontend instead of native C++ graphics.

---

## three output formats

the `Visualiser` class exposes three output tiers, each useful at a different stage of debugging.

| format | use when | how |
| --- | --- | --- |
| ASCII slice | fastest sanity check while iterating on the solver | `print_slice_z(grid, k)` → stdout |
| PPM image | you want a static 2D heatmap to drop into a report | `export_slice_ppm(grid, k, "out.ppm")` |
| Three.js HTML | full 3D interactive view for presentation / writeup | `export_3d_html(grid, network, "out.html", "title")` |

### 1. ASCII slice (the debugging tier)

the fastest way to check if the solver is finding anything at all is to print a 2D slice directly to the terminal. `print_slice_z(grid, k)` walks a fixed z-plane of the voxel grid and maps each value to one of: `space`, `.`, `-`, `%`, `@`. higher attenuation gets the denser character.

it looks like this (real output from the project):

```
z=5 (height=1.1m):
+----------+
|          |
|          |
|          |
|          |
|    %@    |
|    %%    |
|          |
|          |
|          |
|          |
+----------+
```

that's the reconstructed image of a person standing dead-centre in the 2×2×2m room, sliced at mid-height. the two `@` characters mean "peak attenuation", the two `%` mean "roughly 60% of peak". no GPU required.

### 2. PPM heatmap (the report tier)

PPM (Portable Pixmap Format) is the simplest image format in existence. the file is literally:

```
P3
10 10
255
0 0 0  0 0 0  0 0 0  ...
```

one magic number, the width and height, the max colour value, then the RGB triplets in plain text. you can write it with `std::ofstream` and a handful of `<<` operators. no image library, no PNG encoder, no zlib.

the mapping from attenuation to pixel colour is just a linear normalisation:

1. find `min` and `max` over the full `Eigen::VectorXd` (or the slice, depending on the call)
2. for each voxel in the slice, compute `t = (value - min) / (max - min)`
3. map `t ∈ [0,1]` to a heatmap: low → blue, mid → green, high → red
4. clamp to `[0, 255]` and write the three bytes

the result is a 10×10 pixel heatmap per slice. tiny, but perfectly legible when viewed at reasonable zoom. the real power is in `export_comparison_ppm()`, which writes ground truth and reconstruction side-by-side in the same PPM so you can eyeball-diff them in one glance.

### 3. three.js HTML (the presentation tier)

the PPM is fine for a static slice, but RTI is fundamentally volumetric — a single slice doesn't do it justice. for the real 3D view, `export_3d_html` writes a fully self-contained HTML file.

"self-contained" is the critical word. the file loads the Three.js library from a CDN, but **all of the voxel data is baked directly into the HTML as inline JavaScript**. no JSON fetching, no backend, no CORS. you can email the file or drop it on any static host and it just works.

the generated HTML does four things on load:

1. creates a Three.js `Scene` with an ambient + directional light and grey wireframe room outline matching the voxel grid dimensions
2. loops over the inlined voxel data, and for every voxel above the threshold (default: 10% of peak) adds a `BoxGeometry` cube at the right position, coloured and opacity-scaled from the attenuation value
3. adds blue spheres at each ESP32 node position and grey lines connecting every pair (so you can see the link network)
4. attaches `OrbitControls` so you can click-drag to rotate and scroll to zoom

the whole thing is about 30KB per file. here's the skeleton structure (heavily simplified from `src/visualiser.cpp`):

```cpp
void Visualiser::export_3d_html(const VoxelGrid& grid,
                                 const NodeNetwork& network,
                                 const std::string& filename,
                                 const std::string& title,
                                 double threshold) {
    std::ofstream f(filename);

    // find peak for auto-threshold
    double max_val = 0.0;
    for (int i = 0; i < grid.get_total(); ++i)
        max_val = std::max(max_val, grid.at_index(i));
    double thresh = (threshold > 0.0) ? threshold : max_val * 0.1;

    // write html header + three.js cdn imports
    f << "<!DOCTYPE html><html><head><title>" << title << "</title>..."
      << "<script src=\"https://cdnjs.../three.min.js\"></script>"
      << "<script src=\"https://cdn.jsdelivr.net/.../OrbitControls.js\"></script>"
      << "<script>";

    // build scene, camera, renderer, controls
    f << "const scene = new THREE.Scene();\n"
      << "const camera = new THREE.PerspectiveCamera(...);\n"
      << "const renderer = new THREE.WebGLRenderer({antialias:true});\n"
      << "const controls = new THREE.OrbitControls(camera, renderer.domElement);\n";

    // emit wireframe room outline
    f << "const room = new THREE.EdgesGeometry(new THREE.BoxGeometry("
      << grid.get_room_x() << "," << grid.get_room_y() << "," << grid.get_room_z()
      << "));\nscene.add(new THREE.LineSegments(room, ...));\n";

    // inline every above-threshold voxel as a BoxGeometry cube
    f << "const voxelGeo = new THREE.BoxGeometry(vs*0.9, vs*0.9, vs*0.9);\n";
    for (int k = 0; k < grid.get_nz(); ++k) {
        for (int j = 0; j < grid.get_ny(); ++j) {
            for (int i = 0; i < grid.get_nx(); ++i) {
                double val = grid.at(i, j, k);
                if (val <= thresh) continue;
                auto col = attenuation_to_rgb(val, max_val);
                double opacity = std::min(1.0, val / max_val);
                f << "{const m = new THREE.MeshPhongMaterial({"
                  << "color: new THREE.Color("
                  << col.r/255.0 << "," << col.g/255.0 << "," << col.b/255.0
                  << "),transparent:true,opacity:" << opacity << "});\n"
                  << "const mesh = new THREE.Mesh(voxelGeo, m);\n"
                  << "mesh.position.set(" << x_phys << "," << y_phys << "," << z_phys << ");\n"
                  << "scene.add(mesh);}\n";
            }
        }
    }

    // emit node spheres + link lines for the ESP32 network
    for (const auto& node : network.nodes()) {
        f << "{const s = new THREE.SphereGeometry(0.05);...}\n";
    }

    // animation loop
    f << "function animate(){requestAnimationFrame(animate);"
      << "controls.update();renderer.render(scene,camera);}animate();";

    f << "</script></head><body></body></html>";
}
```

the whole export is under 650 lines of C++ in `visualiser.cpp`, and it cost me literally zero runtime dependencies. that matters. if i had pulled in a native renderer I'd be dragging OpenGL headers, GLFW, GLEW, cross-platform build scripts and a shader pipeline into a project that is fundamentally about linear algebra.

---

## why decouple the viewer at all?

the classic beginner move is to write the renderer directly inside the solver class. `Reconstructor::draw()` calls into OpenGL, allocates VBOs, manages shaders, handles window resize events. six weeks later you can't test the solver without a graphics card and the class has grown to 2000 lines.

i explicitly did not do this. the C++ math core has no knowledge of rendering. it exports flat data, and a separate frontend visualises it. the benefits:

1. **testability.** i can run the whole reconstruction pipeline on a headless server with zero display. the test suite doesn't need a GPU.
2. **portability.** the HTML file runs on any browser — laptop, phone, Chromebook, uni library terminal. no install, no permissions.
3. **separation of concerns.** if i ever want to swap Three.js for Plotly or Babylon.js, i only change `visualiser.cpp`. the solver doesn't even notice.
4. **code size.** my visualiser is ~650 lines because it just emits text. a native renderer would easily be 3000+ lines with a shader pipeline.

this is the classic **presentation/logic separation** pattern from software engineering. the maths engine produces pure data, and the view consumes it. if you're asked about architectural patterns in an interview, this is the exact example to reach for.

---

## the rendering pipeline (data → pixels)

for completeness, here is the full chain of transformations a single voxel's attenuation value goes through before you see it on screen:

```
Eigen::VectorXd x              (solver output, 1000 doubles)
      ↓ apply_to_grid()
VoxelGrid                       (same 1000 values, 3D indexed)
      ↓ export_3d_html loop
{position (x,y,z), value}       (above-threshold voxels only)
      ↓ attenuation_to_rgb()
{position, rgb, opacity}        (baked into HTML as JS literals)
      ↓ browser parse
THREE.Mesh                      (BoxGeometry + MeshPhongMaterial)
      ↓ WebGL render
fragment shader                 (per-pixel lighting)
      ↓ gl.drawElements
pixels on screen                (what you actually see)
```

the first three steps happen in C++ at export time (once, when you run the simulator). the last four happen in the browser at view time (continuously, as you rotate the camera). no server in the loop, no roundtrip, no state.

---

## viewer output files

when `main.cpp` finishes, these are the files sitting in `build/`:

- `output_truth_3d.html` — ground truth scene (what the forward model was fed)
- `output_recon_3d.html` — reconstructed estimate (what Tikhonov produced)
- `output_comparison_3d.html` — both in a side-by-side split view
- `output_two_people_3d.html` — the two-person test scene, both truth and reconstruction
- `output_truth_z5.ppm` / `output_recon_z5.ppm` / `output_comparison_z5.ppm` — PPM slices at mid-height

these all get committed to a `viewer/` directory in the repo so the vercel deployment can pick them up (see `portfolio/04-testing-and-results.md` for the deployment story).

this is the full end-to-end simulator. next up in the syllabus: the critical evaluation folder — where the whole architecture gets torn apart.
