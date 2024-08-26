#pragma once

#include "_types.h"
#include "geometry.h"
#include "mesh.h"

struct Object {
    struct Mesh* mesh;
    struct point3f position;
    struct vec3f rotation;
    struct vec3f scale;
    Matrix44f transform;
};

void object_init(struct Object* object, struct Mesh* mesh, struct point3f position, struct vec3f rotation, struct vec3f scale) {
    object->mesh = mesh;
    object->position = position;
    object->rotation = rotation;
    object->scale = scale;
    object->transform = matrix_identity();
}