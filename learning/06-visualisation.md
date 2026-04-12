# 06-visualisation: c++ vectors to 3d clouds

we have a solved $\mathbf{x}$ vector from the Tikhonov Regularisation engine. it is a $27000 \times 1$ column vector representing a $30 \times 30 \times 30$ room.

but you cannot show admissions tutors a `std::vector<float>`, so we need a visualisation pipeline. the goal is to transform this raw 1D array back into an interpretable 3D space, or at the very least, a 2D slice image (like a CT scan).

## mapping 1d results to 2d image pixels

the easiest way to check if the maths worked is a "top-down slice" (an XY plane) of the room.
an image is just a 2D grid of pixels (R, G, B values from 0-255).

1.  **select a slice:** we lock $z = \text{height of human chest}$, and iterate through all $x$ and $y$ indices.
2.  **inverse mapping:** for each $(x,y,z)$, we calculate its 1D index $i = x + yN_x + zN_xN_y$ (from `01-voxel-maths.md`).
3.  **retrieve value:** we pull `x_vector[i]`.
4.  **normalisation & colour mapping:** the solver outputs floating-point attenuation values (e.g. $-3.1$ to $45.2$). pixels need integers $0-255$.
    *   find the minimum and maximum values in the entire $\mathbf{x}$ vector.
    *   normalise `x[i]` to a value between $0.0$ and $1.0$: $\frac{x_i - \min}{\max - \min}$.
    *   multiply by 255.
    *   assign high values to red, and low values to blue (the heatmap).

you can export this data into a `.ppm` (Portable Pixmap Format) file using direct C++ `std::ofstream`. it writes the RGB bytes in pure text.

## the 3d scatter plot

for a proper Imperial EIE presentation, a static 2D slice is not enough. we need true volumetric rendering. 

writing a native OpenGL/Vulkan C++ renderer for voxels is a massive project in itself. instead, i kept the C++ core decoupled and built a web-based visualiser (the Vercel app).

### exporting point clouds
a point cloud is just a list of 3D coordinates.

in C++, i iterate through the entire $27,000$-element $\mathbf{x}$ vector. if a voxel's attenuation threshold is above a certain value (e.g., higher than $0.2$, meaning "not empty air"), i run the reverse index mapping formula (from `01-voxel-maths.md`) to extract its physical $(x,y,z)$ coordinates.

for every voxel over the threshold, i print a JSON representation:

```json
[
  {"x": 1.5, "y": 2.1, "z": 0.8, "value": 0.94},
  {"x": 1.6, "y": 2.1, "z": 0.8, "value": 0.82}
]
```

this decoupled JSON payload is read by a JavaScript/Three.js engine, which plots them as semi-transparent red cubes. this completely separates the heavy C++ linear algebra from the UI rendering stack, which is a classic software engineering architecture pattern to highlight in an interview.