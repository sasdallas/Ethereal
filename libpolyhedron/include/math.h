/**
 * @file libpolyhedron/include/math.h
 * @brief Math library
 * 
 * 
 * @copyright
 * This file has no copyright header attached to it, as it adapted from the musl libc to fit into
 * libpolyhedron.
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef MATH_H
#define MATH_H

/**** INCLUDES ****/
#define __NEED_float_t
#define __NEED_double_t
#include <bits/alltypes.h>
#include <features.h>


/**** DEFINITIONS ****/

#define M_E  2.7182818284590452354
#define NAN (__builtin_nanf(""))
#define INFINITY (__builtin_inff())
#define HUGE_VAL (__builtin_huge_val())

#define M_E         2.7182818284590452354       // e
#define M_LOG2E     1.4426950408889634074       // log_2 e
#define M_LOG10E    0.43429448190325182765      // log_10 e
#define M_LN2       0.69314718055994530942      // log_e 2
#define M_LN10      2.30258509299404568402      // log_e 10
#define M_PI        3.14159265358979323846      // pi
#define M_PI_2      1.57079632679489661923      // pi/2
#define M_PI_4      0.785398163397448309616     // pi/4
#define M_1_PI      0.318309886183790671538     // 1/pi
#define M_2_PI      0.636619772367581343076     // 2/pi
#define M_SQRT2     1.41421356237309504880      // sqrt(2)
#define M_SQRT1_2   0.70710678118654752440      // 1/sqrt(2)

/**** MACROS ****/

#define FP_ILOGBNAN (-1-0x7fffffff)
#define FP_ILOGB0 FP_ILOGBNAN

#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4

#define signbit(x) ( \
	sizeof(x) == sizeof(float) ? (int)(__FLOAT_BITS(x)>>31) : \
	sizeof(x) == sizeof(double) ? (int)(__DOUBLE_BITS(x)>>63) : \
	__signbitl(x) )

#define isfinite(x) ((fpclassify(x) != FP_NAN && fpclassify(x) != FP_INFINITE))
#define isnormal(x) (fpclassify(x) == FP_NORMAL)
#define isnan(x)    (fpclassify(x) == FP_NAN)
#define isinf(x)    (fpclassify(x) == FP_INFINITE)

/**** FUNCTIONS ****/


double      j0(double);
double      j1(double);
double      jn(int, double);
double      y0(double);
double      y1(double);
double      yn(int, double);
double acos(double x);
double asin(double x);
double atan2(double y, double x);
double ceil(double x);
double cosh(double x);
double ldexp(double a, int exp);
double log(double x);
double log10(double x);
double log2(double x);
double sinh(double x);
double tan(double x);
double tanh(double x);
double atan(double x);
double log1p(double x);
double expm1(double x);
double modf(double x, double *iptr);
double hypot(double x, double y);
double trunc(double x);
double acosh(double x);
double asinh(double x);
double atanh(double x);
double erf(double x);
double erfc(double x);
double gamma(double x);
double tgamma(double x);
double lgamma(double x);
double copysign(double x, double y);
double remainder(double x, double y);
int ilogb(double);
int ilogbf(float);
int ilogbl(long double);
int fpclassify(double x);
double floor(double x);
int abs(int j);
double pow(double x, double y);
double exp(double x);
double fmod(double x, double y);
double sqrt(double x);
float sqrtf(float x);
long double sqrtl(long double x);
double fabs(double x);
float fabsf(float x);
double sin(double x);
double cos(double x);
double rint(double x);
float rintf(float x);
long double rintl(long double x);
double scalbn(double x, int exp);
float scalbnf(float x, int exp);
long double scalbnl(long double x, int exp);
int __signbit(double);
int __signbitf(float);
int __signbitl(long double);
double frexp(double x, int *exp);
double log(double);
float logf(float);
long double logl(long double);
double log10(double);
float log10f(float);
long double log10l(long double);
double log1p(double);
float log1pf(float);
long double log1pl(long double);
double copysign(double, double);
float copysignf(float, float);
long double copysignl(long double, long double);
double frexp(double, int *);
float frexpf(float, int *);
long double frexpl(long double, int *);
double nextafter(double, double);
float nextafterf(float, float);
long double nextafterl(long double, long double);
double exp2(double);
float exp2f(float);
long double exp2l(long double);
double fabs(double);
float fabsf(float);
long double fabsl(long double);
double floor(double);
float floorf(float);
long double floorl(long double);
double round(double);
float roundf(float);
long double roundl(long double);
double modf(double, double *);
float modff(float, float *);
long double modfl(long double, long double *);
double exp(double);
float expf(float);
long double expl(long double);
double expm1(double);
float expm1f(float);
long double expm1l(long double);
double cos(double);
float cosf(float);
long double cosl(long double);
double sin(double);
float sinf(float);
long double sinl(long double);
double pow(double, double);
float powf(float, float);
long double powl(long double, long double);
double atan(double);
float atanf(float);
long double atanl(long double);
double remquo(double, double, int *);
float remquof(float, float, int *);
long double remquol(long double, long double, int *);
float j0f(float);
float j1f(float);
float  jnf(int, float);
float y0f(float);
float y1f(float);
float ynf(int, float);
double fmin(double, double);
float fminf(float, float);
long double fminl(long double, long double);
float ceilf(float x);
double round(double x);
float roundf(float x);
long lroundf(float x);

#endif

_End_C_Header