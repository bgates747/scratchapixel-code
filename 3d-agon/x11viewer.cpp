#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <array>

#include "geometry.h" 
#include "objimporter.h"
#include "Camera.h"
#include "texturing.h"

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
    }

    // Destructor to clean up resources
    ~X11Viewer() {
        XFreeGC(display, gc);
        XDestroyWindow(display, window);
        XCloseDisplay(display);
    }

    // Main loop that handles events and renders the scene
    void mainLoop(struct context* context, int num_meshes, struct mesh** meshes) {
        // Render the scene
        render(context, num_meshes, (const struct mesh** const)meshes);
        


        // Wait for keypress and quit if 'q' is pressed
        XEvent event;
        while (true) {
            // Draw the buffer to the X11 window
            drawBufferToWindow(context);

            XNextEvent(display, &event);

            if (event.type == KeyPress) {
                char key = XLookupKeysym(&event.xkey, 0);
                if (key == 'q') {
                    break; // Exit the loop on 'q' press
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

    // Draw a single pixel on the X11 window
    void draw_pixel(int x, int y, uint8_t pixel) {
        // Decode the pixel
        std::array<uint8_t, 4> rgba = decode_pixel(pixel);

        // Convert the RGBA array to a single 32-bit color value (ARGB format)
        unsigned long color = (rgba[3] << 24) | (rgba[0] << 16) | (rgba[1] << 8) | rgba[2];

        // Set the color in the graphics context
        XSetForeground(display, gc, color);

        // Draw the pixel
        XDrawPoint(display, window, gc, x, y);
    }

    // Draw the entire buffer to the X11 window
    void drawBufferToWindow(struct context* context) {
        for (int y = 0; y < context->extent.height; ++y) {
            for (int x = 0; x < context->extent.width; ++x) {
                uint8_t pixel = context->color_buffer[y * context->extent.width + x];
                draw_pixel(x, y, pixel);
            }
        }
        // Flush the output to make sure everything is drawn
        XFlush(display);
    }
};

int main() {
    // Set the window dimensions
    // int windowWidth = 1600;
    // int windowHeight = 900;

    int windowWidth = 1024;
    int windowHeight = 768;

    // Set the OBJ file path
    // std::string objFilePath = "jet.obj";
    // std::string objFilePath = "cube.obj";
    std::string objFilePath = "wolf_map.obj";

    // Create the X11 viewer with the specified dimensions
    X11Viewer viewer(windowWidth, windowHeight);

    // Initialize the camera
    Vec3f cameraPosition(0, 0, 1.5);
    float cameraFOV = 90.0f;
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
    struct mesh** meshes = (struct mesh**)malloc(sizeof(struct mesh*) * num_meshes);
    for (int i = 0; i < num_meshes; ++i) {
        meshes[i] = (struct mesh*)malloc(sizeof(struct mesh));
    }
    ObjData meshData = ParseObj(objFilePath);
    // create_mesh(&context, meshes[0], meshData, "jet.rgba2", 512, 512);
    // create_mesh(&context, meshes[0], meshData, "blenderaxes.rgba2", 34, 34);
    create_mesh(&context, meshes[0], meshData, "wolf_tex.rgba2", 160, 160);

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

