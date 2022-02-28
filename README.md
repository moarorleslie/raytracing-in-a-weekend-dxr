# DXR-RayTracingInAWeekend
- Implemented Peter Shirley's Ray Tracing in a Weekend (RTIAW) and parts of Weekend Two with DXR
- Video Demo: https://youtu.be/IQIxIsFdRGs 
- See RTEngine – ```IntializeScene()``` to toggle between the two demos
  -	Demo 1 - Ray Tracing in a Weekend in DXR
  -	Demo 2 - Ray Tracing the Next Week in DXR


## Getting Started
- Ensure your machine is DXR compatible 
- Launch the sln and build the application
- If incompatibility errors are showing, most likely the wrong gpu is being used, the app's .exe can be added to Graphics Settings in Windows, and the GPU can be set manually.

## Features - Weekend 1
- Setup architecture in DXR so spheres can be added to world similar to how they are created in RTIAW
- Dielectric, diffuse, and metal shaders
- A/D to move camera/rotate lights

## Features - Weekend 2
- Moving Sphere
- Textures
- Fuzz
- Perlin Noise
