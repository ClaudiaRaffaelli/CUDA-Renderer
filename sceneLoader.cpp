
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <functional>

#include "sceneLoader.h"
#include "util.h"

// randomFloat --
// //
// // return a random floating point value between 0 and 1
static float
randomFloat() {
    return static_cast<float>(rand()) / RAND_MAX;
}

static void
makeCircleGrid(
    int startIndex,
    int circleCount,
    float circleRadius,
    float circleColor[3],
    float startOffsetX,
    float startOffsetY,
    float* position,
    float* color,
    float* radius)
{

    int index = startIndex;
    for (int j=0; j<circleCount; j++) {
        for (int i=0; i<circleCount; i++) {
            int index3 = 3 * index;
            float x = startOffsetX + (2.f * circleRadius * i);
            float y = startOffsetY + (2.f * circleRadius * j);
            position[index3] = x;
            position[index3+1] = y;
            position[index3+2] = randomFloat();
            color[index3] = circleColor[0];
            color[index3+1] = circleColor[1];
            color[index3+2] = circleColor[2];
            radius[index] =  circleRadius;
            index++;
        }
    }
}

static void
generateRandomCircles(
    int numCircles,
    float* position,
    float* color,
    float* radius) {

    srand(0);
    std::vector<float> depths(numCircles);
    for (int i=0; i<numCircles; i++) {
        depths[i] = randomFloat();
    }

    std::sort(depths.begin(), depths.end(),  std::greater<float>());

    for (int i=0; i<numCircles; i++) {

        float depth = depths[i];

        radius[i] = .02f + .06f * randomFloat();

        int index3 = 3 * i;

        position[index3] = randomFloat();
        position[index3+1] = randomFloat();
        position[index3+2] = depth;

        if (numCircles <= 10000) {
            color[index3] = .1f + .9f * randomFloat();
            color[index3+1] = .2f + .5f * randomFloat();
            color[index3+2] = .5f + .5f * randomFloat();
        } else {
            color[index3] = .3f + .9f * randomFloat();
            color[index3+1] = .1f + .9f * randomFloat();
            color[index3+2] = .1f + .4f * randomFloat();
        }
    }
}

static void
generateSizeCircles(
    int numCircles,
    float* position,
    float* color,
    float* radius,
    float targetR) {

    srand(0);
    std::vector<float> depths(numCircles);
    for (int i=0; i<numCircles; i++) {
        depths[i] = randomFloat();
    }

    std::sort(depths.begin(), depths.end(),  std::greater<float>());

    for (int i=0; i<numCircles; i++) {

        float depth = depths[i];

        radius[i] = targetR;

        int index3 = 3 * i;

        position[index3] = randomFloat(); //targetR + (1.f - targetR) * randomFloat();
        position[index3+1] = randomFloat(); //targetR + (1.f - targetR) * randomFloat();
        position[index3+2] = depth;

        if (numCircles <= 10000) {
            color[index3] = .1f + .9f * randomFloat();
            color[index3+1] = .2f + .5f * randomFloat();
            color[index3+2] = .5f + .5f * randomFloat();
        } else {
            color[index3] = .3f + .9f * randomFloat();
            color[index3+1] = .1f + .9f * randomFloat();
            color[index3+2] = .1f + .4f * randomFloat();
        }
    }
}

static void
changeCircles(
    int numCircles,
    float* position,
    float* radius,
    float targetR,
    float center,
    float div
    ) {

    for (int i=0; i<numCircles; i++) {

        radius[i] = targetR;

        int index3 = 3 * i;

        position[index3] = .9f - center + div * randomFloat();
        position[index3+1] = center + div * randomFloat();
    }
}

