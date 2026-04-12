# worst-case-scenarios: when the model breaks

all mathematical simulations are inherently flawed. a strong systems engineer should know exactly how their algorithm will fail long before compiling it.

if you sit in front of code reviewers and say "the simulator is perfect and the physics run fine," they will tear your model apart. you must beat them to it.

if they ask you: "What if X happens?", these are the catastrophic failures of the rti algorithm and how to address them defensively.

---

## Scenario 1: The Model Fails (Multipath Interference)

> **Q:** "You're simulating 2.4GHz signals propagating through an empty $3m \times 3m$ room. But Wi-Fi signals reflect off walls and floors. If a person stands in the corner, they could block a reflected ray that was constructively interfering with the direct Line of Sight (LOS) ray. The measured signal drops massively, but your forward model assumes the direct ray is empty. What happens to the matrix?"

**The Problem:**
This is the single biggest flaw in Radio Tomographic Imaging. The forward model assumes $y_i = \sum W_{ij}x_j$. This is the "Shadow fading" model. It assumes radio behaves exactly like an X-ray CT scan: a straight, tight beam of light.

If multipath fading dominates the measurement, the $\Delta y$ vector is completely disjointed from the $\mathbf{x}$ (voxel attenuation) vector.

**The Catastrophe:**
The Tikhonov solver will try to find a mathematical solution to a physically impossible problem. It will hallucinate a massive dense object floating in the middle of the room to account for the sudden signal drop, completely destroying the localisation accuracy metric.

**The Real-World Fix:**
"The current linear solver ($\mathbf{Wx} = \mathbf{y}$) mathematically cannot model constructive/destructive interference. To deploy this on real hardware, I cannot trust a single RSSI reading. I will need to channel-hop (e.g. read measurements across all 13 Wi-Fi frequencies) and average them to smooth out the severe multipath nulls before passing the vector into my C++ solver. The other option is switching the physical nodes to UWB (Ultra-Wideband) radios, which have nanosecond pulses capable of rejecting multipath echoes entirely, leaving only the pure Line of Sight ray for the math engine."

---

## Scenario 2: The Model Fails (Non-Linearity)

> **Q:** "The matrix equation $\mathbf{Wx} = \mathbf{y}$ assumes attenuation is strictly linear and additive. But human bodies cause heavy diffraction. If the signal bends around a person's chest instead of passing straight through it, the path length $W_{ij}$ is completely wrong. What breaks?"

**The Problem:**
At 2.4GHz (wavelength $\sim 12cm$), the wavelength is on the same order of magnitude as a human limb. The human body does not just absorb RF energy; it diffracts it wildly. The true spatial weighting function is not a 1D straight line (the Siddon's ray). It is a 3D ellipsoidal "Fresnel zone".

**The Catastrophe:**
If diffraction introduces a non-linear path, the linear matrix $\mathbf{W}$ fails to map the true physical interaction. The solver will smear the reconstructed image of the human over a massive area, struggling to converge on a single high-attenuation centre.

**The Real-World Fix:**
"The standard Straight-Line model is a first-order approximation. For true volumetric imaging, I would have to transition the forward model to generate a **Fresnel Zone Weight Matrix**. Instead of a 1D ray, the new $W_{ij}$ calculation would map an ellipsoid of influence between the nodes. The linear algebra solver ($\mathbf{Wx} = \mathbf{y}$) and Tikhonov regularisation remain mathematically identical, but generating the $\mathbf{W}$ matrix in C++ would become significantly more computationally expensive, replacing Siddon's algorithm with a 3D distance equation calculating proximity to the major axis connecting the two node receivers."

---

## Scenario 3: Execution Time (Embedded hardware)

> **Q:** "You wrote this in C++ with Eigen to run on ESP32s on the edge. But generating a $27,000 \times 27,000$ Tikhonov matrix and running a Conjugate Gradient iterative solver takes megabytes of RAM and heavy floating-point operations. The ESP32 only has 520KB of SRAM. How does the simulator actually port to your hardware target?"

**The Problem:**
The mathematics run beautifully on an x86 laptop with 16GB of RAM. The embedded constraint changes everything. The ESP32 cannot hold the matrix $\mathbf{W}$ in its SRAM. Even if we use the external PSRAM (8MB), calculating the matrix inverse ($\mathbf{W}^T\mathbf{W} + \lambda \mathbf{I}$) will take several seconds per frame, rendering "real-time" localisation impossible.

**The Catastrophe:**
Out Of Memory (OOM) fatal exception on the microcontroller before the first frame of $\mathbf{x}$ is computed.

**The Real-World Fix:**
"The ESP32 cannot compute the inverse problem dynamically in real-time. Since the baseline room dimensions and node positions are static, the entire Tikhonov pseudo-inverse matrix: $\mathbf{\Pi} = (\mathbf{W}^T\mathbf{W} + \lambda \mathbf{I})^{-1}\mathbf{W}^T$ is computationally constant. It never changes during runtime unless a sensor node moves.

I would run the heavy Siddon's algorithm and matrix inversion purely on a host machine (my laptop) during a one-time calibration phase. I would export the final solved matrix $\mathbf{\Pi}$ as a binary flash file to the ESP32 array.

During runtime on the embedded hardware, the entire $27,000$-voxel reconstruction collapses from a multi-stage Conjugate Gradient iterative solver into a single, highly efficient matrix-vector multiplication step: $\mathbf{x} = \mathbf{\Pi} \mathbf{y}$. By pre-computing the inverse, the ESP32 only has to perform basic $O(N)$ multiplication instantly, bypassing the memory and timing limits."