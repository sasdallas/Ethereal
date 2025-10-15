/**
 * @file userspace/lib/graphics/matrix.c
 * @brief 3x3 matrix library
 * 
 * 
 * @copyright
 * This file is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2025 Samuel Stuart
 */

#include <graphics/gfx.h>
#include <assert.h>
#include <math.h>

/**
 * @brief Get the 2x3 identity matrix
 */
gfx_mat2x3_t gfx_mat2x3_identity() {
    return (gfx_mat2x3_t){ .m = {
        1, 0, 0,
        0, 1, 0
    },
    };
}

/**
 * @brief Multiply two matrices together (A * B)
 * @param a Matrix one
 * @param b Matrix two
 */
void gfx_mat2x3_multiply(gfx_mat2x3_t *a, gfx_mat2x3_t *b) {
    // Technically you can't actually multiply A * B, so we're gonna cheat

    float c11 = a->m[0] * b->m[0] + a->m[1] * b->m[3];
    float c12 = a->m[0] * b->m[1] + a->m[1] * b->m[4];
    float c13 = a->m[0] * b->m[2] + a->m[1] * b->m[5] + a->m[2];
    float c21 = a->m[3] * b->m[0] + a->m[4] * b->m[3];
    float c22 = a->m[3] * b->m[1] + a->m[4] * b->m[4];
    float c23 = a->m[3] * b->m[2] + a->m[4] * b->m[5] + a->m[5];


    a->m[0] = c11;
    a->m[1] = c12;
    a->m[2] = c13;
    a->m[3] = c21;
    a->m[4] = c22;
    a->m[5] = c23;
}

/**
 * @brief Scale a matrix
 * @param mat The matrix to scale
 * @param sx Scale X
 * @param sy Scale Y
 */
void gfx_mat2x3_scale(gfx_mat2x3_t *mat, float sx, float sy) {
    gfx_mat2x3_t scale_mat = {
        .m = {
            sx, 0.0, 0.0,
            0.0, sy, 0.0      
        }
    };

    gfx_mat2x3_multiply(mat, &scale_mat);
}

/**
 * @brief Translate a matrix
 * @param mat The matrix to translate
 * @param tx The translation X
 * @param ty The translation Y
 */
void gfx_mat2x3_translate(gfx_mat2x3_t *mat, float tx, float ty) {
    gfx_mat2x3_t translate_mat = {
        .m = {
            1.0, 0.0, tx,
            0.0, 1.0, ty
        }
    };

    gfx_mat2x3_multiply(mat, &translate_mat);
}

/**
 * @brief Transform a point
 * @param mat The matrix to use
 * @param x The X to transform
 * @param y The Y to transform
 * @param out_x Output X
 * @param out_y Output Y
 */
void gfx_mat2x3_transform(gfx_mat2x3_t *mat, float x, float y, float *out_x, float *out_y) {
    *out_x = mat->m[0] * x + mat->m[1] * y + mat->m[2];
    *out_y = mat->m[3] * x + mat->m[4] * y + mat->m[5]; 
}

/**
 * @brief Rotate a matrix
 * @param mat The matrix to rotate
 * @param angle The angle (in degrees) to rotate
 */
void gfx_mat2x3_rotate(gfx_mat2x3_t *mat, float angle) {
    float r = angle * (M_PI / 180.0);
    float s = sinf(r);
    float c = cosf(r);
    gfx_mat2x3_multiply(mat, &(gfx_mat2x3_t){ .m = {
        c, -s, 0,
        s, c, 0
    }});
}

/**
 * @brief Calculate the determinant of a matrix
 * @param mat The matrix
 */
float gfx_mat2x3_determinant(gfx_mat2x3_t *mat) {
    return mat->m[0] * mat->m[4] - mat->m[1] * mat->m[3];
}

/**
 * @brief Invert the matrix
 * @param matrix The matrix to invert
 * @param result The resulting matrix to store into
 */
void gfx_mat2x3_invert(gfx_mat2x3_t *matrix, gfx_mat2x3_t *result) {
    // Get the determinant
    float det = gfx_mat2x3_determinant(matrix);
    assert(det && "Matrix is non-invertible");

    float inverse_det = (1.0 / det);
    result->m[0] = matrix->m[4] * inverse_det;
    result->m[1] = -matrix->m[1] * inverse_det;
    result->m[3] = -matrix->m[3] * inverse_det;
    result->m[4] = matrix->m[0] * inverse_det;

    result->m[2] = (matrix->m[1] * matrix->m[5] - matrix->m[4] * matrix->m[2]);
    result->m[5] = (matrix->m[3] * matrix->m[0] - matrix->m[4] * matrix->m[5]);
}