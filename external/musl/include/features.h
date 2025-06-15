#ifndef _FEATURES_H
#define _FEATURES_H

/* MUSL will mess with compilation of other programs if these are defined. */
#ifdef __MUSL__
#define weak __attribute__((__weak__))
#define hidden __attribute__((__visibility__("hidden")))
#define weak_alias(old, new) \
	extern __typeof(old) new __attribute__((__weak__, __alias__(#old)))
#endif


#endif
