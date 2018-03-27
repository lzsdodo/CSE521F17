#include <stdint.h>
#ifndef __THREAD_FIXED_POINT_H
#define __THREAD_FIXED_POINT_H

/* Basic definitions of fixed point. */
typedef int fixed_t;

/* 16 LSB used for fractional part. */
#ifndef FP_SHIFT_AMOUNT
#define FP_SHIFT_AMOUNT 16
#endif

/* Convert a value to fixed-point value. */
fixed_t FP_CONST(int A);
fixed_t FP_CONST(int A) {
    return (fixed_t)(A << FP_SHIFT_AMOUNT);
}

/* Add two fixed-point value. */
fixed_t FP_ADD(fixed_t A, fixed_t B);
fixed_t FP_ADD(fixed_t A, fixed_t B){
    return (A + B);
}

/* Add a fixed-point value A and an int value B. */
fixed_t FP_ADD_MIX(fixed_t A, int B);
fixed_t FP_ADD_MIX(fixed_t A, int B){
    return (A + (B << FP_SHIFT_AMOUNT));
}

/* Substract two fixed-point value. */
fixed_t FP_SUB(fixed_t A, fixed_t B);
fixed_t FP_SUB(fixed_t A, fixed_t B){
    return (A - B);
}

/* Substract an int value B from a fixed-point value A */
fixed_t FP_SUB_MIX(fixed_t A, int B);
fixed_t FP_SUB_MIX(fixed_t A, int B){
    return (A - (B << FP_SHIFT_AMOUNT));
}

/* Multiply a fixed-point value A by an int value B. */
fixed_t FP_MULT_MIX(fixed_t A, int B);
fixed_t FP_MULT_MIX(fixed_t A, int B){
    return (A * B);
}

/* Divide a fixed-point value A by an int value B. */
fixed_t FP_DIV_MIX(fixed_t A, int B);
fixed_t FP_DIV_MIX(fixed_t A, int B) {
    return (A / B);
}

/* Multiply two fixed-point value. */
fixed_t FP_MULT(fixed_t A, fixed_t B);
fixed_t FP_MULT(fixed_t A, fixed_t B) {
    return ((fixed_t)(((int64_t) A) * B >> FP_SHIFT_AMOUNT));
}

/* Divide two fixed-point value. */
fixed_t FP_DIV(fixed_t A, fixed_t B);
fixed_t FP_DIV(fixed_t A, fixed_t B){
    return ((fixed_t)((((int64_t) A) << FP_SHIFT_AMOUNT) / B));
}
/* Get integer part of a fixed-point value. */
int  FP_INT_PART(fixed_t A);
int  FP_INT_PART(fixed_t A) {
    return (A >> FP_SHIFT_AMOUNT);
}

/* Get rounded integer of a fixed-point value. */
int FP_ROUND(fixed_t A);
int FP_ROUND(fixed_t A) {
    return (A >= 0 ? ((A + (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT) \
            : ((A -(1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT));
}

#endif /* thread/fixed_point.h */