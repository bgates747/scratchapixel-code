#pragma once

#include "_types.h"

struct Mesh {
    struct point3f* vertices;
    int* vertex_indices;
    struct vec3f* normals;
    int* normal_indices;
    struct uv2f* uvs;
    int* uv_indices;
    int num_triangles;
    struct texture* texture;
};