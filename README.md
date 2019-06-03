<p align="center">
<img src="https://raw.githubusercontent.com/Erfan-Ahmadi/circle_collision/master/docs/LOGO.png" align="center" alt="" width="450" height="43" />
</p>

# Circle Collision
Implementing Different Methods of Circle to Circle Collision Detection using Vulkan Graphics and Compute API  

I'll be refering to the simplest collision detection algorithm which has two nested "for"'s **Double For Algorithm**  

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

Implementing these Collision Detection Methods:
- Double For (simplest)
- Sort and Sweep Algorithm
- Spatial Parttioning
  - Grid
  - Quadtree/Octree
  - BSP
  - k-d Trees
  - Bounding Volume 

**This NOT a benchmark, Although It would give me some ideas and thing to be suspicius about.
And also I would appreciate optimization ideas and results on your system.**

# Features

- Double For
  - [x] CPU
  - [x] Compute Shader
  - [ ] SIMD
  
- Spatial Partitioning
  - [ ] CPU
  - [ ] Compute Shader
  - [ ] SIMD
  

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
