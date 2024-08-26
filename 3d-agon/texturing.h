// Copyright (C) 2024 www.scratchapixel.com
// Distributed under the terms of the CC BY-NC-ND 4.0 License.
// https://creativecommons.org/licenses/by-nc-nd/4.0/
// clang -std=c11 -o texturing.exe  texturing.c -O3

#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h> // For fminf and fmaxf
#include <algorithm> // For std::max and std::min

#include "objimporter.h"
#include "Camera.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Define the necessary structs
struct point2i { int x, y; };
struct point3f { float x, y, z; };
struct texcoord2f { float s, t; };

struct image {
    int width;
    int height;
    unsigned char* data; // rgba2222 format
};

struct shader {
    struct image* image_ptr;
};

struct mesh {
    struct point3f* points;
    int* face_vertex_indices;
    struct {
        struct texcoord2f* coords;
        int* indices;
    } st;
    int num_triangles;
    struct shader* shader;
};

struct extent {
    int width;
    int height;
};

struct screen_coordinates {
    float r, l, t, b;
};

struct context {
    struct extent extent;
    float focal_length;
    struct aperture {
        float width;
        float height;
    } aperture;
    float znear, zfar;
    struct screen_coordinates screen_coordinates;
    float* depth_buffer;
    unsigned char* color_buffer; // rgba2222 format
    float world_to_cam[16];
};

static inline void point_mat_mult(const struct point3f* const p, const float* m, struct point3f* xp) {
    // Perform the matrix multiplication
    xp->x = m[0] * p->x + m[4] * p->y + m[8] * p->z + m[12];
    xp->y = m[1] * p->x + m[5] * p->y + m[9] * p->z + m[13];
    xp->z = m[2] * p->x + m[6] * p->y + m[10] * p->z + m[14];
}

void set_texture(struct image* const image, const char* filename, int width, int height) {
    // Open the file in binary read mode
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open texture file %s\n", filename);
        return;
    }

    // Set image dimensions
    image->width = width;
    image->height = height;

    // Allocate memory for the image data (1 byte per pixel in RGBA2222 format)
    image->data = (unsigned char*)malloc(width * height);
    if (!image->data) {
        fprintf(stderr, "Error: Unable to allocate memory for texture data\n");
        fclose(file);
        return;
    }

    // Read the texture data from the file in one go
    size_t bytesRead = fread(image->data, 1, width * height, file);
    if (bytesRead != (size_t)(width * height)) {
        fprintf(stderr, "Error: Unexpected end of file or file size mismatch\n");
        free(image->data);
        image->data = NULL;  // Avoid dangling pointer
        fclose(file);
        return;
    }

    // Close the file after reading
    fclose(file);
}

static void create_mesh(struct context* const context, struct mesh* const mesh, const ObjData& objData, const char* textureFilename, int textureWidth, int textureHeight) {
    // Determine the number of vertices and triangles
    size_t num_vertices = objData.vertices.size();
    size_t num_faces = objData.face_indices.size();
    mesh->num_triangles = num_faces / 3;

    // Allocate memory for points and transform them in place
    mesh->points = (struct point3f*)malloc(num_vertices * sizeof(struct point3f));
    if (!mesh->points) {
        fprintf(stderr, "Error: Unable to allocate memory for mesh points\n");
        return;
    }

    for (size_t i = 0; i < num_vertices; ++i) {
        struct point3f temp_point = { objData.vertices[i].x, objData.vertices[i].y, objData.vertices[i].z };
        point_mat_mult(&temp_point, context->world_to_cam, &mesh->points[i]);
    }

    // Allocate memory for face vertex indices and copy data
    mesh->face_vertex_indices = (int*)malloc(num_faces * sizeof(int));
    if (!mesh->face_vertex_indices) {
        fprintf(stderr, "Error: Unable to allocate memory for face vertex indices\n");
        free(mesh->points);
        return;
    }
    memcpy(mesh->face_vertex_indices, objData.face_indices.data(), num_faces * sizeof(int));

    // Allocate and copy texture coordinates and indices, if available
    if (!objData.uvs.empty()) {
        size_t num_uvs = objData.uvs.size();
        mesh->st.coords = (struct texcoord2f*)malloc(num_uvs * sizeof(struct texcoord2f));
        if (!mesh->st.coords) {
            fprintf(stderr, "Error: Unable to allocate memory for texture coordinates\n");
            free(mesh->points);
            free(mesh->face_vertex_indices);
            return;
        }

        for (size_t i = 0; i < num_uvs; ++i) {
            mesh->st.coords[i].s = objData.uvs[i].x;
            mesh->st.coords[i].t = objData.uvs[i].y;
        }

        size_t num_uv_indices = objData.uv_indices.size();
        mesh->st.indices = (int*)malloc(num_uv_indices * sizeof(int));
        if (!mesh->st.indices) {
            fprintf(stderr, "Error: Unable to allocate memory for texture indices\n");
            free(mesh->points);
            free(mesh->face_vertex_indices);
            free(mesh->st.coords);
            return;
        }
        memcpy(mesh->st.indices, objData.uv_indices.data(), num_uv_indices * sizeof(int));
    } else {
        mesh->st.coords = nullptr;
        mesh->st.indices = nullptr;
    }

    // Initialize shader and load the texture
    mesh->shader = (struct shader*)malloc(sizeof(struct shader));
    if (!mesh->shader) {
        fprintf(stderr, "Error: Unable to allocate memory for shader\n");
        free(mesh->points);
        free(mesh->face_vertex_indices);
        free(mesh->st.coords);
        free(mesh->st.indices);
        return;
    }

    mesh->shader->image_ptr = (struct image*)malloc(sizeof(struct image));
    if (!mesh->shader->image_ptr) {
        fprintf(stderr, "Error: Unable to allocate memory for shader image\n");
        free(mesh->shader);
        free(mesh->points);
        free(mesh->face_vertex_indices);
        free(mesh->st.coords);
        free(mesh->st.indices);
        return;
    }

    mesh->shader->image_ptr->data = nullptr;
    set_texture(mesh->shader->image_ptr, textureFilename, textureWidth, textureHeight);
}

