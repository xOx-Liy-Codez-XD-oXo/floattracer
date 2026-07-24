#ifndef LIYTRACER_H
#define LIYTRACER_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

//#define USE_SIN_LUTS

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

float pi = 3.141592653589793;

float degToRad(float deg) {
	return deg * 3.141592653589793 / 180.0f;
}

float * shitrands; //0.0f - 1.0f;
float * unitrands; //-1.0f - 1.0f;
u16 whichrand, whichunitrand;
void fillShitrand() {
	long lala = (int)malloc((8*65536) + 31);
	lala += 31;
	lala &= ~31;
	shitrands = (float *)lala;
	unitrands = (float *)((u8 *)shitrands + (4*65535));

	for(int i = 0; i < 65535; i++) {
		shitrands[i] = (rand()%1000000)/1000000.0f;
		unitrands[i] = (shitrands[i] * 2.0f) - 1.0f;
	}
}

float randomFloat() {
	return shitrands[whichrand++];
}

float randomUnitFloat() {
	return unitrands[whichunitrand++];
}

float randomFloatMinmax(float min, float max) {
	return min + (max-min)*randomFloat();
}

int max(int a, int b) {
    return (a > b) ? a : b;
}

int min(int a, int b) {
    return (a < b) ? a : b;
}

float rsqrt(float x) {
	//long i;
	//i = *(long *)&x;
	//i = 0x5f3759df - (i >> 1);
	//return *(float*)&i;
	float r;
	asm ("frsqrte %0, %1\n\t" 
	      : "=f"(r) 
	      : "f"(x));
	return r;
}

float reciprocal(float x) {
	//unsigned long i = *(unsigned long *)&x;
	//i = 0x7effffffU - i;
	//return *(float *)&i;
	float r;
	asm ("fres %0, %1\n\t" 
	      : "=f"(r) 
	      : "f"(x));
	return r;
}

float shitsqrt(float x) {
	//long i;
	//i = *(long *)&x;
	//i = 0x5f3759df - (i >> 1);
	//i = 0x7effffffU - i;
	//return *(float *)&i;
	//float r;
	//asm ("frsqrte %0, %1\n\t"
        //     "fres %1, %1\n\t"
	//      : "=f"(r) 
	//      : "f"(x));
	//return r;
	return reciprocal(rsqrt(x));
}

float shitpow(float a, float b) {
	if(b > 999999.9f) return 0;
	union { float f; s32 i; } u = { a };
	s32 x = (s32)(b * (float)(u.i - 1064866805) + 1064866805);
	if (x < 0) x = 0; // tiny a's were getting turned negative
	u.i = x;
	return u.f;
}

#include "vec3.h"
#include "color.h"
#include "ray.h"

#endif
