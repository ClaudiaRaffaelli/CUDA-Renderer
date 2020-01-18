#include <string>
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <vector>

#include <cuda.h>
#include <cuda_runtime.h>
#include <driver_functions.h>

#include "cudaRenderer.h"
#include "image.h"
#include "sceneLoader.h"

//Defining some constants

struct GlobalConstants {

    SceneName sceneName;

    int numCircles;
    float* position;
    float* color;
    float* radius;

    int imageWidth;
    int imageHeight;
    float* imageData;
};

// Global variable that is in scope, but read-only, for all CUDA
// kernels.  The __constant__ modifier designates this variable will
// be stored in special "constant" memory on the GPU. (constant memory
// is a fast place to put read-only variables).
__constant__ GlobalConstants cuConstRendererParams;

//Constants for hardware: GPU GeForge GT 630M (compute capability 2.1)
#define THREADS_PER_BLOCK_X 32
#define THREADS_PER_BLOCK_Y 32
//SCAN_BLOCK_DIM=1024 max number of threads per block for this hardware.
//Is used the name SCAN_BLOCK_DIM because of the file exclusiveScan.cu_inl
#define SCAN_BLOCK_DIM (THREADS_PER_BLOCK_X * THREADS_PER_BLOCK_Y)


//Including others useful utilities
#include "util.h"
#include "circleBoxTest.cu_inl"
#include "exclusiveScan.cu_inl"


////////////////////////////////////////////////////////////////////////////////////////
// Putting all the CUDA kernels here
///////////////////////////////////////////////////////////////////////////////////////


// kernelClearImage --  (CUDA device code)
//
// Clear the image, setting all pixels to the specified color rgba
__global__ void kernelClearImage(float r, float g, float b, float a) {

    int imageX = blockIdx.x * blockDim.x + threadIdx.x;
    int imageY = blockIdx.y * blockDim.y + threadIdx.y;

    int width = cuConstRendererParams.imageWidth;
    int height = cuConstRendererParams.imageHeight;

    if (imageX >= width || imageY >= height)
        return;

    int offset = 4 * (imageY * width + imageX);
    float4 value = make_float4(r, g, b, a);

    // write to global memory: As an optimization, I use a float4
    // store, that results in more efficient code than if I coded this
    // up as four separate fp32 stores.
    *(float4*)(&cuConstRendererParams.imageData[offset]) = value;
}


// shadePixel -- (CUDA device code)
//
// given a pixel and a circle, determines the contribution to the
// pixel from the circle.  Update of the image is done in this
// function.  Called by kernelRenderCircles()
// inline function: increases compile time but saves a lot of time in runtime
__device__ __inline__ void
shadePixel(int circleIndex, float2 pixelCenter, float3 p, float4* imagePtr) {

    float diffX = p.x - pixelCenter.x;
    float diffY = p.y - pixelCenter.y;
    float pixelDist = diffX * diffX + diffY * diffY;

    float rad = cuConstRendererParams.radius[circleIndex];
    float maxDist = rad * rad;

    // circle does not contribute to the image
    if (pixelDist > maxDist)
        return;

    float3 rgb;
    float alpha;

    // there is a non-zero contribution.  Now compute the shading value

    // simple: each circle has an assigned color
    int index3 = 3 * circleIndex;
    rgb = *(float3*)&(cuConstRendererParams.color[index3]);
    alpha = .5f;


    float oneMinusAlpha = 1.f - alpha;

    // BEGIN SHOULD-BE-ATOMIC REGION
    // global memory read

    float4 existingColor = *imagePtr;
    float4 newColor;
    newColor.x = alpha * rgb.x + oneMinusAlpha * existingColor.x;
    newColor.y = alpha * rgb.y + oneMinusAlpha * existingColor.y;
    newColor.z = alpha * rgb.z + oneMinusAlpha * existingColor.z;
    newColor.w = alpha + existingColor.w;

    // global memory write
    *imagePtr = newColor;

    // END SHOULD-BE-ATOMIC REGION
}

