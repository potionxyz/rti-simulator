# Testing

## 1. test plan

testing is structured in three layers:

1. **unit tests**: verify individual components (voxel grid indexing, node network generation, etc.)
2. **integration tests**: verify the full pipeline (scene -> forward model -> reconstruction -> evaluation)
3. **parameter sweep tests**: explore how system performance varies with configuration changes

## 2. unit test results

### 2.1 voxel grid

| test | input | expected | actual | pass |
|---|---|---|---|---|
| grid dimensions | room 2x2x2, voxel 0.1m | 20x20x20 = 8000 voxels | 20x20x20 = 8000 | ✓ |
| grid dimensions | room 2x2x2, voxel 0.2m | 10x10x10 = 1000 voxels | 10x10x10 = 1000 | ✓ |
| flat index (0,0,0) | nx=20 | 0 | 0 | ✓ |
| flat index (19,19,19) | nx=20, ny=20 | 7999 | 7999 | ✓ |
| round-trip 3D->flat->3D | (5, 10, 15) | (5, 10, 15) | (5, 10, 15) | ✓ |
| voxel centre | (0,0,0) at 0.1m | (0.05, 0.05, 0.05) | (0.05, 0.05, 0.05) | ✓ |
| out of bounds | (-1, 0, 0) | exception thrown | exception thrown | ✓ |
| reset | set values, then reset | all zeros | all zeros | ✓ |

### 2.2 node network

| test | input | expected | actual | pass |
|---|---|---|---|---|
| 2D square nodes | 2x2 room | 4 nodes, 6 links | 4 nodes, 6 links | ✓ |
| 3D cube nodes | 2x2x2 room | 8 nodes, 28 links | 8 nodes, 28 links | ✓ |
| link formula | N=8 -> N*(N-1)/2 | 28 | 28 | ✓ |
| node positions | cube corners | all at (0 or 2) coords | correct | ✓ |
| node lookup | get_node(7) | (2, 2, 2) | (2, 2, 2) | ✓ |
| invalid node | get_node(99) | exception | exception | ✓ |

### 2.3 scene builder

| test | input | expected | actual | pass |
|---|---|---|---|---|
| empty scene | no obstacles | 0 occupied voxels | 0 | ✓ |
| person centre | person at (1,1) | ~32 voxels (at 0.2m) | 32 | ✓ |
| person dimensions | 0.4x0.3x1.7m | bbox (0.8-1.2, 0.85-1.15, 0-1.7) | correct | ✓ |
| JSON round-trip | save then load | identical scene | identical | ✓ |
| overlapping obstacles | two objects, same voxel | max attenuation wins | correct | ✓ |

### 2.4 forward model

| test | input | expected | actual | pass |
|---|---|---|---|---|
| empty room | no obstacles | all measurements = 0 | all 0 | ✓ |
| weight matrix size | 8 nodes, 1000 voxels | 28 x 1000 | 28 x 1000 | ✓ |
| weight matrix sparsity | 28 links | ~2% non-zero | 2% | ✓ |
| link 0-7 attenuation | person centre | 2.078 dB (hand calc) | 2.078 dB | ✓ |
| symmetry | link 0-7 = link 7-0 | same attenuation | same | ✓ |
| noise mean | 1000 samples, 1dB noise | mean ≈ 0 | ≈ 0 | ✓ |

### 2.5 reconstruction

| test | input | expected | actual | pass |
|---|---|---|---|---|
| empty room | y = 0 | x = 0 everywhere | all 0 | ✓ |
| back-proj non-negative | any input | no negative values | all >= 0 | ✓ |
| Tikhonov non-negative | any input | no negative values | all >= 0 | ✓ |
| peak location | person at centre | peak near (1, 1) | within 0.1m | ✓ |
| lambda=0 vs lambda=10 | same measurements | smaller lambda -> higher peak | confirmed | ✓ |

## 3. integration test results

### 3.1 single person centre (clean measurements)

| method | localisation error | MSE | true positives | false positives |
|---|---|---|---|---|
| back-projection | 0.025m | 0.620 | 12 | 44 |
| Tikhonov λ=0.01 | 0.101m | 0.931 | 12 | 44 |
| Tikhonov λ=0.1 | 0.093m | 0.880 | 12 | 44 |
| Tikhonov λ=1.0 | 0.061m | 0.691 | 12 | 44 |
| Tikhonov λ=10.0 | 0.032m | 0.628 | 12 | 44 |

best localisation: back-projection (0.025m) and Tikhonov λ=10 (0.032m)

### 3.2 noise sensitivity

| noise (dB) | localisation error | true positives | false positives |
|---|---|---|---|
| 0.0 | 0.061m | 12 | 44 |
| 0.5 | 0.014-0.049m | 12 | 44 |
| 1.0 | 0.045-0.189m | 6-10 | 24-38 |
| 2.0 | 0.213-0.430m | 0-4 | 5-26 |

noise degrades gracefully up to ~1dB, then detection falls off significantly.

### 3.3 scene comparison

| scene | localisation error | links affected | notes |
|---|---|---|---|
| person centre | 0.061m | 6/28 | best case, many diagonal links cross centre |
| person corner | 0.659m | few | worst case, corner has minimal link coverage |
| two people | 0.114m | 3/28 | centroid-based error, both detected |
| person + wall | 1.972m | many | wall dominates, person masked |

## 4. key findings

1. **centre is easy, corners are hard**: the 8-node cube configuration provides excellent coverage of the centre (space diagonals all cross there) but poor coverage of corners. this is a known limitation of the cube geometry.

2. **Tikhonov is better than back-projection for noisy data**: while back-projection has slightly better localisation in clean conditions, Tikhonov is more robust to noise because the regularisation suppresses noise amplification.

3. **lambda = 1.0 is a good default**: across all test scenes, lambda = 1.0 provides the best balance between localisation accuracy and noise suppression.

4. **through-wall detection needs more nodes**: the concrete wall (12 dB/m) overwhelms the person's signature (4 dB/m). more nodes or CSI-based measurements would help.

5. **resolution is limited by link count**: with only 28 links, we can't resolve fine spatial details. the effective resolution is roughly room_size / sqrt(num_links) ≈ 0.38m, which matches the observed performance.
