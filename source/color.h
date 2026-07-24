#ifndef COLOR_H
#define COLOR_H
#include "vec3.h"

float clamp(float x) {
	if (x > 1) {
		return 1.0f;
	} else if (x < 0) {
		return 0.0f;
	} else {
		return x;
	}
}

void write_color(FILE* out, struct vec3 pixel_color) {
	float r = pixel_color.x;
	float g = pixel_color.y;
	float b = pixel_color.z;
	
	int rbyte = floor(255.999 * clamp(r));
	int gbyte = floor(255.999 * clamp(g));
	int bbyte = floor(255.999 * clamp(b));

	rbyte = max(rbyte, 0);
	gbyte = max(gbyte, 0);
	bbyte = max(bbyte, 0);

	rbyte = min(rbyte, 255);
	gbyte = min(gbyte, 255);
	bbyte = min(bbyte, 255);
	

	fprintf(out, "%d %d %d\n", rbyte, gbyte, bbyte);
}

GXColor colortogxcol(struct vec3 pixel_color) {
	float r = pixel_color.x;
	float g = pixel_color.y;
	float b = pixel_color.z;
	
	int rbyte = 255.0f * r;
	int gbyte = 255.0f * g;
	int bbyte = 255.0f * b;

	rbyte = max(rbyte, 0);
	gbyte = max(gbyte, 0);
	bbyte = max(bbyte, 0);

	rbyte = min(rbyte, 255);
	gbyte = min(gbyte, 255);
	bbyte = min(bbyte, 255);

	return (GXColor){rbyte, gbyte, bbyte, 255};
}

GXColor gxcol_halfnhalf(GXColor x, GXColor y) {
	int r, g, b;
	r = x.r + y.r;
	g = x.g + y.g;
	b = x.b + y.b;
	r /= 2;
	g /= 2;
	b /= 2;
	return (GXColor){r, g, b, 255};
}

GXColor gxcol_lerp(GXColor x, GXColor y, float t) {
	int r, g, b;
	r = x.r + (int)((y.r - x.r) * t);
	g = x.g + (int)((y.g - x.g) * t);
	b = x.b + (int)((y.b - x.b) * t);
	return (GXColor){r, g, b, 255};
}

struct vec3 rawToSrgb(struct vec3 col) {
	col.x = sqrtf(col.x);
	col.y = sqrtf(col.y);
	col.z = sqrtf(col.z);

	return col;
}

struct vec3 black = (struct vec3){0.0f, 0.0f, 0.0f};
struct vec3 grey =  (struct vec3){0.5f, 0.5f, 0.5f};
struct vec3 lightgrey = (struct vec3){0.75f, 0.75f, 0.75f};
struct vec3 lightlight = (struct vec3){0.875f, 0.875f, 0.875f};
struct vec3 white = (struct vec3){1.0f, 1.0f, 1.0f};
struct vec3 red =   (struct vec3){1.0f, 0.0f, 0.0f};
struct vec3 green = (struct vec3){0.0f, 1.0f, 0.0f};
struct vec3 blue =  (struct vec3){0.0f, 0.0f, 1.0f};
struct vec3 yellow = (struct vec3){1.0f, 1.0f, 0.0f};
struct vec3 magenta = (struct vec3){1.0f, 0.0f, 1.0f};
struct vec3 cyan = (struct vec3){0.0f, 1.0f, 1.0f};

struct vec3 skyBtmCol = (struct vec3){1.0f, 1.0f, 1.0f};
struct vec3 skyTopCol = (struct vec3){0.5, 0.7, 1.0f}; 
struct vec3 nightskyBtmCol = (struct vec3){1.0f, 1.0f, 1.0f};
struct vec3 nightskyTopCol = (struct vec3){0.012, 0.02f, 0.2f};


#endif
