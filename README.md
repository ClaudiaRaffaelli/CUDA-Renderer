# CUDA-Renderer
Simple parallel implementation of a renderer that draws colored circles using CUDA.
This implementation is based on the assignment of a CMU's course (http://15418.courses.cs.cmu.edu/fall2016/article/4), whereas the starting code can be found at the link https://github.com/cmu15418/assignment2.

## Overview of the problem

The goal of this work is to provide two implementations of the rendering of various circles patterns:
- A **sequential** version, provided in the file `refRenderer.cpp`. 
- A **parallel** version, provided in the file `cudaRenderer.cu`.
Also this work aims to offer the opportunity to test the correctness of the parallel version, providing a simple benchmark mode, that allows to compare the two images generated by both version of the code.

The basic sequential algorithm for rendering each frame is:

```
Clear image
for each circle
    update position
for each circle
    compute screen bounding box
    for all pixels in bounding box
        compute pixel center point
        if center point is within the circle
            compute color of circle at point
            blend contribution of circle into image for this pixel
```

An important detail of the renderer is that it renders semi-transparent circles. Therefore, the color of any one pixel is not the color of a single circle, but the result of blending the contributions of all the semi-transparent circles overlapping the pixel (note the "blend contribution" part of the pseudocode above). The renderer represents the color of a circle via a 4-tuple of red (R), green (G), blue (B), and opacity (alpha) values (RGBA). Alpha = 1 corresponds to a fully opaque circle. Alpha = 0 corresponds to a fully transparent circle. 

Notice that composition is not commutative (object X over Y does not look the same as object Y over X), so it's important that the render draw circles in a manner that follows the order they are provided by the application. The application provides the circles in depth order. With this motivation is important to mantain the correct ordering of the circles even in the parallel algorithm.

### Render requirements
More specifically the Cuda implementation must maintain two invariants that are preserved trivially in the sequential implementation.

**Atomicity:** All image update operations must be atomic. The critical region includes reading the pixel's rgba color, blending the contribution of the current circle with the current image value, and then writing the pixel's color back to memory.

**Order:** Also the renderer must perform updates to an image pixel in circle input order. That is, if circle 1 and circle 2 both contribute to pixel P, any image updates to P due to circle 1 must be applied to the image before updates to P due to circle 2. Preserving the ordering requirement allows for correct rendering of transparent circles.

### Examples

In the images below are provided two examples of two patterns. On the left can be found the sequential version of the code and on the right the wrong implementation of the Cuda renderer. Comparing the two images can be spotted clearly the differences caused by the lack of atomicity and order.

<p align="center">
    <i>RGB pattern, Sequential and Parallel version:</i>
</p>
<p float="left" align="center">
  <img src="./img/rgb_cuda.jpg" width="400" height="400" />
  <img src="./img/rgb_old.jpg" width="400" height="350" /> 
</p>

<p align="center">
    <i>Rand10k pattern, Sequential and Parallel version:</i>
</p>
<p float="left" align="center">
  <img src="./img/rand10k_cuda.jpg" width="400" height="400" />
  <img src="./img/rand10k_old.jpg" width="400" height="400" /> 
</p>


## Solution

The solution provided operates on the parallelism across pixels. First of all the image is divided into smaller parts. Each of this parts is assigned to a Cuda Block. Each thread inside each block manage a different portion of the original array of circles (within the entire image). Each thread handle a portion of circle indexes, checking if those circles are present in the current block. 
The circles present in a block are stored in shared memory. Their ordering is preserved, because of the way they are added to the new reduced array.

Then each thread of each block is assigned to a specific pixel. At this point is checked every circle of the restricted array. In particular is checked sequentially if the current pixel belongs to each circle of the new array. If so, the color of the pixel is updated taking care of the right ordering.

## How to use the program

First of all build the code from Terminal, using the command:
```
make
```
Following are some of the options to `./render`:
```
-b  --bench <Number of frames>    Benchmark mode, do not create display, but save the specified number of frames. 
-c  --check              Runs 10 frames of sequential and cuda versions and checks correctness of cuda code, providing average timings and speedup  
-f  --file  FILENAME     Save frames with the specified filename (FILENAME_xxxx.ppm)
-r  --renderer WHICH     Select renderer: WHICH=ref or cuda (ref by default)
-?  --help               Prints information about switches mentioned here. 
```

