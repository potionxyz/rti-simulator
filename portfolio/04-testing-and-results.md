# 04-testing-and-results

the hardest part of a heavy mathematical C++ simulator isn't writing the solver. it's proving the solver actually works. you can't just look at a 1000-element `Eigen::VectorXd` and know if the inverse problem found the simulated human. the numbers have to be visualised, tested against ground truth, and stress-tested against noise, multiple people, and edge cases.

so the final phase was designed as a proper test suite with five named scenes, a sweep across four lambda values, three noise levels, and a comparison between the three reconstruction methods.

## the test harness

every test run goes through the same four stages:

1. `Scene::create_...()` factory stamps a ground truth into the voxel grid
2. `forward.simulate(grid, noise_stddev)` produces $\mathbf{y}$ with optional gaussian noise
3. one of `back_projection`, `least_squares`, `tikhonov(λ)` runs the reconstruction
4. `evaluate_auto_threshold` compares against the ground truth and returns `ReconstructionResult`

the `ReconstructionResult` struct carries six metrics: the MSE across all 1000 voxels, the localisation error (euclidean distance between true and reconstructed centre of mass), the peak attenuation value, and true/false/missed voxel counts at a 30%-of-peak detection threshold.

the suite runs five scenes:

| test | scene | purpose |
| --- | --- | --- |
| 1 | single person centre, no noise | baseline correctness check |
| 2 | single person centre, noisy | noise robustness |
| 3 | two people | can the solver resolve multiple bodies? |
| 4 | person behind wall | can Tikhonov see through static obstructions? |
| 5 | person in corner | edge-case geometry stress test |

the room is $2\text{m} \times 2\text{m} \times 2\text{m}$, voxel size $0.2\text{m}$, so $N = 10 \times 10 \times 10 = 1000$ voxels. 8 ESP32 nodes in a cube perimeter = 28 unique links. the weight matrix $\mathbf{W}$ is $28 \times 1000$.

## test 1: baseline correctness (single person, no noise)

the simplest scene: a single $0.4 \times 0.4 \times 1.8$ metre "human" block at the centre, zero noise injected. all three reconstruction methods are run, and for Tikhonov i also swept $\lambda$ across four orders of magnitude.

| method | peak | loc error (m) | MSE | TP | FP | FN |
| --- | --- | --- | --- | --- | --- | --- |
| back-projection | 1.60 | 0.025 | 6.20 × 10⁻¹ | 1 | 8 | 23 |
| tikhonov λ=0.01 | 0.85 | 0.101 | 9.31 × 10⁻¹ | 10 | 25 | 14 |
| tikhonov λ=0.1 | 0.82 | 0.093 | 8.80 × 10⁻¹ | 9 | 23 | 15 |
| tikhonov λ=1.0 | 0.59 | 0.061 | 6.91 × 10⁻¹ | 6 | 14 | 18 |
| tikhonov λ=10.0 | 0.14 | 0.032 | 6.28 × 10⁻¹ | 2 | 5 | 22 |

a few things stand out.

**back-projection wins on peak localisation.** 25mm is absurdly tight for a $200\text{mm}$ voxel grid — that's 12.5% of a single voxel edge. why? because with zero noise, back-projection's spray-back is structurally simple and doesn't try to invert an ill-conditioned matrix. no mathematical panic means no artifacts.

**tikhonov gets smoother as $\lambda$ grows.** watch the peak value drop from 0.85 to 0.14 as $\lambda$ increases. that's the regularisation penalty forcing the whole image towards zero. at $\lambda = 10.0$ the image is almost erased, but the remaining peak is still in the right place.

**localisation error does not monotonically improve.** at $\lambda = 0.01$ the error is 101mm, at $\lambda = 0.1$ it's 93mm, at $\lambda = 1.0$ it drops to 61mm, and at $\lambda = 10.0$ it drops further to 32mm. but the number of true-positive voxels collapses from 10 to 2 as $\lambda$ climbs. **you're trading detection recall for localisation precision.** that's the fundamental Tikhonov trade-off made physically visible.

