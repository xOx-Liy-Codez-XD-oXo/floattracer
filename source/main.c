#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include "ogc/lwp_watchdog.h"

#define liym_min(a, b) ((a) < (b) ? (a) : (b))
#define liym_max(a, b) ((a) > (b) ? (a) : (b))

#include "liytracer.h"
#include "vec3.h"
#include "ray.h"
#include "color.h"
#include "tri.h"

#include "utililiys.h"
#include "liyt.h"
#include "primitives.h"

#include "icosphere_liym3q.h"

#define DEFAULT_FIFO_SIZE	(256*1024)

void *xfb = NULL;
GXRModeObj rmode;
u8 blurfilter[7] = {9, 9, 9, 10, 9, 9, 9};

#define SPHERECOUNT 4
#define PLANECOUNT 1
#define LIGHTCOUNT 1

unsigned long rframe = 0;
struct vec3 skycol;
float skylerp = 0.5f;
float spherez[SPHERECOUNT];
float spherex[SPHERECOUNT];
int spherer[SPHERECOUNT] = {1, 2, 5, 18};
int rayBounces;
int accummode;
unsigned long accumtime;
unsigned long accumlength;
float lightr[LIGHTCOUNT] = {0.000f};

//10ths and 8ths and 3rds
//                    0     1       2       3     4      5     6       7       8     9     10      11      12    13     14    15      16    17    18
float routable[19] = {0.0f, 0.001f, 0.1f, 0.125f, 0.2f, 0.25f, 0.3f, 0.333f, 0.375f, 0.4f, 0.5f, 0.625f, 0.666f, 0.7f, 0.75f, 0.8f, 0.825f, 0.9f, 1.0f};
float roureciprocaltable[19 * LIGHTCOUNT];
#define SHADELUTSIZE 256
float shadelut[LIGHTCOUNT][19][SHADELUTSIZE];

struct pixelbounder {
	u16 top;
	u16 bottom;
	u16 left;
	u16 right;
};

int inbounds1d(u16 a, u16 b, u16 x) {
	if(x >= a && x <= b) return 1;
	return 0;
}

//spheres, then planes
#define BOUNDCOUNT (SPHERECOUNT + 1)
struct pixelbounder objbounds[BOUNDCOUNT];
u8 pixelbounder_inverticalbound[BOUNDCOUNT];
u8 pixelbounder_inbound[BOUNDCOUNT];
u8 pixelbounder_impossible[BOUNDCOUNT];
int rowpossible;
int pixelpossible;
struct vec3 (*pixelfunc)(struct ray r);

struct tdata hitsphere(struct vec3 center, int rou, struct ray r) { //Radius forced 1
	//struct ray roriginal = r;

	struct vec3 oc = vecSubVecByVec(center, r.origin);
	float a = vecLengthSquared(r.direction);
	float h = vecDotProduct(r.direction, oc);
	float c = vecLengthSquared(oc) - 1.0f; //(radius * radius);
	float discriminant = (h*h) - a*c;

	struct tdata hstdat;
	if (discriminant < 0) {
		hstdat.t = -1.0f;
		return hstdat;
	} else {
		hstdat.t = (h - shitsqrt(discriminant)) / a;
		hstdat.hitloc = rayPosAt(r, hstdat.t);
		hstdat.N = vecSubVecByVec( hstdat.hitloc, center);
		hstdat.rouindex = rou;
		hstdat.col = lightlight;
		return hstdat;
	}
}

struct tdata hitPlane(double center, struct vec3 nor, struct ray r) {
	float denom = vecDotProduct(nor, r.direction);
	struct tdata hptdat;
	hptdat.t = -1.0f;
	
	if (-denom < 1e-4f) {
		return hptdat;
	}

	hptdat.t = -1.0f * (vecDotProduct(nor,r.origin) + center) / vecDotProduct(nor, r.direction);

	if (hptdat.t < 1e-4f) {
		hptdat.t = -1.0f;
		return hptdat;
	}
	hptdat.hitloc = rayPosAt(r,hptdat.t);
	hptdat.N = nor;
	hptdat.rouindex = (((int)fabsf(hptdat.hitloc.x)) & 1) ?
	                  	((((int)fabsf(hptdat.hitloc.y)) & 1) ? 9 : 0) 
	                  :
	                  	((((int)fabsf(hptdat.hitloc.y)) & 1) ? 0 : 9);
	//hptdat.rouindex = 18;
	hptdat.col = lightlight;
	
