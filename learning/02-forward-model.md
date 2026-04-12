# 02-forward-model: simulating rf propagation

so we have our room discretised into a 1D vector of voxels. but if we're building a simulator, we actually have to simulate the physics of radio waves travelling through that space. 

this is the forward model. we know the room layout (the ground truth voxels $\mathbf{x}$), and we want to predict the signal strength measurements ($\mathbf{y}$) between any two ESP32 transceivers (nodes).

## the physics of attenuation

when a 2.4GHz Wi-Fi signal travels from node $A$ to node $B$, it experiences path loss. in a vacuum, signal power drops quadratically with distance (the inverse-square law). but in RTI, we care about the *excess* attenuation caused by physical objects—walls, furniture, human bodies—absorbing or scattering the RF energy.

the total signal loss is the line integral of the attenuation field along the line of sight (LOS) path.

mathematically, the change in received signal strength (RSS), which we will call $y$, for a single link $i$ is:

$$ y_i = \int_{L_i} a(r) \, dr + n_i $$

where:
* $L_i$ is the line path from transmitter to receiver.
* $a(r)$ is the continuous spatial attenuation field at point $r$.
* $n_i$ is a Gaussian noise term accounting for hardware imperfections and multipath fading.

but remember from exactly the last document: we don't have a continuous field $a(r)$. we discretised space into $J$ voxels. so the integral becomes a discrete summation over every voxel $j$:

$$ y_i = \sum_{j=1}^{J} W_{ij} x_j + n_i $$

* $x_j$ is the constant attenuation inside voxel $j$.
* $W_{ij}$ is the exact physical distance (the chord length) that the signal travels *inside* voxel $j$ on its path to the receiver.

if the signal ray completely misses voxel $j$, then the distance $W_{ij}$ is simply 0. for a room with 27,000 voxels, a single line-of-sight ray only passes through maybe 50 of them. the remaining 26,950 have a weight of 0.

## siddon's algorithm: solving the intersection

so the entire physics engine hinges on calculating $W_{ij}$. given a 3D ray from $A(x_1, y_1, z_1)$ to $B(x_2, y_2, z_2)$, which voxels does it pierce, and how long is the exact path through each one?

you could use an iterative step-by-step ray march, advancing $1\text{mm}$ at a time and checking which voxel you are currently inside. but this is terribly inefficient computationally. 

instead, we use **siddon's exact ray-tracing algorithm**, heavily used in medical CT image reconstruction (a different type of tomography).

siddon's algorithm leverages parametric line equations. instead of stepping blindly, it calculates the exact intersection points where the ray crosses the physical grid planes that separate the voxels. 

### the parametric line

we define the path from $A$ to $B$ using a parameter $\alpha$ ranging from $0$ (at point $A$) to $1$ (at point $B$). any point $(x,y,z)$ on the ray is given by:

$$ x(\alpha) = x_1 + \alpha(x_2 - x_1) $$
$$ y(\alpha) = y_1 + \alpha(y_2 - y_1) $$
$$ z(\alpha) = z_1 + \alpha(z_2 - z_1) $$

the distance between any two points along this line is just the difference in their $\alpha$ values, scaled by the total distance $d_{AB}$ between the nodes:

$$ \text{distance} = (\alpha_{current} - \alpha_{previous}) \times d_{AB} $$

### how siddon's works

1. **find all grid plane intersections:** the algorithm computes every $\alpha$ value where the ray pierces an X-plane, a Y-plane, and a Z-plane.
2. **merge and sort:** it takes all these $\alpha$ values, merges them into a single list, and sorts them from smallest ($0.0$) to largest ($1.0$).
3. **calculate lengths:** the distance between consecutive $\alpha$ values in the sorted list represents a continuous segment of the ray entirely enclosed within a single voxel.
4. **assign voxel index:** for the midpoint of each $\alpha$ segment, it calculates the corresponding 3D physical position, converts that to the 3D voxel coordinate, and maps it to the 1D flat vector index $i$ using our linear mapping formula.

the result is a sparse array of $(i, \text{length})$ pairs for the active ray. 

### worked example (2d projection)

imagine a 2D ray shooting from $(0.5, 0.5)$ to $(2.5, 2.5)$ through a grid of $1 \times 1$ squares. total distance $d = \sqrt{2^2 + 2^2} \approx 2.828$.

* $\alpha = 0.0$ at start.
* ray hits X=1 plane at $\alpha_x = 0.25$.
* ray hits Y=1 plane at $\alpha_y = 0.25$.
* ray hits X=2 plane at $\alpha_x = 0.75$.
* ray hits Y=2 plane at $\alpha_y = 0.75$.
* $\alpha = 1.0$ at end.

sorted $\alpha$ list: $\{0.0, 0.25, 0.75, 1.0\}$ (in a true 3D case, the intersections usually won't align perfectly).

* segment 1: from $\alpha = 0.0$ to $0.25$. length = $(0.25 - 0.0) \times 2.828 = 0.707$. midpoint is in voxel (0,0). $W(0,0) = 0.707$.
* segment 2: from $\alpha = 0.25$ to $0.75$. length = $(0.75 - 0.25) \times 2.828 = 1.414$. midpoint is in voxel (1,1). $W(1,1) = 1.414$.
* segment 3: from $\alpha = 0.75$ to $1.0$. length = $(1.0 - 0.75) \times 2.828 = 0.707$. midpoint is in voxel (2,2). $W(2,2) = 0.707$.

by iterating this algorithm over every single link pair ($N \times (N-1) / 2$ links for $N$ nodes), we construct the full row elements of the weight matrix $\mathbf{W}$.

## adding noise

the final step of the forward model is empirical stochasticity. a real ESP32 RSSI measurement is incredibly noisy due to multipath scattering (signals bouncing off floors and ceilings), 2.4GHz interference from bluetooth/microwaves, and hardware thermal variance. 

if i reconstruct the perfect, theoretical $y_i$, the inverse solver will have an unrealistically easy time. to simulate physical hardware limits, i inject Gaussian white noise:

```cpp
// from src/scene.cpp
std::random_device rd;
std::mt19937 gen(rd());
// assume standard deviation of 1.5 dBm 
std::normal_distribution<float> noise(0.0, 1.5); 

float measured_rssi = theoretical_rssi + noise(gen);
```

we now have our dataset $\mathbf{y}$. the physics forward model is complete. next, we construct the giant $\mathbf{W}$ matrix fully and try to run the maths in reverse.