static void destroy_mesh(struct mesh* mesh) {
	free(mesh->points);
	free(mesh->face_vertex_indices);
	free(mesh->st.coords);
	free(mesh->st.indices);
	free(mesh->shader->image_ptr->data);
	free(mesh->shader->image_ptr);
	free(mesh->shader);
}

static void context_init(struct context* context, const Camera& camera) {
    context->extent.width = context->extent.width;  // Keep window width
    context->extent.height = context->extent.height;  // Keep window height
    
    // Set the near and far clipping planes based on the camera's values
    context->znear = camera.getNearClip();
    context->zfar = camera.getFarClip();

    // Calculate the screen coordinates based on the camera's FOV and aspect ratio
    float aspectRatio = camera.getAspectRatio();
    float fovRadians = camera.getFov() * M_PI / 180.0f;
    float tanHalfFov = tanf(fovRadians / 2.0f);

    context->screen_coordinates.t = context->znear * tanHalfFov;
    context->screen_coordinates.r = context->screen_coordinates.t * aspectRatio;
    context->screen_coordinates.l = -context->screen_coordinates.r;
    context->screen_coordinates.b = -context->screen_coordinates.t;

    // Set the world-to-camera matrix using the camera's transformation
    Matrix44f worldToCamera = camera.getWorldToCameraMatrix();
    memcpy(context->world_to_cam, &worldToCamera, sizeof(worldToCamera));
}

static void prepare_buffers(struct context* context) {
    int array_size = context->extent.width * context->extent.height;

    // Allocate memory for depth buffer
    context->depth_buffer = (float*)malloc(sizeof(float) * array_size);
    if (!context->depth_buffer) {
        fprintf(stderr, "Error: Unable to allocate memory for depth buffer\n");
        return;
    }

    // Allocate memory for color buffer (RGBA2222 format, 1 byte per pixel)
    context->color_buffer = (unsigned char*)malloc(array_size);
    if (!context->color_buffer) {
        fprintf(stderr, "Error: Unable to allocate memory for color buffer\n");
        free(context->depth_buffer);
        return;
    }

    // Initialize color buffer to 0x00 (transparent)
    memset(context->color_buffer, 0x00, array_size);

    // Initialize depth buffer to the far plane value
    for (int i = 0; i < array_size; ++i) {
        context->depth_buffer[i] = context->zfar;
    }
}

static inline void persp_divide(struct point3f* p, const float znear) {
    float inv_z = 1.0f / -p->z;  // Calculate the inverse of z once
    p->x = p->x * inv_z * znear;
    p->y = p->y * inv_z * znear;
    p->z = -p->z;
}

static inline void to_raster(const struct screen_coordinates coords, const struct extent extent, struct point3f* const p) {
    float inv_width = 1.0f / (coords.r - coords.l);
    float inv_height = 1.0f / (coords.t - coords.b);

    p->x = 2 * p->x * inv_width - (coords.r + coords.l) * inv_width;
    p->y = 2 * p->y * inv_height - (coords.t + coords.b) * inv_height;

    p->x = (p->x + 1) * 0.5f * extent.width;
    p->y = (1 - p->y) * 0.5f * extent.height;
}
static inline void tri_bbox(const struct point3f* const p0, 
                            const struct point3f* const p1, 
                            const struct point3f* const p2, 
                            float* const bbox) {
    bbox[0] = MIN(MIN(p0->x, p1->x), p2->x);
    bbox[1] = MIN(MIN(p0->y, p1->y), p2->y);
    bbox[2] = MAX(MAX(p0->x, p1->x), p2->x);
    bbox[3] = MAX(MAX(p0->y, p1->y), p2->y);
}

