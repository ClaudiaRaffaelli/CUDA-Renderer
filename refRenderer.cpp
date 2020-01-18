#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <vector>

#include "refRenderer.h"
#include "image.h"
#include "sceneLoader.h"
#include "util.h"

RefRenderer::RefRenderer() {
    image = NULL;

    numCircles = 0;
    position = NULL;
    color = NULL;
    radius = NULL;
}

RefRenderer::~RefRenderer() {

    if (image) {
        delete image;
    }

    if (position) {
        delete [] position;
        delete [] color;
        delete [] radius;
    }
}

const Image*
RefRenderer::getImage() {
    return image;
}

void
RefRenderer::setup() {
    // nothing to do here
}

// allocOutputImage --
//
// Allocate buffer the renderer will render into.  Check status of
// image first to avoid memory leak.
void
RefRenderer::allocOutputImage(int width, int height) {

    if (image)
        delete image;
    image = new Image(width, height);
}

// clearImage --
//
// Clear's the renderer's target image.  The state of the image after
// the clear depends on the scene being rendered.
void
RefRenderer::clearImage() {

    image->clear(1.f, 1.f, 1.f, 1.f);

}

void
RefRenderer::loadScene(SceneName scene) {
    sceneName = scene;
    loadCircleScene(sceneName, numCircles, position, color, radius);
}


// shadePixel --
//
// Computes the contribution of the specified circle to the
// given pixel.  All values are provided in normalized space, where
// the screen spans [0,2]^2.  The color/opacity of the circle is
// computed at the pixel center.
void
RefRenderer::shadePixel(
    int circleIndex,
    float pixelCenterX, float pixelCenterY,
    float px, float py, float pz,
    float* pixelData)
{
    float diffX = px - pixelCenterX;
    float diffY = py - pixelCenterY;
    float pixelDist = diffX * diffX + diffY * diffY;

    float rad = radius[circleIndex];
    float maxDist = rad * rad;

    // circle does not contribute to the image
    if (pixelDist > maxDist)
        return;

    float colR, colG, colB;
    float alpha;

    // there is a non-zero contribution.  Now compute the shading

    // simple: each circle has an assigned color
    int index3 = 3 * circleIndex;
    colR = color[index3];
    colG = color[index3+1];
    colB = color[index3+2];
    alpha = .5f;


    // The following code is *very important*: it blends the
    // contribution of the circle primitive with the current state
    // of the output image pixel.  This is a read-modify-write
    // operation on the image, and it needs to be atomic.  Moreover,
    // (and even more challenging) all writes to this pixel must be
    // performed in same order as when the circles are processed
    // serially.
    //
    // That is, if circle 1 and circle 2 both write to pixel P.
    // circle 1's contribution *must* be blended in first, then
    // circle 2's.  If this invariant is not preserved, the
    // rendering of transparent circles will not be correct.

    float oneMinusAlpha = 1.f - alpha;
    pixelData[0] = alpha * colR + oneMinusAlpha * pixelData[0];
    pixelData[1] = alpha * colG + oneMinusAlpha * pixelData[1];
    pixelData[2] = alpha * colB + oneMinusAlpha * pixelData[2];
    pixelData[3] += alpha;
}

void
RefRenderer::render() {

    // render all circles
    for (int circleIndex=0; circleIndex<numCircles; circleIndex++) {

        int index3 = 3 * circleIndex;

        float px = position[index3];
        float py = position[index3+1];
        float pz = position[index3+2];
        float rad = radius[circleIndex];

        // compute the bounding box of the circle.  This bounding box
        // is in normalized coordinates
        float minX = px - rad;
        float maxX = px + rad;
        float minY = py - rad;
        float maxY = py + rad;

        // convert normalized coordinate bounds to integer screen
        // pixel bounds.  Clamp to the edges of the screen.
        int screenMinX = CLAMP(static_cast<int>(minX * image->width), 0, image->width);
        int screenMaxX = CLAMP(static_cast<int>(maxX * image->width)+1, 0, image->width);
        int screenMinY = CLAMP(static_cast<int>(minY * image->height), 0, image->height);
        int screenMaxY = CLAMP(static_cast<int>(maxY * image->height)+1, 0, image->height);

        float invWidth = 1.f / image->width;
        float invHeight = 1.f / image->height;

        // for each pixel in the bounding box, determine the circle's
        // contribution to the pixel.  The contribution is computed in
        // the function shadePixel.  Since the circle does not fill
        // the bounding box entirely, not every pixel in the box will
        // receive contribution.
        for (int pixelY=screenMinY; pixelY<screenMaxY; pixelY++) {

            // pointer to pixel data
            float* imgPtr = &image->data[4 * (pixelY * image->width + screenMinX)];

            for (int pixelX=screenMinX; pixelX<screenMaxX; pixelX++) {

                // When "shading" the pixel ("shading" = computing the
                // circle's color and opacity at the pixel), we treat
                // the pixel as a point at the center of the pixel.
                // We'll compute the color of the circle at this
                // point.  Note that shading math will occur in the
                // normalized [0,1]^2 coordinate space, so we convert
                // the pixel center into this coordinate space prior
                // to calling shadePixel.
                float pixelCenterNormX = invWidth * (static_cast<float>(pixelX) + 0.5f);
                float pixelCenterNormY = invHeight * (static_cast<float>(pixelY) + 0.5f);
                shadePixel(circleIndex, pixelCenterNormX, pixelCenterNormY, px, py, pz, imgPtr);
                imgPtr += 4;
            }
        }
    }
}

void RefRenderer::dumpParticles(const char* filename) {

    FILE* output = fopen(filename, "w");

    fprintf(output, "%d\n", numCircles);
    for (int i=0; i<numCircles; i++) {
        fprintf(output, "%f %f %f     %f\n",
                position[3*i+0], position[3*i+1], position[3*i+2],
                radius[i]);
    }
    fclose(output);

}
