#ifndef BITOPS_H
#define BITOPS_H
/**
	A small set of bitwise operations and mathematical macros based on them. */

#define bitof(e) (sizeof(e)*CHAR_BIT)

// Right shifting a negative number is *implementation defined* behavior.
// * and / by 2**n replicate arithmetic shifting by n.
#define A_RSHIFT_NATIVE(val, n) ((val) >> (n))
// TODO: The emulated shift below does not work on negative numbers. Fix that.
#define A_RSHIFT_EMULATED(val, n) (((val) / ((1+((val)*0)) << ((n)-1))) / 2)

// The nature of a (signed) right shift on a negative number is IDB.
// This #if determines whether the rshift is already arithmetic.
// If not, we emulate an arithmetic one.
#if (-1 >> 1) < 0 // If the right shift is arithmetic, use it.
#define ars A_RSHIFT_NATIVE
#else
#define ars A_RSHIFT_EMULATED
#endif

// Treat a value as an intmax_t, or uintmax_t.
#define as_um(I) ((union{intmax_t i; uintmax_t u; }){(I)}.u)
#define as_im(U) ((union{uintmax_t u; intmax_t i; }){(U)}.i)

// This macro masks off the top n bits of some pattern.
#define _top_mask_(val, n) (as_um(val) & (((uintmax_t)1 << (bitof(val)-(n)))-1))
#define _top_mask(val, n, s_p)										\
	((s_p) ? as_im(_top_mask_((val), (n))) : _top_mask_((val), (n)))

// __FAST_MATH__ for this assumes a two's complement machine.
// Use the unsigned version that assumed two's complement if warnings are on.
// This is to avoid the spookiness of signed and unsigned vals in one ternary.
#if defined(STRICT_WARN__) || defined(__FAST_MATH__)
#define top_mask(val, n) (_top_mask_((val), (n)))
#else
#define top_mask(val, n)											\
	 (_top_mask((val), (n), ((val) < 0) || ((val)*0-1) < 0))
#endif // d(STRICT_WARN__) || d(__FAST_MATH__)

// __FAST_MATH__ for this assumes that left shift overflows drop the bit.
// That is (of course *eye roll*) technically UB, but most implementations do that, thus the option.
// Of course, the UB is avoided by default. By defining the __FAST_MATH__ macro, the user is telling me "I know my compiler doesn't do stupid shit, so do the fast thing, not the stupid UB-avoiding thing."
#ifndef __FAST_MATH__ // Disable OFF_TOP'ing is -ffast-math is requested.
#define ls_s(val, n) (top_mask((val), (n)) << (n))
#else
#define ls_s(val, n) ((val) << (n))
#endif // __FAST_MATH__


#define CEIL_DIV(num, divisor) (((num) + ((divisor)-1))/(divisor))
#define POW2_MODULO(lhs, rhs) ((lhs) & (rhs - 1)) // Requires rhs = a power of 2

#endif /* BITOPS_H */