//Function used by each block to compute the number of circles present in the current
//block. Every block gets a small portion of the original image of
//dimension THREADS_PER_BLOCK_X * THREADS_PER_BLOCK_Y = SCAN_BLOCK_DIM pixels
__device__ __inline__ uint
countCircles(short * blockCoord, uint * circleCountPerThreadList, uint * circleIndexesInBlockList, uint * circleCountPerBlockList){

	// Computing the thread index for that block (as required from exclusiveScan.cu_inl)
	int linearThreadIndex = threadIdx.y * blockDim.x + threadIdx.x;

	short imageWidth = cuConstRendererParams.imageWidth;
	short imageHeight = cuConstRendererParams.imageHeight;

	//inverted width and height
	float invWidth = 1.f / imageWidth;
	float invHeight = 1.f / imageHeight;


	//Remembering that blockCoord[0] is the block left coordinate,
	//[1] the right coordinate, [2] the top, and [3] the bottom

	// array of block coordinates normalized
	float leftIndex = blockCoord[0] * invWidth;
	float rightIndex = blockCoord[1] * invWidth;
	float topIndex = blockCoord[2] * invHeight;
	float bottomIndex = blockCoord[3] * invHeight;

	//Each thread inside the block handles one portion of the array containing all the
	//circles within the image. Each of this threads check if their circles are
	//present or not in the block. When all the threads have finished (after the
	//syncthreads()) we have all the circles within the block.

	//Each thread gets an equal amount of circle index to handle, depending on how many
	//circles there are to draw in the original image.

	int circlesPerThread = (cuConstRendererParams.numCircles + SCAN_BLOCK_DIM - 1) / SCAN_BLOCK_DIM;
	int circleIndexStart = linearThreadIndex * circlesPerThread;
	int circleIndexEnd=0;
	if(linearThreadIndex == SCAN_BLOCK_DIM)
		circleIndexEnd = cuConstRendererParams.numCircles;
	else{
		circleIndexEnd = circleIndexStart + circlesPerThread;
	}

	int circleCountPerThread = 0;

	//CIRCLES_PER_THREAD must contain the size of the array circleArrayPerThread.
	//We don't know yet how many circles there will be in a certain block.
	//The idea is to take a number big enough
	const uint CIRCLES_PER_THREAD=32;
	uint circleArrayPerThread[CIRCLES_PER_THREAD];

	for(int i = circleIndexStart; i< circleIndexEnd; i++){
		if(i<cuConstRendererParams.numCircles){
			float3 position = *(float3*)(&cuConstRendererParams.position[i*3]);
			float radius = cuConstRendererParams.radius[i];

			//call to func from file circleBoxTest.cu_inl to determine if the bounding box of the
			//circle belongs for at least one pixel to the current block
			if(circleInBoxConservative(position.x, position.y, radius, leftIndex, rightIndex, bottomIndex, topIndex) == 1){
				circleArrayPerThread[circleCountPerThread] = i;
				circleCountPerThread++;
			}
		}
	}

	// circleCount contains for each thread the number of circles within that block, found handling
	// a portion of the original array of all circles within the image.

	circleCountPerThreadList[ linearThreadIndex ] = circleCountPerThread;
	__syncthreads();

	//Using the utility given by the file exlusiveScan.cu_inl is possible to compute the sum of each
	//element of the array circleCount. By doing so we have now the total amount of circles present
	//within the current block

	sharedMemExclusiveScan(linearThreadIndex, circleCountPerThreadList, circleCountPerBlockList, circleIndexesInBlockList, SCAN_BLOCK_DIM);
	__syncthreads();

	//updating the totalCircles (sharedMemExlusiveScan is exclusive, so we have to add the last element)
	uint totalCircles = circleCountPerBlockList[SCAN_BLOCK_DIM-1] + circleCountPerThreadList[SCAN_BLOCK_DIM-1];

	//Before returning we have to copy back the list of index for each block (keeping the ordering unchanged)
	uint tmpIndex = circleCountPerBlockList[ linearThreadIndex ];

	for(int i=0; i<circleCountPerThread; i++){
		circleIndexesInBlockList[tmpIndex] = circleArrayPerThread[i];
		tmpIndex++;
	}
	__syncthreads();

	return totalCircles;
}