static inline float edge(const struct point3f* const a, const struct point3f* const b, const struct point3f* const test) {
    return (test->x - a->x) * (b->y - a->y) - (test->y - a->y) * (b->x - a->x);
}

static void shade(const struct shader* shader, struct texcoord2f st, unsigned char* ci) {
    if (shader->image_ptr != NULL) {
        const struct image* const image = shader->image_ptr;
        float s = st.s;
        float t = st.t;

        // Convert normalized coordinates to texel coordinates
        struct point2i texel;
		texel.x = (int)fminf(s * image->width, image->width - 1);
		texel.y = (int)fminf(t * image->height, image->height - 1);

        // Get the color from the texture at the texel position
        unsigned char texel_color = image->data[texel.y * image->width + texel.x]; // rgba2222 format

        // Store the color in the destination buffer
        *ci = texel_color;
    }
}

static inline void rasterize(int x0, int y0, int x1, int y1, 
                             const struct point3f* const p0, const struct point3f* const p1, const struct point3f* const p2, 
                             const struct texcoord2f* const st0, const struct texcoord2f* const st1, const struct texcoord2f* const st2,
                             const struct mesh* const mesh,
                             struct context* context) {
    float inv_area = 1.0f / edge(p0, p1, p2);  // Precompute the inverse of the area
    struct point3f pixel, sample;
    pixel.y = y0;

    for (int j = y0, row = y0 * context->extent.width; j <= y1; ++j, pixel.y += 1, row += context->extent.width) {
        pixel.x = x0;
        for (int i = x0, index = row + x0; i <= x1; ++i, pixel.x += 1, ++index) {
            sample.x = pixel.x + 0.5f;
            sample.y = pixel.y + 0.5f;

            float w0 = edge(p1, p2, &sample) * inv_area;
            float w1 = edge(p2, p0, &sample) * inv_area;
            float w2 = edge(p0, p1, &sample) * inv_area;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                float one_over_z = w0 / p0->z + w1 / p1->z + w2 / p2->z;
                float z = 1.0f / one_over_z;

                // Z-buffer test
                if (z < context->depth_buffer[index]) {
                    context->depth_buffer[index] = z;

                    // Interpolate the texture coordinates
                    struct texcoord2f st;
                    st.s = (st0->s * w0 + st1->s * w1 + st2->s * w2) * z;
                    st.t = (st0->t * w0 + st1->t * w1 + st2->t * w2) * z;

                    // Shade the pixel and update the color buffer
                    shade(mesh->shader, st, &context->color_buffer[index]);
                }
            }
        }
    }
}

void render(struct context* context, int num_meshes, const struct mesh** const meshes) {
    float bbox[4];
    int x0, x1, y0, y1;

    for (int i = 0; i < num_meshes; ++i) {
        const struct mesh* const mesh = meshes[i];
        const int* vi = mesh->face_vertex_indices;
        const int* sti = mesh->st.indices;

        for (int j = 0; j < mesh->num_triangles; ++j, vi += 3, sti += 3) {
            struct point3f p0 = mesh->points[vi[0]];
            struct point3f p1 = mesh->points[vi[1]];
            struct point3f p2 = mesh->points[vi[2]];

            persp_divide(&p0, context->znear);
            persp_divide(&p1, context->znear);
            persp_divide(&p2, context->znear);
            to_raster(context->screen_coordinates, context->extent, &p0);
            to_raster(context->screen_coordinates, context->extent, &p1);
            to_raster(context->screen_coordinates, context->extent, &p2);

            tri_bbox(&p0, &p1, &p2, bbox);

            if (bbox[0] > context->extent.width - 1 || bbox[2] < 0 || bbox[1] > context->extent.height - 1 || bbox[3] < 0)
                continue;

            x0 = MAX(0, (int)bbox[0]);
            y0 = MAX(0, (int)bbox[1]);
            x1 = MIN(context->extent.width - 1, (int)bbox[2]);
            y1 = MIN(context->extent.height - 1, (int)bbox[3]);

            struct texcoord2f st0 = mesh->st.coords[sti[0]];
            struct texcoord2f st1 = mesh->st.coords[sti[1]];
            struct texcoord2f st2 = mesh->st.coords[sti[2]];

            st0.s /= p0.z;
            st0.t /= p0.z;
            st1.s /= p1.z;
            st1.t /= p1.z;
            st2.s /= p2.z;
            st2.t /= p2.z;

            rasterize(x0, y0, x1, y1, &p0, &p1, &p2, &st0, &st1, &st2, mesh, context);
        }
    }
}

void cleanup(struct context* context) {
	free(context->depth_buffer);
}