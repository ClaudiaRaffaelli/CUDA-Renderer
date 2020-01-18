#include <string>
#include <math.h>

#include "circleRenderer.h"
#include "cycleTimer.h"
#include "image.h"
#include "ppm.h"

static void compare_images(const Image* ref_image, const Image* cuda_image) {
    int i;

    int mismatch_count = 0;
    
    if (ref_image->width != cuda_image->width || ref_image->height != cuda_image->height) {
        printf ("Error : width or height of reference and cuda not matching\n");
        printf ("Cuda : width = %d, height = %d\n", cuda_image->width, cuda_image->height);
        printf ("Ref : width = %d, height = %d\n", ref_image->width, ref_image->height);
        exit (1);
    }
    
    for (i = 0 ; i < 4 * ref_image->width * ref_image->height; i++) {
        // Compare with floating point error tolerance of 0.1f and ignore alpha
        if (fabs(ref_image->data[i] - cuda_image->data[i]) > 0.1f && i%4 != 3) {
            mismatch_count++;

            //Uncomment this section to see what are the errors found comparing pixels
            //
            // Get pixel number and print values
            //int j = i/4;
            //printf ("Mismatch detected at pixel [%d][%d], value = %f, expected %f ",
            //        j/cuda_image->width, j%cuda_image->width,
            //        cuda_image->data[i], ref_image->data[i]);

            //printf ("for color ");
            //switch (i%4) {
            //    case 0 : printf ("Red\n"); break;
            //    case 1 : printf ("Green\n"); break;
            //    case 2 : printf ("Blue\n"); break;
            //}
        }

        // Ignore some errors - may come up because of rounding in distance calculation
        if (mismatch_count > 100) {
            printf ("ERROR : Mismatch detected between reference and actual\n");
            printf("Found %d errors",mismatch_count);
            exit (1);
        }
    }
    printf("Found %d errors\n",mismatch_count);
    printf ("***************** Correctness check passed **************************\n\n");
}


//This function returns the time needed for the rendering of a specified number of frames. The time for
//each frame is returned.
//To invoke this method is necessary to call the executable with the option -b <number of frames>. By default
//Also it is necessary to specify if the frames need to be handled by cpu or gpu using -r ref (defualt) or
//-r cuda.
//Finally with the option -f <filename> is specified the name of the frame(s). By default is "image".
//
//First example: ./render -b 3 -f my_file -r cuda rand10k   save 3 frames of rand10k by the name my_file,
//															created with cuda
//Second example: ./render -b 2 pattern 					save 2 frames of pattern by the name image, created
//															with the CPU
void
startBenchmark(
    CircleRenderer* renderer,
    const std::string& rendererType,
    int totalFrames,
    const std::string& frameFilename)
{

    double totalTime = 0.f;
    double startTime= 0.f;

    printf("\nRunning benchmark, %d frames...\n", totalFrames);

    printf("Dumping frames to %s_frameXXX_%s.ppm\n", frameFilename.c_str(), rendererType.c_str());

    for (int frame=0; frame<totalFrames; frame++) {

        if (frame == 0)
            startTime = CycleTimer::currentSeconds();

        double startClearTime = CycleTimer::currentSeconds();

        renderer->clearImage();

        double endClearTime = CycleTimer::currentSeconds();

        renderer->render();

        double endRenderTime = CycleTimer::currentSeconds();

        //saving the frame file
        char filename[1024];
        sprintf(filename, "%s_frame%d_%s.ppm", frameFilename.c_str(),frame,rendererType.c_str());
        writePPMImage(renderer->getImage(), filename);

        double endFileSaveTime = CycleTimer::currentSeconds();

        double clearTime = endClearTime - startClearTime;
        double renderTime = endRenderTime-endClearTime;
        double fileSaveTime = endFileSaveTime - endRenderTime;

        printf("Clear:    %.4f ms\n", 1000.f * clearTime);
		printf("Render:   %.4f ms\n", 1000.f * renderTime);
		printf("Total:    %.4f ms\n", 1000.f * (clearTime + renderTime));
		printf("File IO:  %.4f ms\n", 1000.f * fileSaveTime);
		printf("\n");

    }

    double endTime = CycleTimer::currentSeconds();
    totalTime = endTime - startTime;

    printf("\n");
    printf("Overall:  %.4f sec (note units are seconds)\n", totalTime);

}