// kernelRenderCircles -- (CUDA device code)
//
// The image is divided in smaller fractions treated individually
// by computing the number of circles in that block.
// After that, knowing the number and index of circles per block
// is possible to "shade" all the pixels of the block in a parallel
// way by assigning each pixel to a different thread. Each thread
// runs through the list of circles per block (in which the ordering
// is correct), check if that pixel belongs to the circle. If so
// it applies the shading to that pixel, otherwise pass over.
__global__ void kernelRenderCircles() {

	//Part 1:
	//Consists in dividing the image in small parts (one for each
	//block) that are handled individually. First of all are computed how many
	//and which circles are present per block.
	//Each thread in a block works on a different portion of the array of circles,
	//so that by the end of the computation the entire list of circles has been analyzed.
	//(Only if there are many circles in the image, otherwise this step is avoided)

	//For the task of finding what circles are present in a block, are needed three
	//arrays in shared memory (shared between the threads of the same block), used
	//by the file exclusiveScan.cu_inl for:

	//storing for each thread how many circles has found, each of them working on
	//a small portion of the original array with all the circles in the image.
	__shared__ uint circleCountPerThreadList[SCAN_BLOCK_DIM];
	//storing the number of circles found per block
	__shared__ uint circleCountPerBlockList[SCAN_BLOCK_DIM];
	//keeping the circle indexes found in the block
	__shared__ uint circleIndexesInBlockList[SCAN_BLOCK_DIM * 2];

	short imageWidth = cuConstRendererParams.imageWidth;
	short imageHeight = cuConstRendererParams.imageHeight;

	//inverted width and height
	float invWidth = 1.f / imageWidth;
	float invHeight = 1.f / imageHeight;

	//Setting the block coordinates

	short blockLeftCoord = blockIdx.x * THREADS_PER_BLOCK_X;
	short blockRightCoord = blockLeftCoord + THREADS_PER_BLOCK_X - 1;
	short blockTopCoord = blockIdx.y * THREADS_PER_BLOCK_Y;
	short blockBottomCoord = blockTopCoord + THREADS_PER_BLOCK_Y - 1;


	short blockCoord[]{blockLeftCoord,blockRightCoord,blockTopCoord,blockBottomCoord };

	//Compute the list of circles regarding the current block.
	//The functions returns the total number of circles within the current
	//block. Also it sets circleCount, circleIndexesInBlock and indexList (?)
	uint circlesNumberTotal=countCircles(blockCoord, circleCountPerThreadList, circleIndexesInBlockList, circleCountPerBlockList);

	//Part 2:
	//circlesNumberTotal contains the number of circles within the block.
	//In this part is performed the rendering. Each thread gets the coordinate
	//of a pixel. Then for each pixel is checked if that pixel belongs or not
	//to the first circle in the restricted list. If so, the color of the pixel is
	//updated, otherwise not. In both cases the next move is to check the second
	//circle and so on.

	// Computing the pixels coordinates
	uint pixelXCoord = blockLeftCoord + threadIdx.x;
	uint pixelYCoord = blockTopCoord + threadIdx.y;

	// Computed imgPtr and the pixel center
	float4* imgPtr = (float4*)(&cuConstRendererParams.imageData[4 * (pixelYCoord * imageWidth + pixelXCoord)]);
	float2 pixelCenterNorm = make_float2(invWidth * (static_cast<float>(pixelXCoord) + 0.5f),
	    invHeight * (static_cast<float>(pixelYCoord) + 0.5f));

	// Given a pixel is checked for all the circles counted within the block if that
	//pixel belongs to the circle or not, by calling shadePixel. shadePixel is also responsible
	//for coloring properly the pixel.
	//The right coloring order is respected because of the way the circleIndexesInBlock is created. The list
	//is restricted to the circles present within the block but with the same ordering.
	for(uint i=0; i<circlesNumberTotal;i++){
		uint circleIndex=circleIndexesInBlockList[i];
		float3 circlePosition=*(float3*)(&cuConstRendererParams.position[circleIndex*3]);
		shadePixel(circleIndex, pixelCenterNorm, circlePosition, imgPtr);
	}

}

////////////////////////////////////////////////////////////////////////////////////////


CudaRenderer::CudaRenderer() {
    image = NULL;

    numCircles = 0;
    position = NULL;
    color = NULL;
    radius = NULL;

    cudaDevicePosition = NULL;
    cudaDeviceColor = NULL;
    cudaDeviceRadius = NULL;
    cudaDeviceImageData = NULL;
}

CudaRenderer::~CudaRenderer() {

    if (image) {
        delete image;
    }

    if (position) {
        delete [] position;
        delete [] color;
        delete [] radius;
    }

    if (cudaDevicePosition) {
        cudaFree(cudaDevicePosition);
        cudaFree(cudaDeviceColor);
        cudaFree(cudaDeviceRadius);
        cudaFree(cudaDeviceImageData);
    }
}

const Image*
CudaRenderer::getImage() {

    // need to copy contents of the rendered image from device memory
    // before we expose the Image object to the caller

    printf("Copying image data from device\n");

    cudaMemcpy(image->data,
               cudaDeviceImageData,
               sizeof(float) * 4 * image->width * image->height,
               cudaMemcpyDeviceToHost);

    return image;
}

