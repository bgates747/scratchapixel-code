//[header]
// This program implements a physical pinhole camera model similar to the model
// used in popular 3D packages such as Maya.
//[/header]
//[compile]
// Download the pinhole.cpp and geometry.h files to the same folder.
// Open a shell/terminal, and run the following command where the files are saved:
//
// c++ pinhole.cpp -o pinhole -std=c++11
//
// Run with: ./pinhole. Open the file ./pinhole.svg in any Internet browser to see
// the result.
//[/compile]
//[ignore]
// Copyright (C) 2012  www.scratchapixel.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//[/ignore]
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include "objimporter.h"

#include <fstream>
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

// Global variables to store OBJ data
std::vector<Vec3f> verts;
std::vector<uint32_t> tris;
std::vector<Vec3f> normals;
std::vector<uint32_t> normal_indices;
std::vector<Vec2f> uvs;
std::vector<uint32_t> uv_indices;

bool computePixelCoordinates(
    const Vec3f &pWorld,
    const Matrix44f &worldToCamera,
    const float &b,
    const float &l,
    const float &t,
    const float &r,
    const float &near,
    const uint32_t &imageWidth,
    const uint32_t &imageHeight,
    Vec2i &pRaster)
{
    Vec3f pCamera;
    worldToCamera.multVecMatrix(pWorld, pCamera);
    Vec2f pScreen;
    pScreen.x = pCamera.x / -pCamera.z * near;
    pScreen.y = pCamera.y / -pCamera.z * near;
    
    Vec2f pNDC;
    pNDC.x = (pScreen.x + r) / (2 * r);
    pNDC.y = (pScreen.y + t) / (2 * t);
    pRaster.x = (int)(pNDC.x * imageWidth);
    pRaster.y = (int)((1 - pNDC.y) * imageHeight);

    bool visible = true;
    if (pScreen.x < l || pScreen.x > r || pScreen.y < b || pScreen.y > t)
        visible = false;

    return visible;
}

float focalLength = 35; // in mm
float filmApertureWidth = 0.825;
float filmApertureHeight = 0.446;
static const float inchToMm = 25.4;
float nearClippingPlane = 0.1;
float farClipingPlane = 1000;
uint32_t imageWidth = 1600;
uint32_t imageHeight = 900;

enum FitResolutionGate { kFill = 0, kOverscan };
FitResolutionGate fitFilm = kOverscan;

int main(int argc, char **argv)
{
    // Load OBJ file using the new ParseObj function
    ObjData objData = ParseObj("jet.obj");

    // Set global variables with the parsed data
    verts = std::move(objData.vertices);
    tris = std::move(objData.face_indices);
    normals = std::move(objData.normals);
    normal_indices = std::move(objData.normal_indices);
    uvs = std::move(objData.uvs);
    uv_indices = std::move(objData.uv_indices);

    uint32_t numTris = tris.size() / 3;

    float filmAspectRatio = filmApertureWidth / filmApertureHeight;
    float deviceAspectRatio = imageWidth / (float)imageHeight;

    float top = ((filmApertureHeight * inchToMm / 2) / focalLength) * nearClippingPlane;
    float right = ((filmApertureWidth * inchToMm / 2) / focalLength) * nearClippingPlane;

    float xscale = 1;
    float yscale = 1;

    switch (fitFilm) {
        default:
        case kFill:
            if (filmAspectRatio > deviceAspectRatio) {
                xscale = deviceAspectRatio / filmAspectRatio;
            }
            else {
                yscale = filmAspectRatio / deviceAspectRatio;
            }
            break;
        case kOverscan:
            if (filmAspectRatio > deviceAspectRatio) {
                yscale = filmAspectRatio / deviceAspectRatio;
            }
            else {
                xscale = deviceAspectRatio / filmAspectRatio;
            }
            break;
    }

    right *= xscale;
    top *= yscale;

    float bottom = -top;
    float left = -right;
    
    printf("Screen window coordinates: %f %f %f %f\n", bottom, left, top, right);
    printf("Film Aspect Ratio: %f\nDevice Aspect Ratio: %f\n", filmAspectRatio, deviceAspectRatio);
    printf("Angle of view: %f (deg)\n", 2 * atan((filmApertureWidth * inchToMm / 2) / focalLength) * 180 / M_PI);

    std::ofstream ofs;
    ofs.open("./pinhole.svg");
    ofs << "<svg version=\"1.1\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" xmlns=\"http://www.w3.org/2000/svg\" width=\"" << imageWidth << "\" height=\"" << imageHeight << "\">" << std::endl;
    Matrix44f cameraToWorld(-0.95424, 0, 0.299041, 0, 0.0861242, 0.95763, 0.274823, 0, -0.28637, 0.288002, -0.913809, 0, -3.734612, 7.610426, -14.152769, 1);
    Matrix44f worldToCamera = cameraToWorld.inverse();
    float canvasWidth = 2, canvasHeight = 2;

    for (uint32_t i = 0; i < numTris; ++i) {
        if (tris[i * 3] >= verts.size() ||
            tris[i * 3 + 1] >= verts.size() ||
            tris[i * 3 + 2] >= verts.size()) {
            std::cerr << "Error: Triangle index out of bounds." << std::endl;
            exit(1);
        }

        const Vec3f &v0World = verts[tris[i * 3]];
        const Vec3f &v1World = verts[tris[i * 3 + 1]];
        const Vec3f &v2World = verts[tris[i * 3 + 2]];
        Vec2i v0Raster, v1Raster, v2Raster;

        bool visible = true;
        visible &= computePixelCoordinates(v0World, worldToCamera, bottom, left, top, right, nearClippingPlane, imageWidth, imageHeight, v0Raster);
        visible &= computePixelCoordinates(v1World, worldToCamera, bottom, left, top, right, nearClippingPlane, imageWidth, imageHeight, v1Raster);
        visible &= computePixelCoordinates(v2World, worldToCamera, bottom, left, top, right, nearClippingPlane, imageWidth, imageHeight, v2Raster);
        
        int val = visible ? 0 : 255;
        ofs << "<line x1=\"" << v0Raster.x << "\" y1=\"" << v0Raster.y << "\" x2=\"" << v1Raster.x << "\" y2=\"" << v1Raster.y << "\" style=\"stroke:rgb(" << val << ",0,0);stroke-width:1\" />\n";
        ofs << "<line x1=\"" << v1Raster.x << "\" y1=\"" << v1Raster.y << "\" x2=\"" << v2Raster.x << "\" y2=\"" << v2Raster.y << "\" style=\"stroke:rgb(" << val << ",0,0);stroke-width:1\" />\n";
        ofs << "<line x1=\"" << v2Raster.x << "\" y1=\"" << v2Raster.y << "\" x2=\"" << v0Raster.x << "\" y2=\"" << v0Raster.y << "\" style=\"stroke:rgb(" << val << ",0,0);stroke-width:1\" />\n";
    }

    ofs << "</svg>\n";
    ofs.close();
    
    return 0;
}
