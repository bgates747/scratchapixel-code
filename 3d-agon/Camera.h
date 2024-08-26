// Camera.h
#pragma once

#include "geometry.h"

class Camera {
public:
    Camera(const Vec3f& position, float fov, float nearClip, float farClip, float aspectRatio)
        : position(position), fov(fov), nearClippingPlane(nearClip), farClippingPlane(farClip), aspectRatio(aspectRatio) {}

    Matrix44f getWorldToCameraMatrix() const {
        // Define the camera to world transformation matrix
        Matrix44f cameraToWorld(1, 0, 0, 0,
                                0, 1, 0, 0,
                                0, 0, 1, 0,
                                position.x, position.y, position.z, 1);
        return cameraToWorld.inverse();
    }

    float getNearClip() const {
        return nearClippingPlane;
    }

    float getFarClip() const {
        return farClippingPlane;
    }

    float getFov() const {
        return fov;
    }

    float getAspectRatio() const {
        return aspectRatio;
    }

private:
    Vec3f position;
    float fov;
    float nearClippingPlane;
    float farClippingPlane;
    float aspectRatio;
};