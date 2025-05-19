/**
 * @file libpolyhedron/include/math.h
 * @brief Math library
 * 
 * 
 * @copyright
 * This file was sourced here: https://github.com/klange/toaruos/blob/c1d05df5ff5629705fbfb7a926268ee2b2b9d099/base/usr/include/math.h
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef MATH_H
#define MATH_H

#define __NEED_float_t
#define __NEED_double_t
#include <bits/alltypes.h>

#include <features.h>

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

#define FP_ILOGBNAN (-1-0x7fffffff)
#define FP_ILOGB0 FP_ILOGBNAN

#define M_E  2.7182818284590452354
#define NAN (__builtin_nanf(""))
#define INFINITY (__builtin_inff())

double      j0(double);
double      j1(double);
double      jn(int, double);

double      y0(double);
double      y1(double);
double      yn(int, double);

extern double floor(double x);
extern int abs(int j);
extern double pow(double x, double y);
extern double exp(double x);
extern double fmod(double x, double y);
extern double sqrt(double x);
extern float sqrtf(float x);
extern long double sqrtl(long double x);
extern double fabs(double x);
extern float fabsf(float x);
extern double sin(double x);
extern double cos(double x);

extern double rint(double x);
extern float rintf(float x);
extern long double rintl(long double x);

extern double scalbn(double x, int exp);
extern float scalbnf(float x, int exp);
extern long double scalbnl(long double x, int exp);

extern int __signbit(double);
extern int __signbitf(float);
extern int __signbitl(long double);

#define signbit(x) ( \
	sizeof(x) == sizeof(float) ? (int)(__FLOAT_BITS(x)>>31) : \
	sizeof(x) == sizeof(double) ? (int)(__DOUBLE_BITS(x)>>63) : \
	__signbitl(x) )

static __inline unsigned __FLOAT_BITS(float __f)
{
    union {float __f; unsigned __i;} __u;
    __u.__f = __f;
    return __u.__i;
}
static __inline unsigned long long __DOUBLE_BITS(double __f)
{
    union {double __f; unsigned long long __i;} __u;
    __u.__f = __f;
    return __u.__i;
}

double frexp(double x, int *exp);

double      log(double);
float       logf(float);
long double logl(long double);

double      log10(double);
float       log10f(float);
long double log10l(long double);

double      log1p(double);
float       log1pf(float);
long double log1pl(long double);

double      copysign(double, double);
float       copysignf(float, float);
long double copysignl(long double, long double);


double      frexp(double, int *);
float       frexpf(float, int *);
long double frexpl(long double, int *);

double      nextafter(double, double);
float       nextafterf(float, float);
long double nextafterl(long double, long double);

double      exp2(double);
float       exp2f(float);
long double exp2l(long double);

double      fabs(double);
float       fabsf(float);
long double fabsl(long double);

double      floor(double);
float       floorf(float);
long double floorl(long double);

double      round(double);
float       roundf(float);
long double roundl(long double);

double      modf(double, double *);
float       modff(float, float *);
long double modfl(long double, long double *);

double      exp(double);
float       expf(float);
long double expl(long double);

double      expm1(double);
float       expm1f(float);
long double expm1l(long double);

double      cos(double);
float       cosf(float);
long double cosl(long double);

double      sin(double);
float       sinf(float);
long double sinl(long double);

double      pow(double, double);
float       powf(float, float);
long double powl(long double, long double);

double      atan(double);
float       atanf(float);
long double atanl(long double);

double      remquo(double, double, int *);
float       remquof(float, float, int *);
long double remquol(long double, long double, int *);

float       j0f(float);
float       j1f(float);
float       jnf(int, float);

float       y0f(float);
float       y1f(float);
float       ynf(int, float);

#define HUGE_VAL (__builtin_huge_val())


double      fmin(double, double);
float       fminf(float, float);
long double fminl(long double, long double);

/* Unimplemented, but stubbed */
extern double acos(double x);
extern double asin(double x);
extern double atan2(double y, double x);
extern double ceil(double x);
extern double cosh(double x);
extern double ldexp(double a, int exp);
extern double log(double x);
extern double log10(double x);
extern double log2(double x);
extern double sinh(double x);
extern double tan(double x);
extern double tanh(double x);
extern double atan(double x);
extern double log1p(double x);
extern double expm1(double x);

extern double modf(double x, double *iptr);

extern double hypot(double x, double y);

extern double trunc(double x);
extern double acosh(double x);
extern double asinh(double x);
extern double atanh(double x);
extern double erf(double x);
extern double erfc(double x);
extern double gamma(double x);
extern double tgamma(double x);
extern double lgamma(double x);
extern double copysign(double x, double y);
extern double remainder(double x, double y);

extern int         ilogb(double);
extern int         ilogbf(float);
extern int         ilogbl(long double);

enum {
    FP_NAN, FP_INFINITE, FP_ZERO, FP_SUBNORMAL, FP_NORMAL
};

extern int fpclassify(double x);

#define isfinite(x) ((fpclassify(x) != FP_NAN && fpclassify(x) != FP_INFINITE))
#define isnormal(x) (fpclassify(x) == FP_NORMAL)
#define isnan(x)    (fpclassify(x) == FP_NAN)
#define isinf(x)    (fpclassify(x) == FP_INFINITE)

extern float ceilf(float x);
extern double round(double x);
extern float roundf(float x);
extern long lroundf(float x);

#endif

_End_C_Header