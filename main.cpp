#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string>

#include "refRenderer.h"
#include "cudaRenderer.h"
#include "platformgl.h"


void startRendererWithDisplay(CircleRenderer* renderer);
void startBenchmark(CircleRenderer* renderer, const std::string& rendererType, int totalFrames, const std::string& frameFilename);
void CheckBenchmark(CircleRenderer* ref_renderer, CircleRenderer* cuda_renderer, const std::string& frameFilename);


void usage(const char* progname) {
    printf("Usage: %s [options] scenename\n", progname);
    printf("Valid scenenames are: rgb, rgby, rand10k, rand100k, pattern\n");
    printf("Program Options:\n");
    printf("  -b  --bench <NUM_OF_FRAMES>    Benchmark mode, do not create display. Shows time frames\n");
    printf("  -c  --check                Check correctness of output on one frame\n");
    printf("  -f  --file  <FILENAME>     Dump frames in benchmark mode (FILENAME_xxxx.ppm) for both CPU and GPU versions\n");
    printf("  -r  --renderer <ref/cuda>  Select renderer: ref or cuda\n");
    printf("  -?  --help                 This message\n");
}


int main(int argc, char** argv)
{

    int numberOfFrames = -1;
    int imageSize = 1024;

    std::string sceneNameStr;
    std::string frameFilename;
    SceneName sceneName;
    bool useRefRenderer = true;
    bool checkCorrectness = false;
    bool benchmarkMode= false;

    // parse commandline options ////////////////////////////////////////////
    int opt;
    static struct option long_options[] = {
        {"help",     0, 0,  '?'},
        {"check",    0, 0,  'c'},
        {"bench",    1, 0,  'b'},
        {"file",     1, 0,  'f'},
        {"renderer", 1, 0,  'r'},
        {0 ,0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "b:f:r:s:c?", long_options, NULL)) != EOF) {

        switch (opt) {
        case 'b':
            if (sscanf(optarg, "%d", &numberOfFrames) != 1) {
                fprintf(stderr, "Invalid argument to -b option\n");
                usage(argv[0]);
                exit(1);
            }
            benchmarkMode = true;
            break;
        case 'c':
            checkCorrectness = true;
            break;
        case 'f':
            frameFilename = optarg;
            break;
        case 'r':
            if (std::string(optarg).compare("cuda") == 0) {
                useRefRenderer = false;
            }
            break;
        case '?':
        default:
            usage(argv[0]);
            return 1;
        }
    }
    // end parsing of commandline options //////////////////////////////////////


    if (optind + 1 > argc) {
        fprintf(stderr, "Error: missing scene name\n");
        usage(argv[0]);
        return 1;
    }

    sceneNameStr = argv[optind];

    if (sceneNameStr.compare("rgb") == 0) {
        sceneName = CIRCLE_RGB;
    } else if (sceneNameStr.compare("rgby") == 0) {
        sceneName = CIRCLE_RGBY;
    } else if (sceneNameStr.compare("rand10k") == 0) {
        sceneName = CIRCLE_TEST_10K;
    } else if (sceneNameStr.compare("rand100k") == 0) {
        sceneName = CIRCLE_TEST_100K;
    } else if (sceneNameStr.compare("pattern") == 0) {
        sceneName = PATTERN;
    }else {
        fprintf(stderr, "Unknown scene name (%s)\n", sceneNameStr.c_str());
        usage(argv[0]);
        return 1;
    }

    printf("Rendering to %dx%d image\n", imageSize, imageSize);

    CircleRenderer* renderer;

    if (checkCorrectness) {
        // Need both the renderers

        CircleRenderer* ref_renderer;
        CircleRenderer* cuda_renderer;

        ref_renderer = new RefRenderer();
        cuda_renderer = new CudaRenderer();

        ref_renderer->allocOutputImage(imageSize, imageSize);
        ref_renderer->loadScene(sceneName);
        ref_renderer->setup();
        cuda_renderer->allocOutputImage(imageSize, imageSize);
        cuda_renderer->loadScene(sceneName);
        cuda_renderer->setup();

        // Check the correctness between 10 frames, and the average value in time is returned
        //setting a default name for the file to dump, if the name is not given through -f
        if(frameFilename=="")
        	frameFilename="image";

        // Check the correctness between 10 frames, and the average value in time is returned
        CheckBenchmark(ref_renderer, cuda_renderer, frameFilename);
    }
    else {

        if (useRefRenderer)
            renderer = new RefRenderer();
        else
            renderer = new CudaRenderer();

        renderer->allocOutputImage(imageSize, imageSize);
        renderer->loadScene(sceneName);
        renderer->setup();

        //If we are in benchmark mode we don't have to show the image, but to save it
        if (benchmarkMode && frameFilename!="")
        	if(useRefRenderer)
        		startBenchmark(renderer, "cpu" ,numberOfFrames, frameFilename);
        	else
        		startBenchmark(renderer, "cuda" ,numberOfFrames, frameFilename);
        //If we are in benchmark mode but we don't set a name for the file, we use the default "image"
        else if(benchmarkMode && frameFilename==""){
        	if(useRefRenderer)
        	    startBenchmark(renderer, "cpu" ,numberOfFrames, "image");
        	else
        	    startBenchmark(renderer, "cuda" ,numberOfFrames, "image");
        }
        //...not in benchmark mode, so we show the image on screen
        else{
        	glutInit(&argc, argv);
            startRendererWithDisplay(renderer);
        }
    }

    return 0;
}
