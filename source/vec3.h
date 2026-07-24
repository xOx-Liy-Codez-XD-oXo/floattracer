#ifndef VEC3_H
#define VEC3_H

struct vec3 {
	float x;
	float y;
	float z;
};

static inline float vecLengthSquared(struct vec3 vec) { //faster when zyx
	return vec.z*vec.z
	     + vec.y*vec.y
	     + vec.x*vec.x; 
}

static inline float vecLength(struct vec3 vec) {
	return shitsqrt(vecLengthSquared(vec));
}

static inline float vecInverseLength(struct vec3 vec) {
	return rsqrt(vecLengthSquared(vec));
}

static inline struct vec3 vecAddVecToVec(struct vec3 vecA, struct vec3 vecB) {
	struct vec3 vecC; //zyx very slow
	vecC.x = vecA.x + vecB.x;
	vecC.y = vecA.y + vecB.y;
	vecC.z = vecA.z + vecB.z;
	return vecC;
}

static inline struct vec3 vecAddFloatToVec(struct vec3 vecA, float add) {
	struct vec3 vecC;
	vecC.x = vecA.x + add;
	vecC.y = vecA.y + add;
	vecC.z = vecA.z + add;
	return vecC;
}

static inline struct vec3 vecSubVecByVec(struct vec3 vecA, struct vec3 vecB) {
	struct vec3 vecC;
	vecC.x = vecA.x - vecB.x;
	vecC.y = vecA.y - vecB.y;
	vecC.z = vecA.z - vecB.z;
	return vecC;
}

static inline struct vec3 vecMulVecByFloat(struct vec3 vec, float mul) {// unsure
	struct vec3 vecC;
	vecC.z = vec.z * mul;
	vecC.y = vec.y * mul;
	vecC.x = vec.x * mul;
	return vecC;
}

struct vec3 vecMulVecByVec(struct vec3 vec, struct vec3 mul) {
	struct vec3 vecC; //zyx slower
	vecC.x = vec.x * mul.x;
	vecC.y = vec.y * mul.y;
	vecC.z = vec.z * mul.z;
	return vecC;
}

/*
void vecMulVecByVec_ps(struct vec3 * out, struct vec3 * vecA, struct vec3 * vecB) {
	//imagine it multiply downward
	//vecA.x, vecA.y
	//vecB.x, vecB.y
	//loading the first 8 bytes of each struct arranges xy correcly
	
	asm (
		"psq_l 6, 0(%0), 0, 0\n\t"
		"psq_l 7, 0(%1), 0, 0\n\t"
		"ps_mul 6,7,6\n\t"
		"psq_st 6, 0(%2), 0, 0\n\t"
		: 
		: "b"(vecA), "b"(vecB), "b"(out)
		: "fr6", "fr7", "cc"
	);
	out->z = vecA->z * vecB->z;
	return;
}
*/

static inline struct vec3 vecDivVecByVec(struct vec3 vec, struct vec3 div) {
	struct vec3 vecC; //zyx better
	vecC.z = vec.z / div.z;
	vecC.y = vec.y / div.y;
	vecC.x = vec.x / div.x;
	return vecC;
}

static inline struct vec3 vecInverse(struct vec3 vec) { //not use
	struct vec3 vecC;
	vecC.x = 1.0f / vec.x;
	vecC.y = 1.0f / vec.y;
	vecC.z = 1.0f / vec.z;
	return vecC;
}

static inline struct vec3 vecDivVecByFloat(struct vec3 vec, float div) {
	struct vec3 vecC; //xyz slower
	vecC.x = vec.x / div;
	vecC.y = vec.y / div;
	vecC.z = vec.z / div;
	return vecC;
}

static inline float vecDotProduct(struct vec3 vecA, struct vec3 vecB) { //faster when zyx
	return vecA.z * vecB.z
	     + vecA.y * vecB.y
	     + vecA.x * vecB.x;
	//return vecA.x * vecB.x
	//     + vecA.z * vecB.z
	//     + vecA.y * vecB.y;
}

static inline struct vec3 vecUnitVector(struct vec3 vec) {
	return vecMulVecByFloat(vec, vecInverseLength(vec));
}

static inline struct vec3 randomVec3() {
	struct vec3 out;
	out.x = randomFloat();
	out.y = randomFloat();
	out.z = randomFloat();
	return out;
}

static inline struct vec3 randomVec3Minmax(float min, float max) {
	struct vec3 out;
	out.x = randomFloatMinmax(min, max);
	out.y = randomFloatMinmax(min, max);
	out.z = randomFloatMinmax(min, max);
	return out;
}

static inline struct vec3 randomVectorOfUnits() {
	struct vec3 out;
	out.x = randomUnitFloat();
	out.y = randomUnitFloat();
	out.z = randomUnitFloat();
	return out;
}

static inline struct vec3 randomUnitVector() {
	struct vec3 out;
	out.x = randomUnitFloat();
	out.y = randomUnitFloat();
	out.z = randomUnitFloat();
	return vecUnitVector(out);
}

