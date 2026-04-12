# 04-testing-and-results

the hardest part of building a heavily mathematical C++ simulator is proving it actually works. you can't just look at a $27000$-element float vector and know if the inverse solver found the simulated human.

so the final phase was entirely about testing, metrics, and visualising the results.

## the baseline testing

before i ran the solver on a complex scene, i needed to prove the physics engine (the forward model) was outputting sane numbers. 

i set up an empty $3\text{m} \times 3\text{m} \times 3\text{m}$ virtual room with an 8-node perimeter network.

the baseline signal strength ($y_{baseline}$) is just the free-space path loss. then i injected a "human" (a highly attenuating voxel block of $0.4\text{m} 0.4\text{m} 1.8\text{m}$) dead in the centre of the room.

the $\Delta y$ (change in signal strength) should only happen on links that physically intersect the centre of the room. links running along the edges between adjacent nodes should read $0.0$ attenuation difference. and that's exactly what the C++ spit out. the sparse matrix `W` only caught the rays crossing the high-attenuation voxels.

## the localisation metric

reconstructing the image is cool, but RTI is fundamentally a localisation problem. where is the person?

to measure the accuracy of my Tikhonov solver, i wrote a function to find the "centre of mass" of the reconstructed 3D image. it scans the output `VectorXf x`, finds all the voxels with the highest attenuation values, and averages their 3D coordinates.

the error metric is simple Euclidean distance:

$$ \text{Error} = \sqrt{(x_{true} - x_{calc})^2 + (y_{true} - y_{calc})^2 + (z_{true} - z_{calc})^2} $$

### results without hardware noise

i ran the solver with $0.0$ Gaussian noise injected into the physics engine. the mathematics were perfect. 

the centre of mass of the reconstructed image was `0.025m` (2.5cm) off from the true centre of the simulated human. considering the voxels themselves were 10cm wide, the solver was basically perfectly identifying the exact voxel block the human was standing in. 

this proved the linear algebra (the Tikhonov regularisation and the Eigen Conjugate Gradient solver) was structurally flawless.

### results with empirical noise

but real ESP32s are cheap and noisy. injecting a standard deviation of 1.5 dBm noise into the measurement vector $\mathbf{y}$ drastically changes the game. 

at 1.5 dBm noise, the regularisation parameter $\lambda$ became critical.

*   if $\lambda = 0.1$ (too low): the noise amplified wildly. the solver hallucinated ghost objects in the corners of the room where there were no humans. the localisation error spiked to `1.4m`.
*   if $\lambda = 1000$ (too high): the image washed out into a grey blur.
*   if $\lambda = 50$ (the sweet spot): the Tikhonov penalty mathematically suppressed the 1.5 dBm variance perfectly. the reconstructed centre of mass landed at `0.061m` (6.1cm) error.

6.1cm accuracy in a high-noise environment is a massive win for the algorithm. it proves the underlying mathematics are robust enough for physical deployment.

## the future bottleneck: multipath fading

the simulator works, but it has a massive blind spot that i'll have to solve when i build the real hardware version. 

the forward model shoots a single straight ray from Node A to Node B. in physical reality, 2.4GHz Wi-Fi bounces off the floor, ceiling, and walls. the receiving ESP32 doesn't just hear the direct ray; it hears the constructive and destructive interference of millions of bounced rays. this is called multipath fading.

if a person stands just outside the direct line of sight, their body might block a major bounced ray, causing a huge $\Delta y$ drop even though the direct path is technically clear. 

the current linear matrix $\mathbf{Wx} = \mathbf{y}$ cannot model multipath interference. to fix this on real hardware, i won't be able to just trust a single RSSI reading. i'll have to channel-hop (measure across 13 different Wi-Fi frequencies rapidly) and average them to smooth out the severe multipath nulls before feeding the vector into my C++ solver.

but the mathematical core—the sparse memory architecture, the Siddon's physics engine, and the Tikhonov inverse problem—is entirely locked in and ready for physical edge devices. if that makes sense.