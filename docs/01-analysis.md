# Analysis

## 1. problem definition

so the core problem i'm trying to solve is this: how do you detect and localise objects (specifically people) in a 3D space without using cameras? the answer is radio tomographic imaging (RTI) - using radio signals between wireless sensors to "see" through walls and detect what's blocking the signal.

this matters because:
- cameras don't work in the dark, through smoke, or behind walls
- radar is expensive and complex
- WiFi sensors (ESP32s) are cheap (under £5 each) and everywhere
- applications range from home security to search and rescue

the idea is borrowed from medical CT scanning. in a CT scan, X-rays pass through the body and detectors measure how much signal gets absorbed. from those measurements, you reconstruct a 3D image. RTI does exactly the same thing but with WiFi signals instead of X-rays.

## 2. literature review

the foundational work in RTI comes from Wilson and Patwari (2010) at the University of Utah. they demonstrated that a network of wireless sensors could image moving objects by measuring signal strength changes across multiple links. their key contribution was formalising the weight matrix model: **y = Wx**, where measurements y are a linear function of the attenuation image x.

Kaltiokallio et al. (2012) extended this to handle through-wall scenarios, showing that RTI can detect people even when walls partially block the signal. they found that regularisation is essential because the inverse problem is heavily underdetermined.

Zhao and Patwari (2013) introduced the concept of using Channel State Information (CSI) instead of basic RSSI measurements, achieving much higher resolution by exploiting the phase information across multiple sub-carriers.

Hamilton et al. (2014) explored 3D RTI specifically, demonstrating that vertical links (floor-to-ceiling) are critical for height estimation. their 8-node cube configuration is what i based my sensor layout on.

Bocca et al. (2014) addressed the noise problem in RTI, showing that Tikhonov regularisation outperforms simple back-projection in noisy environments, which directly influenced my choice of reconstruction algorithm.

## 3. existing solutions

| solution | pros | cons |
|---|---|---|
| camera-based (CCTV) | high resolution, cheap | no privacy, fails in dark/smoke |
| LIDAR | very accurate, 3D | expensive (£200+), line of sight only |
| passive infrared (PIR) | cheap, simple | binary only (present/not present), no location |
| radar | works through walls | expensive, complex signal processing |
| WiFi RTI | cheap sensors, works through walls, 3D capable | lower resolution, needs calibration |

my project focuses on WiFi RTI because it sits in a unique sweet spot: cheap hardware, through-wall capability, and 3D localisation. the trade-off is resolution, but for detecting people (not identifying them), it's more than sufficient.

## 4. stakeholder analysis

**home security**: detect intruders without cameras. works in darkness, can cover areas where cameras can't be placed. no privacy concerns since it only detects presence, not identity.

**healthcare/elderly care**: monitor elderly people living alone. detect falls or unusual inactivity without the invasiveness of cameras. the voxel-level resolution is enough to distinguish "person standing" from "person on the floor".

**search and rescue**: detect people trapped behind rubble or walls after earthquakes. RF signals penetrate materials that cameras and LIDAR can't see through.

**smart buildings**: occupancy sensing for energy management. know which rooms are occupied to optimise heating/lighting without identifying individuals.

## 5. requirements specification

### functional requirements

| id | requirement | priority |
|---|---|---|
| F1 | represent a 3D space as a voxel grid with configurable resolution | must |
| F2 | model a network of sensor nodes at arbitrary 3D positions | must |
| F3 | simulate signal propagation between all node pairs | must |
| F4 | compute the weight matrix using ray-voxel intersection | must |
| F5 | add configurable noise to simulated measurements | must |
| F6 | reconstruct 3D attenuation image from measurements | must |
| F7 | support multiple reconstruction algorithms | should |
| F8 | evaluate reconstruction accuracy with quantitative metrics | must |
| F9 | export 2D slice images for analysis | should |
| F10 | generate interactive 3D visualisation | should |
| F11 | load/save scene configurations from JSON files | could |
| F12 | support preset test scenes for reproducible testing | should |

### non-functional requirements

| id | requirement | priority |
|---|---|---|
| NF1 | build and run on Linux with standard C++ toolchain | must |
| NF2 | complete reconstruction in under 60 seconds | should |
| NF3 | every source line documented with explanatory comments | must |
| NF4 | modular architecture (each component independently testable) | must |
| NF5 | no external runtime dependencies (header-only libraries only) | should |

## 6. success criteria

the project is successful if:
1. the simulator can reconstruct the location of a person to within 0.2m accuracy from clean measurements
2. the reconstruction degrades gracefully with increasing noise
3. the system can distinguish between one and two people in the space
4. all code is documented line-by-line and the maths can be explained from first principles
5. a working 3D visualisation demonstrates the reconstruction results
