/* Fill in your Name and GNumber in the following two comment fields
 * Name:Siam Al-mer Chowdhury
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fp.h"

// exp field value for denormalized representation(includes 0)
const int EXPONENT_DENORMALIZED = 0;	
// exp field value for special representation(INFINITE, NAN)
const int EXPONENT_SPECIAL = (1 << (EXPONENT_BITS)) - 1;
// exp bias for float representation
const int EXPONENT_BIAS = (1 << (EXPONENT_BITS - 1)) - 1;
// exp field mask for float representation
const int EXPONENT_MASK = (1 << EXPONENT_BITS) - 1;
// frac field mask for float representation
const int FRACTION_MASK = (1 << FRACTION_BITS) - 1;
// addition value to change frac -> mantassia in normalized representation
const int MANTISSA_PLUS = (1 << FRACTION_BITS);

// macros for extracting field from float representation
#define SIGN(fp) ((fp >> (FRACTION_BITS + EXPONENT_BITS)) & 0x01)		// Extract sign value from fp_gmu representation
#define EXPONENT(fp) (((fp >> FRACTION_BITS) & EXPONENT_MASK))			// Extract exp value from fp_gmu representation
#define FRACTION(fp) (fp & FRACTION_MASK)								// Extract frac value from fp_gmu representation or mantissa representation
// macros for making mantissa in normalized representation. It will be the decimal representation such as 1.XXXXXXXX but here it is represented as 1XXXXXXXX to avoid the float operate
#define MANTISSA(frac) (MANTISSA_PLUS | frac)							

// macros for making float representation
#define MAKE_FP(sign, exp, frac) ((sign << (EXPONENT_BITS + FRACTION_BITS)) | (exp << FRACTION_BITS) | frac);
#define MAKE_ZERO(sign) MAKE_FP(sign, EXPONENT_DENORMALIZED, 0)
#define MAKE_INFINITE(sign) MAKE_FP(sign, EXPONENT_SPECIAL, 0)
#define MAKE_NAN() MAKE_FP(0, EXPONENT_SPECIAL, 1)

// macros for checking special representations
#define IS_ZERO(fp)	(EXPONENT(fp) == EXPONENT_DENORMALIZED && FRACTION(fp) == 0)
#define IS_INFINITE(fp) (EXPONENT(fp) == EXPONENT_SPECIAL && FRACTION(fp) == 0)
#define IS_NAN(fp) (EXPONENT(fp) == EXPONENT_SPECIAL && FRACTION(fp) == 1)

// get bit count of int value, it is used to produce the proper M(mantissa) value
int get_bits(int val) {
	int bits = 0;
	while (val != 0) {
		val = val >> 1;
		bits++;
	}
	return bits;
}

// convert the C float value to the S(sign), M(mantissa), E(exponent) format value (-1)^s * M * 2^E (M)
void convert_cfloat_to_SME(float val, int *S, int *M, int *E)
{
	int sign, exp;
	int mant;
	// First, confirm the sign of value and make the val positive. 
	sign = (val >= 0) ? 0 : 1;
	if (sign == 1)
		val = -1 * val;
	// Next, normalize the float value
	exp = 0;
	if (val >= 1) {	// if value is in [1, infinite), the expected exp is more than 0.
		while (val >= 2) {
			val /= 2;
			++exp;
		}
	} else {	// if value is in (0, 1), the expected exp is below 0.
		while (val < 1) {
			val *= 2;
			--exp;
		}
	}
	// in this point, val is 1.XXXXXXXX. But mantissa used in program is 1XXXXXXXX format, so convert it 
	val = val * (1 << FRACTION_BITS);
	mant = (int) val;
	// now extracted S, M, E value 
	*S = sign;
	*E = exp;
	*M = mant;
	return;
}

// convert the fp_gmu representation to the S(sign), M(mantissa), E(exponent) value
float convert_SME_to_cfloat(int S, int M, int E) {
	float val = (float)M / (MANTISSA_PLUS);
	unsigned long temp;
	if (E == -31) {// if this is denormalized representation
		temp = (1 << (-1 * (E - 1)));
		val = (val) / temp;
	}
	else if (E >= 0) {// if this is in [1, infi)
		temp = (1 << E);
		val = val * temp;
	}
	else { // if this is in (0, 1)
		temp = (1 << (-1 * E));
		val = val / temp;
	}
	return S == 0 ? val : (-val);
}

// convert the fp_gmu representation to the S(sign), M(mantissa), E(exponent) value
void convert_fp_to_SME(fp_gmu fp, int *S, int *M, int *E) {
	int exp, frac;
	*S = SIGN(fp);
	exp = EXPONENT(fp);
	frac = FRACTION(fp);
	*E = exp - EXPONENT_BIAS;
	if (exp == EXPONENT_SPECIAL) {	// if this is a special representation
		*M = frac;
		*E = exp - EXPONENT_BIAS;
	}
	else if (exp == EXPONENT_DENORMALIZED) { // if this is a denormalized representation
		*E = 1 - EXPONENT_BIAS;	// set the proper E
		*M = frac;			// set the mantissa 0XXXXXXXX
	}
	else {	// if this is a normalized representation
		*E = exp - EXPONENT_BIAS;
		*M = MANTISSA(frac);	// set the mantissa 1XXXXXXXX
	}
}

// convert the S(sign), M(mantissa), E(exponent) value to the fp_gmu representation
fp_gmu convert_SME_to_fp(int S, int M, int E)
{
	int exp = E + EXPONENT_BIAS;
	int frac;
	// check exp and determine the representation
	if (exp >= EXPONENT_SPECIAL) { // if overflow, set it INFINITE
		exp = EXPONENT_SPECIAL;
		frac = 0;	
	}
	else if (exp >= 1) { // if it is a normalized representation
		frac = FRACTION(M);	// convert 1XXXXXXXX -> XXXXXXXX
	}
	else {	// if underflow, return the denormalized representation
		frac = FRACTION(M >> (1 - exp));	// convert 1XXXXXXXX -> 0..1XXXX
		exp = EXPONENT_DENORMALIZED;
	}
	return MAKE_FP(S, exp, frac);
}


/* input: float value to be represented
 * output: fp_gmu representation of the input float
 *
 * Follow the Project Documentation for Instructions
 */
