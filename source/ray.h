#ifndef RAY_H
#define RAY_H

struct ray {
	struct vec3 origin;
	struct vec3 direction;
	struct vec3 direction_inv;
};

struct vec3 rayPosAt(struct ray ray, float t) {
	return vecAddVecToVec(ray.origin, vecMulVecByFloat(ray.direction, t));
}

struct tdata {
	float t;
	struct vec3 hitloc;
	struct vec3 N;
	struct vec3 col;
	int rouindex;
};

struct ray rayRot(struct ray r, float rx, float ry, float rz) {
	struct ray out;
	float nx, ny, nz, nnx, nny, nnz;

	nx = (r.origin.x * cos(-rz)) - (r.origin.y * sin(-rz));
	ny = (r.origin.x * sin(-rz)) + (r.origin.y * cos(-rz));
	nz = r.origin.z;
	
	nnx = nx;
	nny = (ny * cos(-rx)) - (nz * sin(-rx));
	nnz = (ny * sin(-rx)) + (nz * cos(-rx));
	
	out.origin.x = (nnz * sin(-ry)) + (nnx * cos(-ry));
	out.origin.y = nny;
	out.origin.z = (nnz * cos(-ry)) - (nnx * sin(-ry));

	nx = (r.direction.x * cos(rz)) - (r.direction.y * sin(rz));
	ny = (r.direction.x * sin(rz)) + (r.direction.y * cos(rz));
	nz = r.direction.z;
	
	nnx = nx;
	nny = (ny * cos(rx)) - (nz * sin(rx));
	nnz = (ny * sin(rx)) + (nz * cos(rx));
	
	out.direction.x = (nnz * sin(ry)) + (nnx * cos(ry));
	out.direction.y = nny;
	out.direction.z = (nnz * cos(ry)) - (nnx * sin(ry));

	return out;
}

struct ray rayStr(struct ray r, float sx, float sy, float sz) {
	struct ray out;
	out.direction.x = r.direction.x * sx;
	out.direction.y = r.direction.y * sy;
	out.direction.z = r.direction.z * sz;

	out.origin.x = r.origin.x * sx;
	out.origin.y = r.origin.y * sy;
	out.origin.z = r.origin.z * sz;

	return out;
}
#endif