void
loadCircleScene(
    SceneName sceneName,
    int& numCircles,
    float*& position,
    float*& color,
    float*& radius)
{

	if (sceneName == CIRCLE_RGB) {

        // simple test scene containing 3 circles. All circles have
        // 50% opacity
        //
        // farthest circle is red.  Middle is green.  Closest is blue.

        numCircles = 3;

        position = new float[3 * numCircles];
        color = new float[3 * numCircles];
        radius = new float[numCircles];

        for (int i=0; i<numCircles; i++)
            radius[i] = .3f;

        position[0] = .4f;
        position[1] = .5f;
        position[2] = .75f;
        color[0] = 1.f;
        color[1] = 0.f;
        color[2] = 0.f;

        position[3] = .5f;
        position[4] = .5f;
        position[5] = .5f;
        color[3] = 0.f;
        color[4] = 1.f;
        color[5] = 0.f;

        position[6] = .6f;
        position[7] = .5f;
        position[8] = .25f;
        color[6] = 0.f;
        color[7] = 0.f;
        color[8] = 1.f;

    } else if (sceneName == CIRCLE_RGBY) {

        // Another simple test scene containing 4 circles

        numCircles = 4;

        position = new float[3 * numCircles];
        color = new float[3 * numCircles];
        radius = new float[numCircles];

        const float TINY_RADIUS = .1f;
        const float SMALL_RADIUS = .19f;
        const float BIG_RADIUS = .25f;

        radius[0] = SMALL_RADIUS;
        radius[1] = SMALL_RADIUS;
        radius[2] = BIG_RADIUS;
        radius[3] = TINY_RADIUS;

        position[0] = .25f;
        position[1] = .25f;
        position[2] = .75f;
        color[0] = 1.f;
        color[1] = 0.f;
        color[2] = 0.f;

        position[3] = .3f;
        position[4] = .3f;
        position[5] = .5f;
        color[3] = 0.f;
        color[4] = 1.f;
        color[5] = 0.f;

        position[6] = .5f;
        position[7] = .5f;
        position[8] = .25f;
        color[6] = 0.f;
        color[7] = 0.f;
        color[8] = 1.f;

        position[9] = .2f;
        position[10] = .2f;
        position[11] = .9f;
        color[9] = 1.f;
        color[10] = 1.f;
        color[11] = 0.f;

    } else if (sceneName == CIRCLE_TEST_10K) {

        // test scene containing 10K randomly placed circles

        numCircles = 10 * 1000;

        position = new float[3 * numCircles];
        color = new float[3 * numCircles];
        radius = new float[numCircles];

        generateRandomCircles(numCircles, position, color, radius);

    } else if (sceneName == CIRCLE_TEST_100K) {

        // test scene containing 100K randomly placed circles

        numCircles = 100 * 1000;

        position = new float[3 * numCircles];
        color = new float[3 * numCircles];
        radius = new float[numCircles];

        generateRandomCircles(numCircles, position, color, radius);

    } else if (sceneName == PATTERN) {

        int circleCount1 = 16;
        int circleCount2 = 31;
        numCircles = circleCount1 * circleCount1;
        numCircles += circleCount2 * circleCount2;

        position = new float[3 * numCircles];
        color = new float[3 * numCircles];
        radius = new float[numCircles];

        int startIndex = 0;
        float circleRadius = .5f * (1.f / circleCount1);
        float startOffsetX = circleRadius;
        float startOffsetY = circleRadius;
        float circleColor[3];

        circleColor[0] = 1.f; circleColor[1] = 0.f; circleColor[2] = 0.f;
        makeCircleGrid(startIndex, circleCount1, circleRadius, circleColor, startOffsetX, startOffsetY, position, color, radius);

        startIndex += circleCount1 * circleCount1;
        startOffsetX = 0.f;
        startOffsetY = 0.f;
        circleColor[0] = 1.f; circleColor[1] = 1.f; circleColor[2] = .0f;
        makeCircleGrid(startIndex, circleCount2, circleRadius, circleColor, startOffsetX, startOffsetY, position, color, radius);
    } else {
        fprintf(stderr, "Error: cann't load scene (unknown scene)\n");
        return;
    }

    printf("Loaded scene with %d circles\n", numCircles);
}