	return hptdat;
}

struct tdata hitAny(struct ray r, struct tdata closestTdat) {
	struct tdata k;	

	for(int i = 0; i < SPHERECOUNT; i++) {
		k = hitsphere((struct vec3){spherex[i], 0.0f, spherez[i]}, spherer[i], r);
		if (k.t < closestTdat.t && k.t > 0.000001f)
			closestTdat = k;
	}
	//return closestTdat;
	k = hitPlane(0.0f, (struct vec3){0.0f, 0.0f, 1.0f}, r);
	if (k.t < closestTdat.t && k.t > 0.000001f)
		closestTdat = k;

	return closestTdat;
}

struct tdata hitAnyPrepass(struct ray r, struct tdata closestTdat) {
	struct tdata k;	

	for(int i = 0; i < SPHERECOUNT; i++) {
		if(!pixelbounder_inbound[i]) continue;
		k = hitsphere((struct vec3){spherex[i], 0.0f, spherez[i]}, spherer[i], r);
		if (k.t < closestTdat.t && k.t > 0.000001f)
			closestTdat = k;
	}
	//return closestTdat;
	//first after the spheres
	if(!pixelbounder_inbound[SPHERECOUNT]) return closestTdat;
	k = hitPlane(0.0f, (struct vec3){0.0f, 0.0f, 1.0f}, r);
	if (k.t < closestTdat.t && k.t > 0.000001f)
		closestTdat = k;

	return closestTdat;
}

int hitAnyOccluding(struct ray r, float t) {
	struct tdata k;
	for(int i = 0; i < SPHERECOUNT; i++) {
		k = hitsphere((struct vec3){spherex[i], 0.0f, spherez[i]}, spherer[i], r);
		if (k.t < t && k.t > 0.000001f)
			return 1;
	}
	k = hitPlane(0.0f, (struct vec3){0.0f, 0.0f, 1.0f}, r);
	if (k.t < t && k.t > 0.000001f)
		return 1;

	return 0;
}

struct vec3 rayColorAfter(struct ray r);

#define raycol(this, after, hit) \
struct vec3 this(struct ray r) {\
	if (rayBounces < 16) {\
		struct tdata closestTdat;\
		closestTdat.t = 99999999.9f;\
\
		closestTdat = hit(r, closestTdat);\
\
		if (closestTdat.t < 999999.9f) {\
			/*return closestTdat.col;*/\
\
			struct ray roughRay;\
			roughRay.origin = closestTdat.hitloc;\
\
			struct vec3 reflec;\
			reflec = vecSubVecByVec(r.direction, vecMulVecByFloat(closestTdat.N, 2.0f * vecDotProduct(r.direction, closestTdat.N))  );\
\
			roughRay.direction =  vecAddVecToVec(vecMulVecByFloat(vecAddVecToVec(randomUnitVector(), closestTdat.N), routable[closestTdat.rouindex]), /*lambert and lerp*/\
							     vecMulVecByFloat(reflec, (1.0f - routable[closestTdat.rouindex]))) ;\
			/*roughRay.direction = vecUnitVector(roughRay.direction);*/\
\
			rayBounces++;	\
			struct vec3 ocolor = vecMulVecByVec(after(roughRay), closestTdat.col);\
			\
\
\
			/*Local lighting*/\
\
			struct vec3 lampContributionTotal = black;\
			struct vec3 lampContribution;\
			\
			for(int i = 0; i < 1; i++) {\
				struct vec3 lightSampleLoc = (struct vec3){-2.0f, -2.0f, 4.0f};\
				/*float lightradius = lightr[i];*/\
\
				/*fuzz light.. not worth for so low resolution*/\
				/*lightSampleLoc = vecAddVecToVec(lightSampleLoc, vecMulVecByFloat(randomUnitVector(), lightr[i]));*/\
\
				float distToLight = vecLength(vecSubVecByVec(lightSampleLoc, closestTdat.hitloc));\
				closestTdat.t = distToLight;\
				struct ray lightRay;\
				lightRay.direction = vecUnitVector(vecSubVecByVec(lightSampleLoc, closestTdat.hitloc));\
				lightRay.origin = closestTdat.hitloc;\
\
				float lightbright = 30.0f;\
\
				int shadowed = hitAnyOccluding(lightRay, closestTdat.t);\
				if (unlikely(shadowed)) {\
					continue;\
				} else {\
					float ndotl = vecDotProduct(closestTdat.N, lightRay.direction);\
					if(ndotl > 0.999f) ndotl = 0.999f;\
					if(ndotl > 0.001f) {\
						float atten = reciprocal(distToLight * distToLight) * lightbright;\
						int lutshade = (int)(ndotl * (float)(SHADELUTSIZE - 1));\
						float shaped = shadelut[i][closestTdat.rouindex][lutshade];\
						float influence = atten * shaped;\
						lampContribution = vecMulVecByFloat(white, influence);\
					} else {\
						continue;\
					}\
				}\
\
				lampContributionTotal = vecAddVecToVec(lampContributionTotal, lampContribution);\
			}\
\
			lampContributionTotal = vecMulVecByVec(closestTdat.col, lampContributionTotal);\
\
			ocolor = vecAddVecToVec(lampContributionTotal, ocolor);\
\
			return ocolor;\
		}\
\
		/*we missed everything! check the sky*/\
\
		float a = 0.5f * r.direction.z + 1.0f;\
\
		struct vec3 out = vecAddVecToVec(vecMulVecByFloat( white, (1.0f - a) ), vecMulVecByFloat( skycol, a ));\
		return out;\
	} else {\
		return black;\
	}\
}\

