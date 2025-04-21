/**
 * @file hexahedron/gfx/gfx.c
 * @brief Minimal graphics routine
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <stdlib.h>
#include <kernel/gfx/gfx.h>
#include <kernel/drivers/video.h>
#include <kernel/debug.h>

/**
 * @brief Draw a line
 * @param x1 First X
 * @param y1 First Y
 * @param x2 Second X
 * @param y2 Second Y
 * @param color The color of the line
 */
void gfx_drawLine(int x1, int y1, int x2, int y2, color_t color) {
    int dx = abs(x2 - x1); // Delta X
    int dy = abs(y2 - y1); // Delta Y
    int cx = (x1 < x2) ? 1 : -1; // Change for X
    int cy = (y1 < y2) ? 1 : -1; // Change for Y
    int dc = dx - dy; // DC/error

    // Start looping infinitely, we'll break when x == x2 and y == y2.
    int x = x1;
    int y = y1;
    while (1) {
        video_plotPixel(x, y, color);
        if (x == x2 && y == y2) break;

        // Update dc for dy
        if (2*dc > -dy) {
            dc -= dy;
            x += cx;
        }

        // Update dc for dx
        if (2*dc < dx) {
            dc += dx;
            y += cy;
        }
    }
}

/**
 * @brief Draw the Hexahedron logo
 * @param color The logo color
 */
void gfx_drawLogo(color_t color) {
    // Calculate size and offsets
    int size = 100;
    int offsetX = size / 2;
    int offsetY = size / 2;

    // Calculate the center x/y of the screen
    int center_x = video_getDriver()->screenWidth - size - 10; // Subtract 10 for some extra centering
    int center_y = size + 10;

    


    // Vertices of the cube in 3D space (without any projection applied)
    // Cube vertices (8 total)
    int vertices[8][2] = {
        {center_x - offsetX, center_y - offsetY}, // Front-bottom-left
        {center_x + offsetX, center_y - offsetY}, // Front-bottom-right
        {center_x + offsetX, center_y + offsetY}, // Front-top-right
        {center_x - offsetX, center_y + offsetY}, // Front-top-left
        {center_x - offsetX + offsetY, center_y - offsetY - offsetX}, // Back-bottom-left
        {center_x + offsetX + offsetY, center_y - offsetY - offsetX}, // Back-bottom-right
        {center_x + offsetX + offsetY, center_y + offsetY - offsetX}, // Back-top-right
        {center_x - offsetX + offsetY, center_y + offsetY - offsetX}  // Back-top-left
    };

    // Draw the front and back squares (front and back edges)
    gfx_drawLine(vertices[0][0], vertices[0][1], vertices[1][0], vertices[1][1], color); // Front-bottom-left to front-bottom-right
    gfx_drawLine(vertices[1][0], vertices[1][1], vertices[2][0], vertices[2][1], color); // Front-bottom-right to front-top-right
    gfx_drawLine(vertices[2][0], vertices[2][1], vertices[3][0], vertices[3][1], color); // Front-top-right to front-top-left
    gfx_drawLine(vertices[3][0], vertices[3][1], vertices[0][0], vertices[0][1], color); // Front-top-left to front-bottom-left

    gfx_drawLine(vertices[4][0], vertices[4][1], vertices[5][0], vertices[5][1], color); // Back-bottom-left to back-bottom-right
    gfx_drawLine(vertices[5][0], vertices[5][1], vertices[6][0], vertices[6][1], color); // Back-bottom-right to back-top-right
    gfx_drawLine(vertices[6][0], vertices[6][1], vertices[7][0], vertices[7][1], color); // Back-top-right to back-top-left
    gfx_drawLine(vertices[7][0], vertices[7][1], vertices[4][0], vertices[4][1], color); // Back-top-left to back-bottom-left

    // Connect the front and back squares
    gfx_drawLine(vertices[0][0], vertices[0][1], vertices[4][0], vertices[4][1], color); // Front-bottom-left to back-bottom-left
    gfx_drawLine(vertices[1][0], vertices[1][1], vertices[5][0], vertices[5][1], color); // Front-bottom-right to back-bottom-right
    gfx_drawLine(vertices[2][0], vertices[2][1], vertices[6][0], vertices[6][1], color); // Front-top-right to back-top-right
    gfx_drawLine(vertices[3][0], vertices[3][1], vertices[7][0], vertices[7][1], color); // Front-top-left to back-top-left
}