//CheckBenchmark executes 10 frames both for cpu and gpu, and returns the rendering average time for both,
//allowing us to compare them.
//It is invokable executing the runnable with option -c.
//
//Example: ./render -c rand100k executes 10 frames of rand100k with cpu and cuda both and print the average
//								results
void
CheckBenchmark(
    CircleRenderer* ref_renderer,
    CircleRenderer* cuda_renderer,
    const std::string& frameFilename)
{

    double totalClearTime = 0.f;
    double totalRenderTime = 0.f;
    double totalCPUFileSaveTime = 0.f;
    double totalCudaFileSaveTime = 0.f;
    double totalTime = 0.f;
    double startTime= 0.f;

    printf("\nRunning benchmark with 10 frames, the result is an average of the results\n");

    printf("Dumping frames to %s_cpu.ppm and %s_cuda\n", frameFilename.c_str(), frameFilename.c_str());

    //first we compute the average time needed for the rendering of 10 frames with the CPU

    for (int frame=0; frame<10; frame++) {
    	if (frame==0)
    		startTime = CycleTimer::currentSeconds();

    	double startClearTime = CycleTimer::currentSeconds();
    	ref_renderer->clearImage();
    	double endClearTime = CycleTimer::currentSeconds();

    	double startRenderTime = CycleTimer::currentSeconds();
    	ref_renderer->render();
    	double endRenderTime = CycleTimer::currentSeconds();

    	//if we are handling the first frame, we have to save it
    	if(frame==0){
    		double startFileSaveTime = CycleTimer::currentSeconds();

    		char filename[1024];
			sprintf(filename, "%s_cpu.ppm", frameFilename.c_str());
			writePPMImage(ref_renderer->getImage(), filename);

			double endFileSaveTime = CycleTimer::currentSeconds();
			totalCPUFileSaveTime += endFileSaveTime - startFileSaveTime;
    	}
		totalClearTime += endClearTime - startClearTime;
		totalRenderTime += endRenderTime - startRenderTime;
    }
    double totalCPUClearTime =totalClearTime/10;
    double totalCPURenderTime = totalRenderTime/10;

    //re-initializations, this time for CUDA analysis
    totalClearTime = 0.f;
    totalRenderTime = 0.f;

    for (int frame=0; frame<10; frame++) {
    	if(frame==0)
    		startTime = CycleTimer::currentSeconds();

		double startClearTime = CycleTimer::currentSeconds();
		cuda_renderer->clearImage();
		double endClearTime = CycleTimer::currentSeconds();

		double startRenderTime = CycleTimer::currentSeconds();
		cuda_renderer->render();
		double endRenderTime = CycleTimer::currentSeconds();

		//if we are handling the first frame, we have to save it
		if(frame==0){
			double startFileSaveTime = CycleTimer::currentSeconds();

			char filename[1024];
			sprintf(filename, "%s_cuda.ppm", frameFilename.c_str());
			writePPMImage(cuda_renderer->getImage(), filename);

			double endFileSaveTime = CycleTimer::currentSeconds();
			totalCudaFileSaveTime += endFileSaveTime - startFileSaveTime;
		}
		totalClearTime += endClearTime - startClearTime;
		totalRenderTime += endRenderTime - startRenderTime;
    }
    double totalCudaClearTime =totalClearTime/10;
    double totalCudaRenderTime = totalRenderTime/10;

    //Comparing the 2 images
    compare_images(ref_renderer->getImage(), cuda_renderer->getImage());

	double endTime = CycleTimer::currentSeconds();
	totalTime = endTime - startTime;

	printf("CPU time:\n");
	printf("Clear:    %.4f ms\n", 1000.f * totalCPUClearTime);
	printf("Render:   %.4f ms\n", 1000.f * totalCPURenderTime);
	printf("Total:    %.4f ms\n", 1000.f * (totalCPUClearTime + totalCPURenderTime));
	printf("File IO:  %.4f ms\n", 1000.f * totalCPUFileSaveTime);

	printf("\n*********************************************************************\n\n");

	printf("CUDA time:\n");
	printf("Clear:    %.4f ms\n", 1000.f * totalCudaClearTime);
	printf("Render:   %.4f ms\n", 1000.f * totalCudaRenderTime);
	printf("Total:    %.4f ms\n", 1000.f * (totalCudaClearTime + totalCudaRenderTime));
	printf("File IO:  %.4f ms\n", 1000.f * totalCudaFileSaveTime);

	printf("\n");
	printf("Overall:  %.4f sec (note units are seconds)\n", totalTime);
	double speedup=totalCPURenderTime/totalCudaRenderTime;
	printf("Speedup: %.2fx\n", speedup);

}
