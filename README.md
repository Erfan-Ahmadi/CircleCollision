<p align="center">
<img src="https://raw.githubusercontent.com/Erfan-Ahmadi/circle_collision/master/docs/LOGO.png" align="center" alt="" height="200"/>
</p>

<p align="center">
<img src="https://www.khronos.org/assets/uploads/apis/vulkan2.svg" align="center" alt="" height="40" hspace="20"/>
<img src="https://upload.wikimedia.org/wikipedia/commons/c/c9/Intel-logo.svg" align="center" alt="" height="40" hspace="20"/>
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
  - **CPU**
    - Simple Sequential  
    - Multi-Threaded
    - SIMD
      - AVX2
      - AVX-512
  - **GPU**
    - Vulkan Compute Shaders
    - OpenCL

<img src="https://raw.githubusercontent.com/Erfan-Ahmadi/circle_collision/master/docs/spatial%20partitioning.PNG" alt="" />

# Implemented

- Brute Force
  - [x] [CPU](https://github.com/Erfan-Ahmadi/CircleCollision/tree/master/src/simple_cpu)
  - [x] [Compute Shader](https://github.com/Erfan-Ahmadi/CircleCollision/tree/master/src/simple_compute_shader)
  - [x] **SIMD: AVX2**
      - [x] [Load/Store Same Simple Data Structure and Calculate](https://github.com/Erfan-Ahmadi/CircleCollision/tree/master/src/simple_simd_avx2)
      - [x] [All SIMD Data Strcuture](https://github.com/Erfan-Ahmadi/CircleCollision/tree/master/src/simple_simd_avx2_better)
  - [x] **Multi-Threaded**
  - [ ] **Multi-Threaded + SIMD**

# Build
- [**Visual Studio 2019**](https://github.com/Erfan-Ahmadi/CircleCollision/tree/master/build/Visual%20Studio%202019) 
- :soon: **Linux** Builds Will be Added Soon

# Blog 
You can here about implementation details soon in my blog, [GraphicsCoder](https://graphicscoder.com)

# Slides
[Presentation Slides](https://docs.google.com/presentation/d/1qI02vD9Wr6rhxHnOlLWc8WHF7K7XYQr85-vk4y1C0ro/edit?usp=sharing)  

# Resources
- SIMD
  - [SIMD at Insomniac Games](https://www.gdcvault.com/play/1022248/SIMD-at-Insomniac-Games-How)
  - [Crunching Numbers with AVX and AVX2](https://www.codeproject.com/Articles/874396/Crunching-Numbers-with-AVX-and-AVX)
  - [Intel Intrinsics Guide](https://software.intel.com/sites/landingpage/IntrinsicsGuide/#techs=AVX2)
- Spatial Partitioning
  - [Game Programming Patterns Book/Spatial Partitioning](https://gameprogrammingpatterns.com/spatial-partition.html)
- Vulkan and GPU Programming
  - [A Simple Vulkan Compute Example](http://www.duskborn.com/posts/a-simple-vulkan-compute-example/)
  - [Thinking Parallel, Part I: Collision Detection on the GPU](https://devblogs.nvidia.com/thinking-parallel-part-i-collision-detection-gpu/)
- Circle Collision Handling
  - [Circle-Circle Collision Tutorial (Math)](https://ericleong.me/research/circle-circle/)
  - [Programming Balls #1 Circle Vs Circle Collisions C++ (Fun Video)](https://www.youtube.com/watch?v=LPzyNOHY3A4s)
  
  
:heavy_exclamation_mark: This project is not for benchmarking purposes!, Although It would give me some ideas to be suspicious about.

:heart_decoration: I would appreciate optimization ideas and results on your system on [issues](https://github.com/Erfan-Ahmadi/CircleCollision/issues).

