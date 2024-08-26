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
    float x = m[0] * p->x + m[4] * p->y + m[8] * p->z + m[12];
    float y = m[1] * p->x + m[5] * p->y + m[9] * p->z + m[13];
    float z = m[2] * p->x + m[6] * p->y + m[10] * p->z + m[14];
    xp->x = x; 
    xp->y = y; 
    xp->z = z;
}

void set_texture(struct image* const image, const char* filename, int width, int height) {
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

    // Read the data from the file
    size_t bytesRead = fread(image->data, 1, width * height, file);
    if (bytesRead != width * height) {
        fprintf(stderr, "Error: Unexpected end of file or file size mismatch\n");
        free(image->data);
        fclose(file);
        return;
    }

    fclose(file);
}

static void create_mesh(struct context* const context, struct mesh* const mesh, const ObjData& objData, const char* textureFilename, int textureWidth, int textureHeight) {
    // Populate the number of points and triangles
    mesh->num_triangles = objData.face_indices.size() / 3;
    mesh->points = (struct point3f*)malloc(objData.vertices.size() * sizeof(struct point3f));

    // Copy the vertices
    for (size_t i = 0; i < objData.vertices.size(); ++i) {
        mesh->points[i].x = objData.vertices[i].x;
        mesh->points[i].y = objData.vertices[i].y;
        mesh->points[i].z = objData.vertices[i].z;

        // Transform vertices by the world-to-camera matrix
        point_mat_mult(&mesh->points[i], context->world_to_cam, &mesh->points[i]);
    }

    // Allocate memory for face vertex indices
    mesh->face_vertex_indices = (int*)malloc(objData.face_indices.size() * sizeof(int));
    memcpy(mesh->face_vertex_indices, objData.face_indices.data(), objData.face_indices.size() * sizeof(int));

    // Allocate memory and copy texture coordinates and indices, if they exist
    if (!objData.uvs.empty()) {
        mesh->st.coords = (struct texcoord2f*)malloc(objData.uvs.size() * sizeof(struct texcoord2f));
        for (size_t i = 0; i < objData.uvs.size(); ++i) {
            mesh->st.coords[i].s = objData.uvs[i].x;
            mesh->st.coords[i].t = objData.uvs[i].y;
        }

        mesh->st.indices = (int*)malloc(objData.uv_indices.size() * sizeof(int));
        memcpy(mesh->st.indices, objData.uv_indices.data(), objData.uv_indices.size() * sizeof(int));
    } else {
        mesh->st.coords = nullptr;
        mesh->st.indices = nullptr;
    }

    // Initialize shader and set texture
    mesh->shader = (struct shader*)malloc(sizeof(struct shader));
    mesh->shader->image_ptr = (struct image*)malloc(sizeof(struct image));
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

    context->depth_buffer = (float*)std::malloc(sizeof(float) * array_size);
    context->color_buffer = (unsigned char*)std::malloc(array_size * 4); // rgba2222 format

    for (int i = 0; i < array_size; ++i) {
        memset(&context->color_buffer[i * 4], 0x00, 4); // Initialize to transparent
        context->depth_buffer[i] = context->zfar;
    }
}

static inline void persp_divide(struct point3f* p, const float znear) {
    p->x = p->x / -p->z * znear;
    p->y = p->y / -p->z * znear;
    p->z = -p->z;
}

static inline void to_raster(const struct screen_coordinates coords, const struct extent extent, struct point3f* const p) {
    p->x = 2 * p->x / (coords.r - coords.l) - (coords.r + coords.l) / (coords.r - coords.l);
    p->y = 2 * p->y / (coords.t - coords.b) - (coords.t + coords.b) / (coords.t - coords.b);

    p->x = (p->x + 1) / 2 * extent.width;
    p->y = (1 - p->y) / 2 * extent.height;
}

static inline void tri_bbox(const struct point3f* const p0, 
                            const struct point3f* const p1, 
                            const struct point3f* const p2, 
                            float* const bbox) {
    bbox[0] = std::fmin(std::fmin(p0->x, p1->x), p2->x); // Min x-coordinate
    bbox[1] = std::fmin(std::fmin(p0->y, p1->y), p2->y); // Min y-coordinate
    bbox[2] = std::fmax(std::fmax(p0->x, p1->x), p2->x); // Max x-coordinate
    bbox[3] = std::fmax(std::fmax(p0->y, p1->y), p2->y); // Max y-coordinate
}

static inline float edge(const struct point3f* const a, const struct point3f* const b, const struct point3f* const test) {
    return (test->x - a->x) * (b->y - a->y) - (test->y - a->y) * (b->x - a->x);
}

static void shade(const struct shader* shader, struct texcoord2f st, unsigned char* ci) {
    if (shader->image_ptr != NULL) {
        const struct image* const image = shader->image_ptr;
        
        // Wrap texture coordinates to [0, 1] range
        float s = st.s - std::floor(st.s);
        float t = st.t - std::floor(st.t);

        // Convert normalized coordinates to texel coordinates
        struct point2i texel;
        texel.x = static_cast<int>(s * (image->width - 1));
        texel.y = static_cast<int>((t) * (image->height - 1));

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
    float area = edge(p0, p1, p2);
    struct point3f pixel, sample;
    pixel.y = y0;

    for (int j = y0, row = y0 * context->extent.width; j <= y1; ++j, pixel.y += 1, row += context->extent.width) {
        pixel.x = x0;
        for (int i = x0, index = row + x0; i <= x1; ++i, pixel.x += 1, ++index) {
            sample.x = pixel.x + 0.5f;
            sample.y = pixel.y + 0.5f;

            float w0 = edge(p1, p2, &sample);
            float w1 = edge(p2, p0, &sample);
            float w2 = edge(p0, p1, &sample);

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                w0 /= area, w1 /= area, w2 /= area;
                float one_over_z = w0 / p0->z + w1 / p1->z + w2 / p2->z;
                float z = 1 / one_over_z;

                // Z-buffer test
                if (z < context->depth_buffer[index]) {
                    // Update the Z-buffer with the new closest Z value
                    context->depth_buffer[index] = z;

                    // Interpolate the texture coordinates
                    struct texcoord2f st;
                    st.s = st0->s * w0 + st1->s * w1 + st2->s * w2;
                    st.t = st0->t * w0 + st1->t * w1 + st2->t * w2;
                    st.s *= z;
                    st.t *= z;

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

            x0 = std::max(0, (int)bbox[0]);
            y0 = std::max(0, (int)bbox[1]);
            x1 = std::min(context->extent.width - 1, (int)bbox[2]);
            y1 = std::min(context->extent.height - 1, (int)bbox[3]);

            struct texcoord2f st0 = mesh->st.coords[sti[0]];
            struct texcoord2f st1 = mesh->st.coords[sti[1]];
            struct texcoord2f st2 = mesh->st.coords[sti[2]];

            st0.s /= p0.z, st0.t /= p0.z;
            st1.s /= p1.z, st1.t /= p1.z;
            st2.s /= p2.z, st2.t /= p2.z;

            rasterize(x0, y0, x1, y1, &p0, &p1, &p2, &st0, &st1, &st2, mesh, context);
        }
    }
}

void cleanup(struct context* context) {
	free(context->depth_buffer);
}