**the sweet spot for a noiseless scene is around $\lambda = 1.0$.** it keeps the peak value high enough to be meaningful (0.59) while pulling localisation error below 10cm. the $\lambda = 10.0$ row looks tempting for accuracy, but if you actually look at the reconstructed image, the whole thing is a dim wash — you cannot visually confirm anything is there.

## test 2: noise robustness (gaussian noise injection)

this is where Tikhonov earns its keep. i fixed $\lambda = 1.0$ and swept noise standard deviation through 0.5, 1.0 and 2.0 dB. real ESP32 hardware sits around 1.0 to 1.5 dB on a good day.

| noise σ (dB) | loc error (m) | true positives | false positives |
| --- | --- | --- | --- |
| 0.5 | 0.073 | 10 | 38 |
| 1.0 | 0.145 | 6 | 25 |
| 2.0 | 0.493 | 0 | 24 |

at 0.5 dB noise, the solver holds up: 73mm localisation error, 10 voxels correctly detected. this is the regime a real ESP32 should operate in if the hardware is clean.

at 1.0 dB, the localisation error doubles to 145mm, but the centre of mass is still physically in the right region of the room. the detector is starting to struggle — only 6 TPs, 25 FPs.

at 2.0 dB the system is broken. zero true positives. the noise is large enough that the solver completely loses the human in the FP spray. **this defines the upper hardware noise budget for the simulator: around 1.5 dB stddev.** above that, Tikhonov with fixed $\lambda = 1.0$ cannot suppress the noise without also suppressing the signal.

the fix on real hardware would be adaptive $\lambda$ — increasing the penalty as noise grows — or channel-hopping across the 13 Wi-Fi channels and averaging to reduce the effective noise floor. both of those are future work once the ESP32 deployment starts.

## test 3: two people

can the solver resolve two separate bodies? this is where RTI gets interesting — if you only care about localisation a single person, you could get away with a much simpler centre-of-mass estimator. the whole point of volumetric imaging is identifying multiple objects at once.

scene: two $0.4 \times 0.4 \times 1.8$ metre bodies, one in each half of the room, no noise.

| method | peak | loc error (m) | MSE |
| --- | --- | --- | --- |
| back-projection | 1.60 | 0.103 | 5.66 × 10⁻¹ |
| tikhonov λ=1.0 | 0.64 | 0.114 | 5.84 × 10⁻¹ |

103mm and 114mm errors — both methods successfully resolved the two separate bodies in the 3D viewer output (`output_two_people_3d.html`). the localisation error here is measured against the **aggregate** centre of mass of both bodies, which is why the numbers look worse than test 1 despite a successful resolution. a better metric for multi-body scenes would be per-body nearest-neighbour matching, but that's more work than it's worth for now.

## test 4: person behind a wall

this one genuinely failed, and that's actually the most informative result in the suite.

scene: a "wall" voxel block between two nodes, with a person on the far side. the wall provides a large static attenuation, the person provides a second attenuation on top.

| method | peak | loc error (m) |
| --- | --- | --- |
| tikhonov λ=1.0 | 0.00 | 1.972 |

**zero peak value.** the Tikhonov solver completely failed to localise the person. the localisation error of 1.97m essentially means the centre of mass is at the wrong end of the room.

why? two compounding issues:

1. **the straight-line forward model assumes no occlusion.** when a radio wave "passes through" a wall voxel, the model just adds the wall's attenuation to the link's total loss. it doesn't model the fact that the signal is physically blocked beyond a certain attenuation threshold. the maths is still linear.
2. **the regularisation smooths over sharp attenuation gradients.** Tikhonov's L2 penalty explicitly prefers smooth solutions. a wall next to a person is a sharp gradient. the solver blurs them together and neither is recovered cleanly.

this is a known limitation of linear forward models for RTI. the real fix involves either:

