/**
 * @file userspace/lib/include/graphics/matrix.h
 * @brief Matrix
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#ifndef _GRAPHICS_MATRIX_H
#define _GRAPHICS_MATRIX_H

/**** INCLUDES ****/
#include <stdint.h>

/**** TYPES ****/
typedef struct gfx_mat2x3 {
    float m[3 * 2];
} gfx_mat2x3_t;

/**** FUNCTIONS ****/

/**
 * @brief Get the 2x3 identity matrix
 */
gfx_mat2x3_t gfx_mat2x3_identity();

/**
 * @brief Multiply two matrices together (A * B)
 * @param a Matrix one
 * @param b Matrix two
 */
void gfx_mat2x3_multiply(gfx_mat2x3_t *a, gfx_mat2x3_t *b);

/**
 * @brief Scale a matrix
 * @param mat The matrix to scale
 * @param sx Scale X
 * @param sy Scale Y
 */
void gfx_mat2x3_scale(gfx_mat2x3_t *mat, float sx, float sy);

/**
 * @brief Translate a matrix
 * @param mat The matrix to translate
 * @param tx The translation X
 * @param ty The translation Y
 */
void gfx_mat2x3_translate(gfx_mat2x3_t *mat, float tx, float ty);

/**
 * @brief Transform a point
 * @param mat The matrix to use
 * @param x The X to transform
 * @param y The Y to transform
 * @param out_x Output X
 * @param out_y Output Y
 */
void gfx_mat2x3_transform(gfx_mat2x3_t *mat, float x, float y, float *out_x, float *out_y);

/**
 * @brief Calculate the determinant of a matrix
 * @param mat The matrix
 */
float gfx_mat2x3_determinant(gfx_mat2x3_t *mat);

/**
 * @brief Invert the matrix
 * @param matrix The matrix to invert
 * @param result The resulting matrix to store into
 */
void gfx_mat2x3_invert(gfx_mat2x3_t *matrix, gfx_mat2x3_t *result);

/**
 * @brief Rotate a matrix
 * @param mat The matrix to rotate
 * @param angle The angle (in degrees) to rotate
 */
void gfx_mat2x3_rotate(gfx_mat2x3_t *mat, float angle);


#endif