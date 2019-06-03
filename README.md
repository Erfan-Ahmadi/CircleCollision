<p align="center">
<img src="https://raw.githubusercontent.com/Erfan-Ahmadi/circle_collision/master/docs/LOGO.png" align="center" alt="" width="360" height="34" />
</p>

# Circle Collision
Implementing Different Methods of Circle to Circle Collision Detection using Vulkan Graphics and Compute API  

**Circles** were chosen to focus more on [**Broad-Phase**](https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch32.html) algorithms of the Collision Detection Pipeline.

<p align="center">
 <img src="https://raw.githubusercontent.com/Erfan-Ahmadi/circle_collision/master/docs/Collision-detection-pipeline.png" width="372" height="85"/>
 </p>

# Images and Preview

<p align="center">
 
 <img src="https://raw.githubusercontent.com/Erfan-Ahmadi/circle_collision/master/docs/heart_collision.jpg" alt="" width="320" height="180" />
 <img src="https://raw.githubusercontent.com/Erfan-Ahmadi/circle_collision/master/docs/draw-fun.gif" alt="" width="320" height="180" />
 <img src="https://raw.githubusercontent.com/Erfan-Ahmadi/circle_collision/master/docs/explode_fun.gif" alt="" width="320" height="180" />

</p>
 
# Motivation

This Project Is For Learning Purposes of Following Topics
- SIMD/Vectorization using **AVX**/**AVX2**
- Vulkan Compute Shaders and GPGPU Programming
- Data Oriented Programming

# Features

- **Broad-Phase**
  - Brute Force ( O(n^2) )
  - Spatial Partitioning
    - Grid
    - K-D Tree
    - Quadtrees
    - BSP
  - Sort and sweep
  - Simplex-Based : GJK
  
- **Narrow-Phase**
   - We Have Circles Here and Math/Physics Calculations are easy.
   
- **Technologies/API's**
  - Simple CPU 
  - Vulkan Compute Shaders
  - CPU Multi-Threading
  - OpenCL GPU
  - SIMD AVX2

<img src="https://raw.githubusercontent.com/Erfan-Ahmadi/circle_collision/master/docs/spatial%20partitioning.PNG" alt="" />

# Implemented

- Brute Force
  - [x] CPU
  - [x] Compute Shader
  

# Build
- **MSVC14** 

CMAKE and Linux Builds Will be Added Soon

# Blog 
You can here about implementation details soon in my blog, [GraphicsCoder](https://graphicscoder.com)

# Slides
[Presentation Slides](https://docs.google.com/presentation/d/1qI02vD9Wr6rhxHnOlLWc8WHF7K7XYQr85-vk4y1C0ro/edit?usp=sharing)  

# Resources
- [Game Programming Patterns Book/Spatial Partitioning](https://gameprogrammingpatterns.com/spatial-partition.html)
- [Circle-Circle Collision Tutorial (Math)](https://ericleong.me/research/circle-circle/)
- [Programming Balls #1 Circle Vs Circle Collisions C++ (Fun Video)](https://www.youtube.com/watch?v=LPzyNOHY3A4s)
- [A Simple Vulkan Compute Example](http://www.duskborn.com/posts/a-simple-vulkan-compute-example/)
- [Thinking Parallel, Part I: Collision Detection on the GPU](https://devblogs.nvidia.com/thinking-parallel-part-i-collision-detection-gpu/)
