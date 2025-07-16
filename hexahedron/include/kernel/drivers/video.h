/**
 * @file hexahedron/include/kernel/drivers/video.h
 * @brief Generic video driver header
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#ifndef DRIVERS_VIDEO_H
#define DRIVERS_VIDEO_H

/**** INCLUDES ****/
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

/**** TYPES ****/

// This type is for any color. You can use the definitions or macros to interact, or make your own.
typedef union _color {
    unsigned long rgb;
    struct {
        unsigned char b;
        unsigned char g;
        unsigned char r;
    } c;
} color_t;

struct _video_driver; // Prototype
struct _video_mode; // Prototype

/**
 * @brief Update the screen and draw the given framebuffer
 * @param driver The driver to update the screen of
 * @param buffer A linear framebuffer to update the screen with
 */
typedef void (*video_updscreen_t)(struct _video_driver *driver, uint8_t *buffer);

/**
 * @brief Load the driver
 * @param driver The driver object
 */
typedef int (*video_load_t)(struct _video_driver *driver);

/**
 * @brief Unload the driver
 * @param driver The driver object
 */
typedef int (*video_unload_t)(struct _video_driver *driver);

/**
 * @brief Map the raw framebuffer into memory
 * @param driver The driver object
 * @param size How much of the framebuffer was requested to be mapped into memory
 * @param off The offset to start mapping at
 * @param addr The address to map at
 * @returns Error code
 */
typedef int (*video_map_t)(struct _video_driver *driver, size_t size, off_t off, void *addr);

/**
 * @brief Unmap the raw framebuffer from memory
 * @param driver The driver object
 * @param size How much of the framebuffer was mapped in memory
 * @param off The offset to start mapping at
 * @param addr The address of the framebuffer memory
 * @returns 0 on success
 */
typedef int (*video_unmap_t)(struct _video_driver *driver, size_t size, off_t off, void *addr);

/**
 * @brief Set a specific video mode
 * @param driver The video driver object
 * @param mode The requested mode to set
 * @returns 0 on success 
 */
typedef int (*video_setmode_t)(struct _video_driver *driver, struct _video_mode *mode);

/**
 * @brief Video mode structure
 */
typedef struct _video_mode {
    uint32_t width;                         // Mode width
    uint32_t height;                        // Mode height
    uint32_t bpp;                           // Color depth
} video_mode_t;

typedef struct _video_driver {
    // Driver information
    char            name[64];
     
    // Information/fields of the video driver
    uint32_t        screenWidth;            // Width
    uint32_t        screenHeight;           // Height
    uint32_t        screenPitch;            // Pitch
    uint32_t        screenBPP;              // BPP
    uint8_t         *videoBuffer;           // Linear video buffer in virtual memory (NON-OPTIONAL, should conform to default LFB standards) 
    uint8_t         *videoBufferPhys;       // (OPTIONAL) Physical address of video buffer 
    int             allowsGraphics;         // Whether it allows graphics (WARNING: This may be used. It is best to leave this correct!)
    void            *dev;                   // Specific to the driver

    // Functions
    video_updscreen_t update;               // Update screen method
    video_load_t load;                      // Load method
    video_unload_t unload;                  // Unload method
    video_map_t map;                        // Map method
    video_unmap_t unmap;                    // Unmap method

    // Fonts and other information will be handled by the font driver
} video_driver_t;

/**** MACROS ****/

// RGB manipulation functions
#define RGB_R(color) (color.c.r & 255)
#define RGB_G(color) (color.c.g & 255)
#define RGB_B(color) (color.c.b & 255)
#define RGB(red, green, blue) (color_t){.c.r = red, .c.g = green, .c.b = blue}


/**** DEFINITIONS ****/

// Defines for VGA text mode graphics converted to RGB
#define COLOR_BLACK             RGB(0, 0, 0)
#define COLOR_BLUE              RGB(0, 0, 170)
#define COLOR_GREEN             RGB(0, 170, 0)
#define COLOR_CYAN              RGB(0, 170, 170)
#define COLOR_RED               RGB(170, 0, 0)
#define COLOR_PURPLE            RGB(170, 0, 170)
#define COLOR_BROWN             RGB(170, 85, 0)
#define COLOR_GRAY              RGB(170, 170, 170)
#define COLOR_DARK_GRAY         RGB(85, 85, 85)
#define COLOR_LIGHT_BLUE        RGB(85, 85, 255)
#define COLOR_LIGHT_GREEN       RGB(85, 255, 85)
#define COLOR_LIGHT_CYAN        RGB(85, 255, 255)
#define COLOR_LIGHT_RED         RGB(255, 85, 85)
#define COLOR_LIGHT_PURPLE      RGB(255, 85, 255)
#define COLOR_YELLOW            RGB(255, 255, 85)
#define COLOR_WHITE             RGB(255, 255, 255)

/**** FUNCTIONS ****/

/**
 * @brief Mount video node
 */
void video_mount();

/**
 * @brief Initialize and prepare the video system.
 * 
 * This doesn't actually initialize any drivers, just starts the system.
 */
void video_init();

/**
 * @brief Add a new driver
 * @param driver The driver to add
 */
void video_addDriver(video_driver_t *driver);

/**
 * @brief Switch to a specific driver
 * @param driver The driver to switch to. If not found in the list it will be added.
 */
void video_switchDriver(video_driver_t *driver);

/**
 * @brief Find a driver by name
 * @param name The name to look for
 * @returns NULL on not found, else it returns a driver
 */
video_driver_t *video_findDriver(char *name);

/**
 * @brief Get the current driver
 */
video_driver_t *video_getDriver();

/**
 * @brief Plot a pixel on the screen
 * @param x The x coordinate of where to plot the pixel
 * @param y The y coordinate of where to plot the pixel
 * @param color The color to plot
 */
void video_plotPixel(int x, int y, color_t color);

/**
 * @brief Clear the screen with colors
 * @param bg The background of the screen
 */
void video_clearScreen(color_t bg);

/**
 * @brief Update the screen
 */
void video_updateScreen();

/**
 * @brief Returns the current video framebuffer
 * @returns The framebuffer or NULL
 * 
 * You are allowed to draw in this just like you would a normal linear framebuffer,
 * just call @c video_updateScreen when finished
 */
uint8_t *video_getFramebuffer();

#endif