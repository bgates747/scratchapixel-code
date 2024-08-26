#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <array>
#include <chrono>

#include "geometry.h" 
#include "objimporter.h"
#include "Camera.h"
#include "texturing.h"
#include "object.h"

class X11Viewer {
public:
    // Constructor to create a window with the specified dimensions
    X11Viewer(int width, int height) : width(width), height(height) {
        display = XOpenDisplay(nullptr);
        if (display == nullptr) {
            std::cerr << "Cannot open display\n";
            exit(1);
        }

        screen = DefaultScreen(display);
        window = XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, width, height, 1,
                                     BlackPixel(display, screen), WhitePixel(display, screen));

        XSetStandardProperties(display, window, "X11 Viewer", "X11 Viewer", None, nullptr, 0, nullptr);
        XSelectInput(display, window, ExposureMask | KeyPressMask);
        gc = XCreateGC(display, window, 0, 0);

        XMapWindow(display, window);

        // Create the XImage for drawing
        image = XCreateImage(display, DefaultVisual(display, screen), DefaultDepth(display, screen), ZPixmap, 0,
                             (char*)malloc(width * height * 4), width, height, 32, 0);
    }

    // Destructor to clean up resources
    ~X11Viewer() {
        XFreeGC(display, gc);
        XDestroyWindow(display, window);
        XDestroyImage(image); // Free the XImage
        XCloseDisplay(display);
    }

    void mainLoop(struct context* context, int num_meshes, struct Mesh** meshes) {
        while (true) {
            // auto start = std::chrono::high_resolution_clock::now();

            // Render the scene
            render(context, num_meshes, (const struct Mesh** const)meshes);

            // Draw the buffer to the X11 window
            drawBufferToWindow(context);

            // auto end = std::chrono::high_resolution_clock::now();
            // std::chrono::duration<double> elapsed = end - start;
            // double render_time_ms = elapsed.count() * 1000.0;
            // double fps = 1000.0 / render_time_ms;

            // // Output the timing information
            // std::cout << "Render Time: " << render_time_ms << " ms, FPS: " << fps << std::endl;

            // Handle events in a non-blocking manner
            while (XPending(display) > 0) {
                XEvent event;
                XNextEvent(display, &event);
                if (event.type == KeyPress) {
                    char key = XLookupKeysym(&event.xkey, 0);
                    if (key == 'q') {
                        return; // Exit the loop on 'q' press
                    }
                }
            }
        }
    }


private:
    Display *display;
    int screen;
    Window window;
    GC gc; // Graphics context
    int width, height;
    XImage* image; // XImage to store the pixel data

    // Decode the rgba2222 pixel format
    std::array<uint8_t, 4> decode_pixel(uint8_t pixel) {
        uint8_t a = (pixel >> 6) & 0b11;
        uint8_t b = (pixel >> 4) & 0b11;
        uint8_t g = (pixel >> 2) & 0b11;
        uint8_t r = pixel & 0b11;

        // Map the 2-bit values to 8-bit color values
        static const uint8_t mapping[4] = {0, 85, 170, 255};

        return {mapping[r], mapping[g], mapping[b], mapping[a]};
    }

    // Draw the entire buffer to the X11 window using XPutImage
    void drawBufferToWindow(struct context* context) {
        uint32_t* image_data = (uint32_t*)image->data;

        for (int y = 0; y < context->extent.height; ++y) {
            for (int x = 0; x < context->extent.width; ++x) {
                uint8_t pixel = context->color_buffer[y * context->extent.width + x];
                std::array<uint8_t, 4> rgba = decode_pixel(pixel);

                // Convert the RGBA array to a single 32-bit color value (ARGB format)
                uint32_t color = (rgba[3] << 24) | (rgba[0] << 16) | (rgba[1] << 8) | rgba[2];

                image_data[y * context->extent.width + x] = color;
            }
        }

        // Put the image data to the window in one go
        XPutImage(display, window, gc, image, 0, 0, 0, 0, width, height);

        // Flush the output to make sure everything is drawn
        XFlush(display);
    }
};

int main() {
    // Set the window dimensions
    // int windowWidth = 1600;
    // int windowHeight = 900;

    // int windowWidth = 1024;
    // int windowHeight = 768;

    int windowWidth = 640;
    int windowHeight = 320;

    // Set the OBJ file path
    std::string objFilePath = "objects/jet.obj";
    // std::string objFilePath = "objects/cube.obj";
    // std::string objFilePath = "objects/wolf_map.obj";

    // Create the X11 viewer with the specified dimensions
    X11Viewer viewer(windowWidth, windowHeight);

    // Initialize the camera
    Vec3f cameraPosition(0, 0, 10);
    float cameraFOV = 45.0f;
    float nearClip = 1.0f;
    float farClip = 1000.0f;
    float aspectRatio = static_cast<float>(windowWidth) / windowHeight;
    Camera camera(cameraPosition, cameraFOV, nearClip, farClip, aspectRatio);

    // Initialize the context using the camera
    struct context context;
    context.extent.width = windowWidth;
    context.extent.height = windowHeight;
    context_init(&context, camera); // Pass the camera object to context_init
    prepare_buffers(&context);

    // Set up the mesh data
    int num_meshes = 1;
    struct Mesh** meshes = (struct Mesh**)malloc(sizeof(struct Mesh*) * num_meshes);
    for (int i = 0; i < num_meshes; ++i) {
        meshes[i] = (struct Mesh*)malloc(sizeof(struct Mesh));
    }
    ObjData meshData = ParseObj(objFilePath);
    struct texture* my_texture = create_texture("objects/jet.rgba2", 512, 512);
    // struct texture* my_texture = create_texture("objects/blenderaxes.rgba2", 34, 34);
    // struct texture* my_texture = create_texture("objects/wolf_tex.rgba2", 160, 160);

    create_mesh(&context, meshes[0], meshData, my_texture);

    // Set up some objects
    int num_objects = 1;
    struct Object* objects = (struct Object*)malloc(sizeof(struct Object) * num_objects);
    object_init(&objects[0], meshes[0], {0, 0, 0}, {0, 0, 0}, {1, 1, 1});

    // Enter the main loop, passing the context and meshes for rendering
    viewer.mainLoop(&context, num_meshes, meshes);

    // Cleanup
    cleanup(&context);
    for (int i = 0; i < num_meshes; ++i) {
        destroy_mesh(meshes[i]);
        free(meshes[i]);
    }
    free(meshes);

    return 0;
}