static inline struct vec3 randomOnHemisphere(struct vec3 normal) {
	struct vec3 onUnitSphere = randomUnitVector();
	if (vecDotProduct(onUnitSphere, normal) > 0.0f) { // in the same hemisphere as the normal
		return onUnitSphere;
	} else {
		return (struct vec3){-onUnitSphere.x, -onUnitSphere.y, -onUnitSphere.z};
	}
}

static inline struct vec3 rotVec(struct vec3 vec, struct vec3 rot) {
	struct vec3 out;
	
	float sinrx = sinf(rot.x);
	float sinry = sinf(rot.y);
	float sinrz = sinf(rot.z);
	float cosrx = cosf(rot.x);
	float cosry = cosf(rot.y);
	float cosrz = cosf(rot.z);

	float nx, ny, nz, nnx, nny, nnz;

	nx = vec.x;
	ny = (vec.y * cosrx) - (vec.z * sinrx);
	nz = (vec.y * sinrx) + (vec.z * cosrx);

	nnx = (nz * sinry) + (nx * cosry);
	nny = ny;
	nnz = (nz * cosry) - (nx * sinry);

	out.x = (nnx * cosrz) - (nny * sinrz);
	out.y = (nnx * sinrz) + (nny * cosrz);
	out.z = nnz;

	return out;
}

static inline struct vec3 rotVecEdgecase(struct vec3 vec, struct vec3 rot) { // fuck
	struct vec3 out;
	
	float sinrx = sin(-rot.x);
	float sinry = sin(rot.y);
	float sinrz = sin(rot.z);
	float cosrx = cos(-rot.x);
	float cosry = cos(rot.y);
	float cosrz = cos(rot.z);

	float nx, ny, nz, nnx, nny, nnz;

	nx = vec.x;
	ny = (vec.y * cosrx) - (vec.z * sinrx);
	nz = (vec.y * sinrx) + (vec.z * cosrx);

	nnx = (nz * sinry) + (nx * cosry);
	nny = ny;
	nnz = (nz * cosry) - (nx * sinry);

	out.x = (nnx * cosrz) - (nny * sinrz);
	out.y = (nnx * sinrz) + (nny * cosrz);
	out.z = nnz;

	return out;
}

	/*
	
	Z Y X order

	nx = (vec.x * cosrz) - (vec.y * sinrz);
	ny = (vec.x * sinrz) + (vec.y * cosrz);
	nz = vec.z;

	nnx = (nz * sinry) + (nx * cosry);
	nny = ny;
	nnz = (nz * cosry) - (nx * sinry);

	out.x = nnx;
	out.y = (nny * cosrx) - (nnz * sinrx);
	out.z = (nny * sinrx) + (nnz * cosrx);

	*/

static inline struct vec3 rotVecZyx(struct vec3 vec, struct vec3 rot) {
	struct vec3 out;
	
	float sinrx = sin(rot.x);
	float sinry = sin(rot.y);
	float sinrz = sin(rot.z);
	float cosrx = cos(rot.x);
	float cosry = cos(rot.y);
	float cosrz = cos(rot.z);

	float nx, ny, nz, nnx, nny, nnz;

	nx = (vec.x * cosrz) - (vec.y * sinrz);
	ny = (vec.x * sinrz) + (vec.y * cosrz);
	nz = vec.z;

	nnx = (nz * sinry) + (nx * cosry);
	nny = ny;
	nnz = (nz * cosry) - (nx * sinry);

	out.x = nnx;
	out.y = (nny * cosrx) - (nnz * sinrx);
	out.z = (nny * sinrx) + (nnz * cosrx);

	return out;
}

static inline struct vec3 rotVecZyxEdgecase(struct vec3 vec, struct vec3 rot) {
	struct vec3 out;
	
	float sinrx = sin(-rot.x);
	float sinry = sin(rot.y);
	float sinrz = sin(rot.z);
	float cosrx = cos(-rot.x);
	float cosry = cos(rot.y);
	float cosrz = cos(rot.z);

	float nx, ny, nz, nnx, nny, nnz;

	nx = (vec.x * cosrz) - (vec.y * sinrz);
	ny = (vec.x * sinrz) + (vec.y * cosrz);
	nz = vec.z;

	nnx = (nz * sinry) + (nx * cosry);
	nny = ny;
	nnz = (nz * cosry) - (nx * sinry);

	out.x = nnx;
	out.y = (nny * cosrx) - (nnz * sinrx);
	out.z = (nny * sinrx) + (nnz * cosrx);

	return out;
}

static inline struct vec3 vecCrossProduct(struct vec3 a, struct vec3 b) {
	struct vec3 out;
	out.x = (a.y * b.z) - (a.z * b.y);
	out.y = (a.z * b.x) - (a.x * b.z);
	out.z = (a.x * b.y) - (a.y * b.x);

	return out;
}


#endif
