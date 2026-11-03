/* C math ABI declarations; definitions come exclusively from target libm. */
#ifndef MHI2_MATH_COMPAT_H
#define MHI2_MATH_COMPAT_H
#define M_PI 3.14159265358979323846
#define INFINITY (__builtin_inff())
#define NAN (__builtin_nanf(""))
#define isfinite(x) __builtin_isfinite(x)
#define isnan(x) __builtin_isnan(x)
#define isinf(x) __builtin_isinf(x)
#define signbit(x) __builtin_signbit(x)
#define HUGE_VAL (__builtin_huge_val())
#define HUGE_VALF (__builtin_huge_valf())
#ifdef __cplusplus
extern "C" {
#endif
float sinf(float),cosf(float),sqrtf(float),expf(float),logf(float),log2f(float),log10f(float),fabsf(float),floorf(float),ceilf(float),tanhf(float),acosf(float),expm1f(float),roundf(float);
float powf(float,float),fminf(float,float),fmaxf(float,float),atan2f(float,float),fmodf(float,float);
double sin(double),cos(double),sqrt(double),exp(double),log(double),log2(double),log10(double),fabs(double),floor(double),ceil(double),tanh(double),acos(double),expm1(double),round(double);
double pow(double,double),fmin(double,double),fmax(double,double),atan2(double,double),fmod(double,double);
long lrint(double),lrintf(float),lround(double),lroundf(float);
#ifdef __cplusplus
}
#endif
#endif