fp_gmu compute_fp(float val) {
	int S = 0, E = 0, M = 0;
	// first, verify if the float value is ZERO. if so, return 0 representation
	if (val == 0)
		return MAKE_ZERO(0);

	// convert the C float to S(sign), E(exponent), M(mantissa or fraction) format;
	convert_cfloat_to_SME(val, &S, &M, &E);

	// convert S, M, E format to fp_gmu representation. special, denormailized, normalized representation is process in this function
	return convert_SME_to_fp(S, M, E);
}

/* input: fp_gmu representation of our floats
 * output: float value represented by our fp_gmu encoding
 *
 * If your input fp_gmu is Infinity, return the constant INFINITY
 * If your input fp_gmu is -Infinity, return the constant -INFINITY
 * If your input fp_gmu is NaN, return the constant NAN
 *
 * Follow the Project Documentation for Instructions
 */
float get_fp(fp_gmu fp) {
	int S, M, E;
	// first, process the special representation
	if (IS_INFINITE(fp)) {
		if (SIGN(fp) == 0)
			return INFINITY;
		else
			return -INFINITY;
	}
	else if (IS_NAN(fp)) {
		return NAN;
	}
	else if (IS_ZERO(fp)) {
		return 0;
	}
	// convert fp_gmu format to (S, M, E) format
	convert_fp_to_SME(fp, &S, &M, &E);
	// convert (S, M, E) format to c_float format
	return convert_SME_to_cfloat(S, M, E);
}

/* input: Two fp_gmu representations of our floats
 * output: The multiplication result in our fp_gmu representation
 *
 * You must implement this using the algorithm described in class:
 *   Xor the signs: S = S1 ^ S2
 *   Add the exponents: E = E1 + E2
 *   Multiply the Frac Values: M = M1 * M2
 *   If M is not in a valid range for normalized, adjust M and E.
 *
 * Follow the Project Documentation for Instructions
 */
