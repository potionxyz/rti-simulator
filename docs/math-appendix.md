# Mathematical Appendix

## 1. the forward model

### signal attenuation

when a radio signal passes through a material, it loses energy. the amount of loss depends on the material's attenuation coefficient (measured in dB/m) and the distance the signal travels through it.

for a single link between nodes a and b:

```
attenuation = integral from a to b of alpha(r) dr
```

where alpha(r) is the attenuation coefficient at position r along the path.

### discretisation

we divide the continuous space into discrete voxels. each voxel j has a constant attenuation value x_j (dB/m). the integral becomes a sum:

```
y_i = sum_j (w_ij * x_j)
```

where:
- y_i is the measured attenuation on link i (dB)
- w_ij is the length of link i that passes through voxel j (metres)
- x_j is the attenuation coefficient of voxel j (dB/m)

### matrix form

stacking all links into a vector:

```
y = W * x
```

where:
- y is the measurement vector (M x 1), M = number of links
- W is the weight matrix (M x N), N = number of voxels
- x is the attenuation image (N x 1)

## 2. weight matrix construction (Siddon's algorithm)

### parametric ray

a ray from point P1 = (x1, y1, z1) to P2 = (x2, y2, z2) can be written parametrically:

```
P(t) = P1 + t * (P2 - P1),  t in [0, 1]
```

at t=0 we're at P1, at t=1 we're at P2.

### grid plane crossings

the voxel grid has planes at:
- x = i * vs for i = 0, 1, ..., nx
- y = j * vs for j = 0, 1, ..., ny
- z = k * vs for k = 0, 1, ..., nz

where vs is the voxel size.

the ray crosses the x-plane at x = i * vs when:

```
t_x(i) = (i * vs - x1) / (x2 - x1)
```

similarly for y and z planes.

### intersection lengths

after collecting all t-values where the ray crosses any grid plane, we sort them. each consecutive pair [t_n, t_{n+1}] represents the ray being inside one specific voxel. the intersection length is:

```
length = (t_{n+1} - t_n) * ||P2 - P1||
```

this length becomes the weight w_ij for that link-voxel pair.

## 3. the inverse problem

### why it's hard

we have M = 28 measurements but N = 1000+ unknowns. the system is heavily underdetermined (M << N). infinitely many x vectors could produce the same measurements y.

additionally, W^T W has rank at most M = 28, but size N x N = 1000 x 1000. this means it's singular and cannot be inverted directly.

### back-projection

the simplest approach: "spray" each measurement back along its link path.

```
x_bp = W^T * y
```

this is not a true inverse but an adjoint operation. it produces a blurry image where voxels traversed by more attenuated links get higher values. the result is useful as a first approximation.

### least-squares

minimise the squared error between predicted and actual measurements:

```
min_x ||Wx - y||^2
```

taking the derivative and setting to zero:

```
d/dx ||Wx - y||^2 = 2 W^T (Wx - y) = 0
W^T W x = W^T y
```

this requires inverting W^T W, which is singular. the pseudoinverse gives a minimum-norm solution, but it amplifies noise because the small eigenvalues of W^T W create large coefficients in the inverse.

### Tikhonov regularisation

add a penalty on the solution magnitude:

```
min_x ||Wx - y||^2 + lambda * ||x||^2
```

the first term ensures data fidelity (the reconstruction should match the measurements). the second term ensures stability (the reconstruction shouldn't blow up).

taking the derivative:

```
2 W^T (Wx - y) + 2 lambda x = 0
(W^T W + lambda I) x = W^T y
x = (W^T W + lambda I)^{-1} W^T y
```

adding lambda * I to W^T W shifts all eigenvalues by lambda, making the matrix positive definite and invertible. small eigenvalues that would cause numerical instability are now bounded below by lambda.

### lambda selection

- lambda -> 0: approaches least-squares, noisy and unstable
- lambda -> infinity: x -> 0, over-smoothed to nothing
- optimal lambda: balances data fit and solution stability

in practice, lambda = 1.0 works well for this problem. more rigorous methods (L-curve, generalised cross-validation) exist but are beyond the scope of this project.

## 4. evaluation metrics

### mean squared error (MSE)

```
MSE = (1/N) * sum_j (x_hat_j - x_true_j)^2
```

measures overall reconstruction accuracy. lower is better, 0 is perfect.

### localisation error

```
error = ||centroid(x_hat) - centroid(x_true)||
```

where the centroid is the attenuation-weighted centre of mass. measures how far the detected peak is from the true object position.

### detection statistics

using a threshold to binarise the reconstruction:
- **true positive (TP)**: voxel is occupied AND detected
- **false positive (FP)**: voxel is empty BUT detected
- **false negative (FN)**: voxel is occupied BUT missed
- **true negative (TN)**: voxel is empty AND not detected

## 5. physical constants

| material | attenuation at 2.4GHz (dB/m) |
|---|---|
| air | ~0 |
| wood | 2-3 |
| human body | 3-5 |
| brick | 5-8 |
| concrete | 10-15 |
| metal | 30+ |

human body attenuation is primarily due to water content (~60% of body mass). water has a high dielectric constant at 2.4GHz, causing significant RF absorption.
