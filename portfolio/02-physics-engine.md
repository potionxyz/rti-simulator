# 02-the-physics-engine

once the memory architecture (the 1D voxel grid) was locked in, the next step was building the forward model. basically: if i have a virtual 3D room with a human standing in it, and i put 8 virtual ESP32 nodes around the perimeter, what signals will they actually measure?

i needed a physics engine to simulate radio wave propagation. this is entirely separate from the reconstruction maths — the solver (Reconstructor) isn't allowed to see the forward model. it only gets the final, noisy signal data to guess what the room looks like.

## the forward math

an ESP32 measures Received Signal Strength Indicator (RSSI) in dBm. when a 2.4GHz Wi-Fi signal travels from node A to node B, the power drops due to distance (inverse-square law) and physical obstructions in the line of sight (LOS).

in Radio Tomographic Imaging (RTI), we only care about the *change* in signal strength caused by obstacles. 

let $\Delta y_i$ be the change in RSSI on link $i$. this is mathematically the integral of the attenuation field $x$ along the path length $L_i$:

$$ \Delta y_i = \int_{L_i} x \, dr + n_i $$

where $n_i$ is a noise term to simulate physical hardware imperfections.

because i discretised the room into voxels, the continuous integral becomes a discrete sum spanning every voxel $j$:

$$ \Delta y_i = \sum_j W_{ij} x_j + n_i $$

where $W_{ij}$ is the physical distance the radio wave travels *through* voxel $j$ on its path from node A to node B. if the ray misses the voxel entirely, the weight is 0.

## finding the weights: siddon's algorithm

so the entire physics simulation comes down to calculating exactly which voxels a 3D line intersects, and exactly how long the line segment inside each voxel is.

the naive approach is ray-marching: stepping along the line 1mm at a time and checking the current `(x,y,z)` coordinate to see which voxel you are in. this is computationally awful. taking 3,000 steps per line across 28 lines (for an 8-node network) every single time the room updates would completely bottleneck the C++ execution speed.

instead, i implemented **Siddon's exact ray-tracing algorithm**. 

it's a classic technique from medical CT scanning. instead of stepping blindly, Siddon’s algorithm uses parametric line equations to calculate the exact intersection points where the ray crosses the physical grid planes that separate the voxels. 

1. **parametric $\alpha$:** the path from node A to node B is defined by a parameter $\alpha$ ranging from 0.0 (start) to 1.0 (end).
2. **plane intersections:** the algorithm computes every $\alpha$ value where the ray pierces an X-plane, a Y-plane, and a Z-plane in the grid.
3. **sort and merge:** it merges all these intersection points into a single list and sorts them from smallest to largest.
4. **calculate segments:** the distance between any two consecutive $\alpha$ values in the sorted list represents a segment of the ray entirely enclosed within a single voxel.
5. **map to memory:** for the midpoint of each segment, it calculates the 3D position, finds the containing voxel, maps that to a 1D index using my `x + y*Nx + z*Nx*Ny` formula from the architecture phase, and stores the segment length as the weight $W_{ij}$.

this approach calculates the exact lengths without a single wasted loop iteration. the forward model generates the system matrix $\mathbf{W}$ near-instantaneously.

## empirical noise simulation

the final step was acknowledging that real hardware is messy. if i ran the inverse solver on perfect, theoretical math, the reconstruction would be unrealistically flawless. 

in reality, 2.4GHz Wi-Fi suffers heavily from multipath fading (signals bouncing off the floor and arriving slightly out of phase), interference from bluetooth and microwaves, and thermal variance in the ESP32 silicon. 

to simulate physical limits, i inject Gaussian white noise directly into the $\Delta y$ measurement vector. i used the C++ `<random>` library with a Mersenne Twister engine (`std::mt19937`) to generate normally distributed noise:

```cpp
std::random_device rd;
std::mt19937 gen(rd());
// simulate a 1.5 dBm hardware variance 
std::normal_distribution<float> hardware_noise(0.0, 1.5); 

float measured_rssi_delta = theoretical_rssi_delta + hardware_noise(gen);
```

by forcing the solver to reconstruct the room using noisy data, i could empirically test how robust the inverse mathematics really were. if the mathematics fail when the noise hits 2 dBm, the physical ESP32 deployment will fail entirely.

with the physics engine complete and generating simulated $\mathbf{y}$ vectors, the next phase was the actual core of the project: the inverse solver.