fp_gmu mult_vals(fp_gmu source1, fp_gmu source2) {
	int S1, S2, E1, E2, M1, M2, S, E, M;
	// first, process the special case
	if (IS_NAN(source1) || IS_NAN(source2)) {
		return MAKE_NAN();
	} 
	else if ((IS_INFINITE(source1) && IS_ZERO(source2)) || (IS_INFINITE(source2) && IS_ZERO(source1))) {
		return MAKE_NAN();
	}
	else if (IS_INFINITE(source1) || IS_INFINITE(source2)) {
		S1 = SIGN(source1); S2 = SIGN(source2);
		S = S1 ^ S2;
		return MAKE_INFINITE(S);
	}
	else if (IS_ZERO(source1) || IS_ZERO(source2)) {
		S1 = SIGN(source1); S2 = SIGN(source2);
		S = S1 ^ S2;
		return MAKE_ZERO(S);
	}
	// convert the fp_gmu representation to S(sign), M(mantissa), E(exponent) format
	convert_fp_to_SME(source1, &S1, &M1, &E1);
	convert_fp_to_SME(source2, &S2, &M2, &E2);
	// update S, E, M
	S = (S1 ^ S2);
	E = E1 + E2;
	M = (M1 * M2);
	// M is 1XXXXXXXX(not 1.XXXXXXXX). so the following is neccesary.
	E = E - FRACTION_BITS;

	// If M is not in a valid range for normalized, adjust M and E.
	int diff_bits = get_bits(M) - (FRACTION_BITS + 1);
	if (diff_bits >= 0) {
		E = E + diff_bits;
		M >>= diff_bits;
	}
	else {
		E = E + diff_bits;
		M <<= (-diff_bits);
	}
	// return the new fp representation from S, M, E format
	return convert_SME_to_fp(S, M, E);
}

/* input: Two fp_gmu representations of our floats
 * output: The addition result in our fp_gmu representation
 *
 * You must implement this using the algorithm described in class:
 *   If needed, adjust the numbers to get the same exponent E
 *   Add the two adjusted Mantissas: M = M1 + M2
 *   Adjust M and E so that it is in the correct range for Normalized
 *
 * Follow the Project Documentation for Instructions
 */
fp_gmu add_vals(fp_gmu source1, fp_gmu source2) {
	int S1, S2, E1, E2, M1, M2;
	int S, E, M;

	// first process the special cases
	if (IS_NAN(source1) || IS_NAN(source2)) {
		return MAKE_NAN();
	}
	else if (IS_INFINITE(source1) && IS_INFINITE(source2) && (SIGN(source1) != SIGN(source2))) {
		return MAKE_NAN();
	}
	else if (IS_INFINITE(source1)) {
		return MAKE_INFINITE(SIGN(source1));
	}
	else if (IS_INFINITE(source2)) {
		return MAKE_INFINITE(SIGN(source2));
	}
	else if (IS_ZERO(source1) && IS_ZERO(source2)) {
		return MAKE_ZERO(0);
	}
	else if (IS_ZERO(source1)) {
		return source2;
	}
	else if (IS_ZERO(source2)) {
		return source1;
	}
	// convert the fp_gmu representation to S(sign), M(mantissa), E(exponent) format
	convert_fp_to_SME(source1, &S1, &M1, &E1);
	convert_fp_to_SME(source2, &S2, &M2, &E2);

	// If needed, adjust the numbers to get the same exponent E
	if (E1 < E2) {
		if ((E1 + FRACTION_BITS) < E2) { // if exp difference is greater than FRACTION_BITS, it seems like 1XXXXXXXX - 0.1XXXXXXXX, so equals to 1XXXXXXXX - 0.1 because of round-zero
			M1 = 1; 
			M2 = M2 << 1;
			E = E2 - 1;
		}
		else {	// adjust the exp to E1
			E = E1;
			M2 = M2 << (E2 - E1);
		}
	}
	else {
		if ((E2 + FRACTION_BITS) < E1) { // if exp difference is greater than FRACTION_BITS, it seems like 1XXXXXXXX - 0.1XXXXXXXX, so equals to 1XXXXXXXX - 0.1 because of round-zero
			M2 = 1;
			M1 = M1 << 1;
			E = E1 - 1;
		}
		else { // adjust the exp to E1
			E = E2;
			M1 = M1 << (E1 - E2);
		}
	}
	//  Add the two adjusted Mantissas: M = M1 + M2
	M1 = (S1 == 0) ? M1 : (-1 * M1);
	M2 = (S2 == 0) ? M2 : (-1 * M2);
	M = M1 + M2;
	// Adjust M and E so that it is in the correct range for Normalized
	if (M == 0)
		return MAKE_ZERO(0);
	S = (M > 0) ? 0 : 1;
	if (M < 0)
		M = M * -1;
	int diff_bits = get_bits(M) - (FRACTION_BITS + 1);
	if (diff_bits >= 0) {
		E = E + diff_bits;
		M >>= diff_bits;
	}
	else {
		E = E + diff_bits;
		M <<= (-diff_bits);
	}
	return convert_SME_to_fp(S, M, E);
}