void
CudaRenderer::loadScene(SceneName scene) {
    sceneName = scene;
    loadCircleScene(sceneName, numCircles, position, color, radius);
}

void
CudaRenderer::setup() {

    int deviceCount = 0;
    std::string name;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);

    printf("---------------------------------------------------------\n");
    printf("Initializing CUDA for CudaRenderer\n");
    printf("Found %d CUDA devices\n", deviceCount);

    for (int i=0; i<deviceCount; i++) {
        cudaDeviceProp deviceProps;
        cudaGetDeviceProperties(&deviceProps, i);
        name = deviceProps.name;

        printf("Device %d: %s\n", i, deviceProps.name);
        printf("   SMs:        %d\n", deviceProps.multiProcessorCount);
        printf("   Global mem: %.0f MB\n", static_cast<float>(deviceProps.totalGlobalMem) / (1024 * 1024));
        printf("   CUDA Cap:   %d.%d\n", deviceProps.major, deviceProps.minor);
    }
    printf("---------------------------------------------------------\n");

    // By this time the scene should be loaded.  Now copy all the key
    // data structures into device memory so they are accessible to
    // CUDA kernels
    //
    // See the CUDA Programmer's Guide for descriptions of
    // cudaMalloc and cudaMemcpy

    cudaMalloc(&cudaDevicePosition, sizeof(float) * 3 * numCircles);
    cudaMalloc(&cudaDeviceColor, sizeof(float) * 3 * numCircles);
    cudaMalloc(&cudaDeviceRadius, sizeof(float) * numCircles);
    cudaMalloc(&cudaDeviceImageData, sizeof(float) * 4 * image->width * image->height);

    cudaMemcpy(cudaDevicePosition, position, sizeof(float) * 3 * numCircles, cudaMemcpyHostToDevice);
    cudaMemcpy(cudaDeviceColor, color, sizeof(float) * 3 * numCircles, cudaMemcpyHostToDevice);
    cudaMemcpy(cudaDeviceRadius, radius, sizeof(float) * numCircles, cudaMemcpyHostToDevice);

    // Initialize parameters in constant memory.  We didn't talk about
    // constant memory in class, but the use of read-only constant
    // memory here is an optimization over just sticking these values
    // in device global memory.  NVIDIA GPUs have a few special tricks
    // for optimizing access to constant memory.  Using global memory
    // here would have worked just as well.  See the Programmer's
    // Guide for more information about constant memory.

    GlobalConstants params;
    params.sceneName = sceneName;
    params.numCircles = numCircles;
    params.imageWidth = image->width;
    params.imageHeight = image->height;
    params.position = cudaDevicePosition;
    params.color = cudaDeviceColor;
    params.radius = cudaDeviceRadius;
    params.imageData = cudaDeviceImageData;

    cudaMemcpyToSymbol(cuConstRendererParams, &params, sizeof(GlobalConstants));

}

// allocOutputImage --
//
// Allocate buffer the renderer will render into.  Check status of
// image first to avoid memory leak.
void
CudaRenderer::allocOutputImage(int width, int height) {

    if (image)
        delete image;
    image = new Image(width, height);
}

// clearImage --
//
// Clear's the renderer's target image.  The state of the image after
// the clear depends on the scene being rendered.
void
CudaRenderer::clearImage() {

    // 256 threads per block is a healthy number
    dim3 blockDim(16, 16, 1);
    dim3 gridDim(
        (image->width + blockDim.x - 1) / blockDim.x,
        (image->height + blockDim.y - 1) / blockDim.y);

    kernelClearImage<<<gridDim, blockDim>>>(1.f, 1.f, 1.f, 1.f);

    cudaDeviceSynchronize();
}

void
CudaRenderer::render() {
	//numbers of threads per blocks (blockDim). If blockDim(X,Y) then there are X*Y threads per block
	dim3 blockDim(THREADS_PER_BLOCK_X, THREADS_PER_BLOCK_Y);
	//gridDim is the number of blocks. If gridDim(X,Y) then there are X*Y blocks
	dim3 gridDim((image->width + THREADS_PER_BLOCK_X - 1) / THREADS_PER_BLOCK_X,(image->height + THREADS_PER_BLOCK_Y - 1) / THREADS_PER_BLOCK_Y);
	kernelRenderCircles<<<gridDim, blockDim>>>();
	cudaDeviceSynchronize();
}