- **a non-linear forward model** that accounts for signal extinction when attenuation exceeds a certain threshold per voxel
- **L1 (sparse) regularisation** instead of L2, which preserves sharp edges

both are significant changes to the maths engine and would be the natural next step if i wanted to extend this project towards genuine through-wall imaging.

## test 5: person in the corner

edge-case geometry: a person standing hard against one corner of the room, with the majority of links passing *away* from them.

| method | peak | loc error (m) |
| --- | --- | --- |
| tikhonov λ=1.0 | 0.57 | 0.659 |

659mm error. the solver found *something*, but it's badly offset. the problem is that corners are covered by fewer links than the centre — with 8 nodes at the 8 cube corners, the link geometry heavily oversamples the middle of the room and undersamples the corners. a person in the corner only affects maybe 3-4 links, which is not enough information for the solver to triangulate.

the fix: node placement. if the real deployment has a person expected near a known corner, i'd add extra nodes along the near walls to increase the number of links that cross that region. this is a deployment-specific calibration, not a mathematical fix.

## method comparison summary

based on test 1 (single person, no noise), the three methods rank as:

| method | speed | accuracy (noiseless) | noise robustness | interpretability |
| --- | --- | --- | --- | --- |
| back-projection | fastest | best peak localisation (25mm) | poor | blurry, not quantitative |
| least squares | medium | theoretically optimal, in practice unstable | catastrophic | mathematically correct but useless |
| tikhonov λ=1.0 | medium | 61mm (worse than back-projection!) | strong | best for quantitative attenuation |

the surprising takeaway: **for a noise-free problem, back-projection's raw simplicity beats everything.** Tikhonov only earns its place when noise enters the picture, because it's the only method that maintains a stable inversion. in real ESP32 deployments, noise will always be present, so Tikhonov is still the correct default, but it's worth knowing that the fancy regularisation actually *hurts* localisation accuracy in the ideal case.

## the baseline sanity test

before running anything, i had to prove the physics engine itself was outputting sane numbers. i set up an empty room with zero attenuation everywhere, ran `forward.simulate()`, and verified that $\mathbf{y}$ came out as a vector of essentially zeros (within floating-point epsilon). then i injected a single high-attenuation voxel at the centre, simulated again, and checked that only the links physically passing through the centre returned a non-zero $\Delta y$. every link that shouldn't see the voxel returned `0.00000`. this confirms Siddon's algorithm is correctly computing ray-voxel intersections and the sparse structure of $\mathbf{W}$ is physically correct.

## the future bottleneck: multipath fading

the simulator works for the linear, line-of-sight, isotropic-noise model. in physical reality, 2.4GHz signals bounce off the floor, ceiling, walls and metal objects. the receiving ESP32 actually measures the constructive-and-destructive interference of dozens of reflected rays, not just the direct path.

if a person stands just outside the direct line of sight between two nodes, their body can block a *bounced* ray that was carrying significant power to the receiver. the measurement drops, but the forward model believes the link was unaffected because the direct ray is clear. the solver then hallucinates attenuation in the wrong voxels.

the current linear matrix $\mathbf{Wx} = \mathbf{y}$ cannot model this. the real fixes, in order of increasing effort:

1. **channel hopping.** measure RSSI across all 13 Wi-Fi channels rapidly and average, smoothing the severe multipath nulls into something more gaussian.
2. **Fresnel zone weighting.** replace Siddon's 1D ray tracing with an ellipsoidal influence region, making $W_{ij}$ a function of how close voxel $j$ is to the major axis of link $i$, not just whether the line hits it.
3. **UWB radios.** switch the physical nodes from ESP32 Wi-Fi to Ultra-Wideband transceivers. UWB's nanosecond pulses naturally reject multipath echoes and leave only the first-arriving direct ray, making the linear forward model valid again.

but the mathematical core — the sparse memory architecture, Siddon's physics engine, and the Tikhonov inverse problem — is solid and ready for physical edge devices. if that makes sense.