raycol(rayColor, rayColorAfter, hitAnyPrepass)
raycol(rayColorAfter, rayColorAfter, hitAny)

struct vec3 skycolor(struct ray r) {
	float a = 0.5f * r.direction.z + 1.0f;
	return vecAddVecToVec(vecMulVecByFloat( white, (1.0f - a) ), vecMulVecByFloat( skycol, a ));
}


int main(int argc,char **argv) {
	VIDEO_Init();
	//WPAD_Init();
	PAD_Init();

	rmode = *(VIDEO_GetPreferredMode(NULL));

	int accumwif = 640;
	int fastwif = 288;
	int wif = fastwif;
	//height 226 fixed

	//if one of these is odd the other has to be even
	//unless one of them is 1, then the other can be any number
	//they can both be 1 also
	int horizontalinterlace = 3;
	int heightinterlace = 2;

	xfb = MEM_K0_TO_K1(memalign(32, 640*452*2));
	void * prepassDummybuffer = memalign(32, 640*226);
	
	SYS_STDIO_Report(true);
	//console_init(xfb, 0, 0, wif, 226, wif*VI_DISPLAY_PIX_SZ);

	rmode.viWidth = 648;
	rmode.fbWidth = 480;
	rmode.viXOrigin = (720-648)/2;
	rmode.viYOrigin = (480-452)/2;
	rmode.efbHeight = 226;
	rmode.xfbHeight = 452;
	VIDEO_Configure(&rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode.viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

	void *gp_fifo = NULL;
	gp_fifo = memalign(32,DEFAULT_FIFO_SIZE);
	memset(gp_fifo,0,DEFAULT_FIFO_SIZE);
	GX_Init(gp_fifo,DEFAULT_FIFO_SIZE);

	GX_SetCopyClear((GXColor){0, 0, 0, 255}, 0x00ffffff);

	u8 usecopyfilter = 0;
	GX_SetDispCopySrc(0, 0, wif, 226);
	GX_SetDispCopyDst(wif, 452);
	GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);
	GX_SetDispCopyYScale(2.0f);

	GX_CopyDisp(xfb, GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);
	GX_SetColorUpdate(GX_TRUE);

	fillShitrand();


	//Prepass stuf

	//Clear sections
	GX_SetNumTevStages(1);
	GX_SetCopyClear((GXColor){0, 0, 0, 255}, 0x00ffffff);
	GX_SetTexCopySrc(0, 0, 640, 226);
	GX_SetTexCopyDst(640, 226, GX_TF_I4, GX_FALSE);
	GX_CopyTex(prepassDummybuffer, GX_TRUE); 
	GX_SetTexCopySrc(0, 226, 640, 226);
	GX_CopyTex(prepassDummybuffer, GX_TRUE); 
	GX_PixModeSync();

	GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_ENABLE);
	GX_SetCullMode(GX_CULL_NONE);
	

	GX_SetCurrentMtx(GX_PNMTX0);


	//Trace deets

	float aspect = 1.3333;
	#ifdef HW_RVL
	if (CONF_GetAspectRatio() == CONF_ASPECT_16_9)
		aspect = 16.0f/9.0f; else
		aspect = 4.0f/3.0f;
	#endif

	float focallength = 1.0f;
	float viewportheight = 2.0f;
	float viewportwidth = viewportheight * aspect;
	struct vec3 cam = (struct vec3){0.0f, -5.0f, 1.0f};
	struct vec3 camrot = (struct vec3){1.57f, 0.0f, 0.0f};

	u64 frametime;

	for(int i = 0; i < LIGHTCOUNT; i++)
		for(int j = 0; j < 19; j++) {
			roureciprocaltable[j] = 1.0f / (routable[j] + lightr[i]);
			for(int k = 0; k < SHADELUTSIZE; k++)
				shadelut[i][j][k] = powf((float)k / (float)(SHADELUTSIZE - 1), roureciprocaltable[j]);
		}

	for(int i = 0; i < SPHERECOUNT; i++) spherex[i] = ((float)i * 2.0f) - 3.0f;

	while(1) {
		u64 startframe = gettime();

		//WPAD_ScanPads();
		//if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) exit(0);

		u32 gcconnected = PAD_ScanPads();
		u32 whichcon = 0; //use first connected controler
		if(gcconnected & 1) whichcon = 0; 
		else if (gcconnected & 2) whichcon = 1;
		else if (gcconnected & 4) whichcon = 2;
		else if (gcconnected & 8) whichcon = 3;
		int gbuttonsheld = 0;
		int gbuttonsdown = 0;
		if(gcconnected) {
			float movespeed = 0.08f;
			float looksens = 0.05f;
			float oneslashonetwentyeight = 1.0f / 128.0f;
			gbuttonsheld = PAD_ButtonsHeld(whichcon);
			gbuttonsdown = PAD_ButtonsDown(whichcon);
			if(gbuttonsdown & PAD_BUTTON_START) break;

			if(gbuttonsheld & PAD_TRIGGER_Z) {
				movespeed = 1.0f;
				looksens = 0.15f;
			}

			camrot.x += (float)PAD_SubStickY(whichcon) * oneslashonetwentyeight * looksens / focallength;
			camrot.z -= (float)PAD_SubStickX(whichcon) * oneslashonetwentyeight * looksens / focallength;

			//updown
			cam.y += movespeed * cos(-camrot.z) * oneslashonetwentyeight * (float)PAD_StickY(whichcon);
			cam.x += movespeed * sin(-camrot.z) * oneslashonetwentyeight * (float)PAD_StickY(whichcon);
			//lr
			cam.y += movespeed * sin(camrot.z) * oneslashonetwentyeight * (float)PAD_StickX(whichcon);
			cam.x += movespeed * cos(camrot.z) * oneslashonetwentyeight * (float)PAD_StickX(whichcon);

			if(gbuttonsheld & PAD_BUTTON_Y) {
				if(gbuttonsheld & PAD_TRIGGER_R) skylerp -= 0.01f;
				if(gbuttonsheld & PAD_TRIGGER_L) skylerp += 0.01f;
			}

			if(gbuttonsheld & PAD_BUTTON_X) {
				if(gbuttonsheld & PAD_TRIGGER_R) focallength *= 1 + (movespeed * 0.5f);
				if(gbuttonsheld & PAD_TRIGGER_L) focallength /= 1 + (movespeed * 0.5f);
			}
			if(gbuttonsheld & PAD_BUTTON_A) {
				if(gbuttonsheld & PAD_TRIGGER_R) cam.z += movespeed * 0.5f;
				if(gbuttonsheld & PAD_TRIGGER_L) cam.z -= movespeed * 0.5f;
			}

			if(gbuttonsheld & PAD_BUTTON_B) {
				if(gbuttonsdown & PAD_TRIGGER_L) {
					accummode ^= 1;
					accumtime = rframe;
					accumlength = 0;
				}
				if(gbuttonsdown & PAD_TRIGGER_R) {
					usecopyfilter ^= 1;
					if(usecopyfilter)
						GX_SetCopyFilter(GX_FALSE, NULL, GX_TRUE, blurfilter);
					else 
						GX_SetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);
				}
			}

			if(gbuttonsdown & PAD_BUTTON_START) exit(0);
		}



		//scene

		skycol = vecAddVecToVec(vecMulVecByFloat( skyTopCol, (1.0f - skylerp) ), vecMulVecByFloat( nightskyTopCol, skylerp));

		float faralong;
		if(accummode) faralong = (float)accumtime * 0.02f; else faralong = (float)rframe * 0.02f;
		for(int i = 0; i < SPHERECOUNT; i++) {
			spherez[i] = (sinf(faralong + ((float)i * 1.0f)) + 1.0f) * 0.5f;
		}

		wif = accummode ? accumwif : fastwif;

		rmode.viWidth = 648;
		rmode.fbWidth = wif;
		rmode.viXOrigin = (720-648)/2;
		rmode.viYOrigin = (480-452)/2;
		rmode.efbHeight = 226;
		rmode.xfbHeight = 452;

		GX_SetDispCopySrc(0, 0, wif, 226);
		GX_SetDispCopyDst(wif, 452);
		GX_SetDispCopyYScale(2.0f);

		//raster prepass .. speedup for offscreen primitives

		u64 startprepasstime = gettime();

		//reset impossibles
		//impossible checking is faster on scenes with many offscreen primitives
		for(int i = 0; i < BOUNDCOUNT; i++) {
			pixelbounder_impossible[i] = 0;
		}

		GX_SetViewport(0, 0, wif, 226, 0, 1);
		GX_SetScissor(0, 0, wif, 226);
		GX_SetScissorBoxOffset(0, -226);
		
		Mtx44 perspective;
		Mtx view, model;
		
		float rasfov = 2.0f * atan2f(viewportheight * 0.5f, focallength) * (180.0f / 3.14159265);
		guPerspective(perspective, rasfov, aspect, 0.01F, 300.0F);
		GX_LoadProjectionMtx(perspective, GX_PERSPECTIVE);
		liyt_genMtxPosRotZyx(view, cam.x, cam.y, cam.z, camrot.x, camrot.y, camrot.z);
		guMtxInverse(view, view);

		//Storage is spheres then planes but we do planes first for better z rejection

		//Plane
		GX_ClearBoundingBox();
		guMtxIdentity(model);
		guMtxConcat(view, model, model);
		GX_LoadPosMtxImm(model, GX_PNMTX0);
		drawBigPlane();
		GX_DrawDone();
		GX_Flush(); //using SPHERECOUNT because the plane is stored right after the spheres
		GX_ReadBoundingBox(&objbounds[SPHERECOUNT].top, &objbounds[SPHERECOUNT].bottom, &objbounds[SPHERECOUNT].left, &objbounds[SPHERECOUNT].right);
		if(objbounds[SPHERECOUNT].top > objbounds[SPHERECOUNT].bottom) pixelbounder_impossible[SPHERECOUNT] = 1;
		if(objbounds[SPHERECOUNT].left) 
			objbounds[SPHERECOUNT].left -= 1;
		objbounds[SPHERECOUNT].right += 1;
		objbounds[SPHERECOUNT].top -= 226;
		if(objbounds[SPHERECOUNT].bottom)
			objbounds[SPHERECOUNT].bottom -= 226;

		//Spheres
		ag_config_icosphere(0);
		for(int i = 0; i < SPHERECOUNT; i++) {
			GX_ClearBoundingBox();
			liyt_genMtxPosRotZyx(model, spherex[i], 0.0f, spherez[i], 0.0f, 0.0f, 0.0f);
			guMtxConcat(view, model, model);
			GX_LoadPosMtxImm(model, GX_PNMTX0);
			ag_draw_icosphere(0);
			GX_DrawDone();
			GX_Flush();
			GX_ReadBoundingBox(&objbounds[i].top, &objbounds[i].bottom, &objbounds[i].left, &objbounds[i].right);
			if(objbounds[i].top > objbounds[i].bottom) pixelbounder_impossible[i] = 1;
			if(objbounds[i].left) 
				objbounds[i].left -= 1;
			objbounds[i].right += 1;
			objbounds[i].top -= 226;
			if(objbounds[i].bottom)
				objbounds[i].bottom -= 226;
		}
		
		//Clear
		GX_SetTexCopySrc(0, 226, wif, 226);
		GX_SetTexCopyDst(wif, 226, GX_TF_I4, GX_FALSE);
		GX_CopyTex(prepassDummybuffer, GX_TRUE); 
		GX_PixModeSync();

		//for(int i = 0; i < BOUNDCOUNT; i++) printf("%d, %d, %d, %d\n", objbounds[i].top, objbounds[i].bottom, objbounds[i].left, objbounds[i].right); putchar('\n');

		u64 prepasstime = gettime() - startprepasstime;
		float prepasstimems = (float)ticks_to_microsecs(prepasstime) / 1000.0f;



		//tracecam setup

		struct vec3 viewportu = (struct vec3){viewportwidth, 0.0f, 0.0f};
		struct vec3 viewportv = (struct vec3){0.0f, -viewportheight, 0.0f};
		struct vec3 foclengthzvec = (struct vec3){0.0f, 0.0f, focallength};
	
		viewportu = rotVec(viewportu, camrot);
		viewportv = rotVec(viewportv, camrot);
		foclengthzvec = rotVec(foclengthzvec, camrot);

		struct vec3 viewportupperleft = cam;
		viewportupperleft = vecSubVecByVec(viewportupperleft, foclengthzvec);
		viewportupperleft = vecSubVecByVec(viewportupperleft, vecMulVecByFloat(viewportu, 0.5f));
		viewportupperleft = vecSubVecByVec(viewportupperleft, vecMulVecByFloat(viewportv, 0.5f));

		struct vec3 pixeldeltau = vecDivVecByFloat(viewportu, (float)wif);
		struct vec3 pixeldeltav = vecDivVecByFloat(viewportv, 226.0f);
		
		struct vec3 pixelorigin = vecAddVecToVec(viewportupperleft, vecMulVecByFloat(vecAddVecToVec(pixeldeltau, pixeldeltav), 0.5f));

		struct ray r;
		r.origin = cam;

		int shouldrender = accummode && accumlength > 0;
		int ynterlace, xnterlace;
		if(accummode) {
			ynterlace = 1;
			xnterlace = 1;
		} else {
			ynterlace = heightinterlace;
			xnterlace = horizontalinterlace;
		}

		for(int y = rframe % ynterlace; y < 226; y += ynterlace) {
			rowpossible = 0;
			for(int i = 0; i < BOUNDCOUNT; i++) {
				pixelbounder_inverticalbound[i] = inbounds1d(objbounds[i].top, objbounds[i].bottom, y);
				if(!rowpossible)
					rowpossible = pixelbounder_inverticalbound[i] ? 1 : 0;
			}
			struct vec3 linepos = vecMulVecByFloat(pixeldeltav, (float)y);
			if(rowpossible) pixelfunc = rayColor; else pixelfunc = skycolor;

			for(int x = rframe % xnterlace; x < wif; x += xnterlace) {
				pixelpossible = 0;
				struct vec3 pixelcenter = vecAddVecToVec(pixelorigin, vecAddVecToVec(vecMulVecByFloat(pixeldeltau, (float)x), linepos));
				r.direction = vecSubVecByVec(pixelcenter, cam);
				if(accummode) {
					r.direction = vecAddVecToVec(r.direction, vecMulVecByFloat(randomVectorOfUnits(), 0.005));
				}
				r.direction = vecUnitVector(r.direction);

				if(likely(rowpossible))
				for(int i = 0; i < BOUNDCOUNT; i++) {
					if(pixelbounder_impossible[i]) {
						pixelbounder_inbound[i] = 0;
						continue;
					}
					pixelbounder_inbound[i] = inbounds1d(objbounds[i].left, objbounds[i].right, x);
					pixelbounder_inbound[i] &= pixelbounder_inverticalbound[i];
					//pixelbounder_inbound[i] = 0;
					if(!pixelpossible)
						pixelpossible = pixelbounder_inbound[i] ? 1 : 0;
				}
				if(pixelpossible) pixelfunc = rayColor; else pixelfunc = skycolor;

				rayBounces = 0;
				if(shouldrender) {
					GXColor newcol = colortogxcol(pixelfunc(r)), oldcol;
					GX_PeekARGB(x, y, &oldcol);
					newcol = gxcol_lerp(oldcol, newcol, reciprocal((float)accumlength));
					GX_PokeARGB(x, y, newcol);
				} else GX_PokeARGB(x, y, colortogxcol(pixelfunc(r)));
			}
		}

		GX_Flush();
		GX_CopyDisp(xfb, GX_FALSE);
		GX_PixModeSync();
		frametime = gettime() - startframe;
		float frametimems = (float)ticks_to_microsecs(frametime) / 1000.0f;
		//printf("\x1b[0;0H");
		printf("%.2f ", prepasstimems);
		printf("%.2f\n", frametimems);
		//fflush(stdout);

		VIDEO_Configure(&rmode);
		VIDEO_Flush();

		//VIDEO_WaitVSync();
		accumlength++;
		rframe++;
	}
	return 0;
}
