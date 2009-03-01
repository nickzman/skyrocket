/*
 * Copyright (C) 1999-2005  Terence M. Welsh
 *
 * This file is part of Skyrocket.
 *
 * Skyrocket is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation.
 *
 * Skyrocket is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


// Skyrocket screen saver


/*#include <windows.h>
#include <stdio.h>
#include <rsWin32Saver/rsWin32Saver.h>
#include <rsText/rsText.h>
#include <math.h>
#include <time.h>
#include <vector>
#include <gl/gl.h>
#include <gl/glu.h>
#include <regstr.h>
#include <commctrl.h>
#include <resource.h>
#include <rsMath/rsMath.h>
#include <Skyrocket/particle.h>
#include <Skyrocket/world.h>*/
//#include "overlay.h"

#include "Skyrocket.h"
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <stdio.h>
#include "rsText.h"
#include <math.h>
#include <time.h>
#include <vector>
#include <list>
#include "resource.h"
#include <stdlib.h>
#include "rsMath.h"
#include "particle.h"
#include "world.h"

// Global variables
//LPCTSTR registryPath = ("Software\\Really Slick\\Skyrocket");
// HDC hdc;
// HGLRC hglrc;
// int readyToDraw = 0;
// list of particles
//std::list<particle> particles;
// Time from one frame to the next
// float elapsedTime = 0.0f;
// Window variables
/*int xsize, ysize, centerx, centery;
float aspectRatio;*/
// Camera variables
/*static rsVec lookFrom[3];  // 3 = position, target position, last position
static rsVec lookAt[3]  // 3 = position, target position, last position
	= {rsVec(0.0f, 1000.0f, 0.0f), 
	rsVec(0.0f, 1000.0f, 0.0f), 
	rsVec(0.0f, 1000.0f, 0.0f)};
rsVec cameraPos;  // used for positioning sounds (same as lookFrom[0])
rsVec cameraVel;  // used for doppler shift
*/
// Mouse variables
/*float mouseIdleTime;
int mouseButtons, mousex, mousey;
float mouseSpeed;*/

// the sound engine
SoundEngine* soundengine = NULL;

// flare display lists
//unsigned int flarelist[4];
// matrix junk for drawing flares in screen space
/*double modelMat[16], projMat[16];
GLint viewport[4];*/

// transformation needed for rendering particles
//float billboardMat[16];

// lifespans for smoke particles
/*float smokeTime[SMOKETIMES];  // lifespans of consecutive smoke particles
int whichSmoke[WHICHSMOKES];  // table to indicate which particles produce smoke
*/
// smoke display lists
//unsigned int smokelist[5];

// the world
//World* theWorld;

// text output
//rsText* textwriter;


/*int numRockets = 0;
std::vector<flareData> lensFlares;
int numFlares = 0;*/
// Parameters edited in the dialog box
/*int dMaxrockets;
int dSmoke;
int dExplosionsmoke;
int dWind;
int dAmbient;
int dStardensity;
int dFlare;
int dMoonglow;
int dMoon;
int dClouds;
int dEarth;
int dIllumination;
int dSound;*/
// Commands given from keyboard
//int kFireworks = 1;
//int kCamera = 1;  // 0 = paused, 1 = autonomous, 2 = mouse control
//int kNewCamera = 0;
//bool kSlowMotion = false;
//int userDefinedExplosion = -1;



/*std::vector<particle> particles;
unsigned int last_particle = 0;
#define ZOOMROCKETINACTIVE 1000000000
unsigned int zoomRocket = ZOOMROCKETINACTIVE;*/



particle* addParticle(SkyrocketSaverSettings *inSettings){
	// Advance to new particle if there is another in the vector.
	// Otherwise, just overwrite the last particle (this will probably never happen)
	if(inSettings->last_particle < inSettings->particles.size())
		++inSettings->last_particle;

	// Return pointer to new particle
	return &(inSettings->particles[inSettings->last_particle-1]);
}


void removeParticle(unsigned int rempart, SkyrocketSaverSettings *inSettings){
	// copy last particle over particle to be removed
	--inSettings->last_particle;
	if(rempart != inSettings->last_particle)
		inSettings->particles[rempart] = inSettings->particles[inSettings->last_particle];

	// correct zoomRocket index if necessary
	if(inSettings->zoomRocket == inSettings->last_particle)
		inSettings->zoomRocket = rempart;
}


void sortParticles(){
	// Sorting doesn't appear to be necessary.  Skyrocket still looks good without it.
}


// Rockets and explosions illuminate smoke
// Only explosions illuminate clouds
void illuminate(particle* ill,SkyrocketSaverSettings * inSettings){
	float temp;
	// desaturate illumination colors
	rsVec newrgb(ill->rgb[0] * 0.6f + 0.4f, ill->rgb[1] * 0.6f + 0.4f, ill->rgb[2] * 0.6f + 0.4f);

	// Smoke illumination
	if((ill->type == ROCKET) || (ill->type == FOUNTAIN)){
		float distsquared;
		for(unsigned int i=0; i<inSettings->last_particle; ++i){
			particle* smk(&(inSettings->particles[i]));
			if(smk->type == SMOKE){
				distsquared = (ill->xyz[0] - smk->xyz[0]) * (ill->xyz[0] - smk->xyz[0])
					+ (ill->xyz[1] - smk->xyz[1]) * (ill->xyz[1] - smk->xyz[1])
					+ (ill->xyz[2] - smk->xyz[2]) * (ill->xyz[2] - smk->xyz[2]);
				if(distsquared < 40000.0f){
					temp = (40000.0f - distsquared) * 0.000025f;
					temp = temp * temp * ill->bright;
					smk->rgb[0] += temp * newrgb[0];
					if(smk->rgb[0] > 1.0f)
						smk->rgb[0] = 1.0f;
					smk->rgb[1] += temp * newrgb[1];
					if(smk->rgb[1] > 1.0f)
						smk->rgb[1] = 1.0f;
					smk->rgb[2] += temp * newrgb[2];
					if(smk->rgb[2] > 1.0f)
						smk->rgb[2] = 1.0f;
				}
			}
		}
	}
	if(ill->type == EXPLOSION){
		float distsquared;
		for(unsigned int i=0; i<inSettings->last_particle; ++i){
			particle* smk(&(inSettings->particles[i]));
			if(smk->type == SMOKE){
				distsquared = (ill->xyz[0] - smk->xyz[0]) * (ill->xyz[0] - smk->xyz[0])
					+ (ill->xyz[1] - smk->xyz[1]) * (ill->xyz[1] - smk->xyz[1])
					+ (ill->xyz[2] - smk->xyz[2]) * (ill->xyz[2] - smk->xyz[2]);
				if(distsquared < 640000.0f){
					temp = (640000.0f - distsquared) * 0.0000015625f;
					temp = temp * temp * ill->bright;
					smk->rgb[0] += temp * newrgb[0];
					if(smk->rgb[0] > 1.0f)
						smk->rgb[0] = 1.0f;
					smk->rgb[1] += temp * newrgb[1];
					if(smk->rgb[1] > 1.0f)
						smk->rgb[1] = 1.0f;
					smk->rgb[2] += temp * newrgb[2];
					if(smk->rgb[2] > 1.0f)
						smk->rgb[2] = 1.0f;
				}
			}
		}
	}

	// cloud illumination
	if(ill->type == EXPLOSION && inSettings->dClouds){
		int north, south, west, east;  // limits of cloud indices to inspect
		int halfmesh = CLOUDMESH / 2;
		float distsquared;
		// remember clouds have 20000-foot radius from the World class, hence 0.00005
		// Hardcoded values like this are evil, but oh well
		south = int((ill->xyz[2] - 1600.0f) * 0.00005f * float(halfmesh)) + halfmesh;
		north = int((ill->xyz[2] + 1600.0f) * 0.00005f * float(halfmesh) + 0.5f) + halfmesh;
		west = int((ill->xyz[0] - 1600.0f) * 0.00005f * float(halfmesh)) + halfmesh;
		east = int((ill->xyz[0] + 1600.0f) * 0.00005f * float(halfmesh) + 0.5f) + halfmesh;
		// bound these values just in case
		if(south < 0) south = 0; if(south > CLOUDMESH-1) south = CLOUDMESH-1;
		if(north < 0) north = 0; if(north > CLOUDMESH-1) north = CLOUDMESH-1;
		if(west < 0) west = 0; if(west > CLOUDMESH-1) west = CLOUDMESH-1;
		if(east < 0) east = 0; if(east > CLOUDMESH-1) east = CLOUDMESH-1;
		//do any necessary cloud illumination
		for(int i=west; i<=east; i++){
			for(int j=south; j<=north; j++){
				distsquared = (inSettings->theWorld->clouds[i][j][0] - ill->xyz[0]) * (inSettings->theWorld->clouds[i][j][0] - ill->xyz[0])
					+ (inSettings->theWorld->clouds[i][j][1] - ill->xyz[1]) * (inSettings->theWorld->clouds[i][j][1] - ill->xyz[1])
					+ (inSettings->theWorld->clouds[i][j][2] - ill->xyz[2]) * (inSettings->theWorld->clouds[i][j][2] - ill->xyz[2]);
				if(distsquared < 2560000.0f){
					temp = (2560000.0f - distsquared) * 0.000000390625f;
					temp = temp * temp * ill->bright;
					inSettings->theWorld->clouds[i][j][6] += temp * newrgb[0];
					if(inSettings->theWorld->clouds[i][j][6] > 1.0f)
						inSettings->theWorld->clouds[i][j][6] = 1.0f;
					inSettings->theWorld->clouds[i][j][7] += temp * newrgb[1];
					if(inSettings->theWorld->clouds[i][j][7] > 1.0f)
						inSettings->theWorld->clouds[i][j][7] = 1.0f;
					inSettings->theWorld->clouds[i][j][8] += temp * newrgb[2];
					if(inSettings->theWorld->clouds[i][j][8] > 1.0f)
						inSettings->theWorld->clouds[i][j][8] = 1.0f;
				}
			}
		}
	}
}


// pulling of other particles
void pulling(particle* suck,SkyrocketSaverSettings * inSettings){
	rsVec diff;
	float pulldistsquared;
	float pullconst = (1.0f - suck->life) * 0.01f * inSettings->frameTime;

	for(unsigned int i=0; i<inSettings->last_particle; ++i){
		particle* puller(&(inSettings->particles[i]));
		diff = suck->xyz - puller->xyz;
		pulldistsquared = diff[0]*diff[0] + diff[1]*diff[1] + diff[2]*diff[2];
		if(pulldistsquared < 250000.0f && pulldistsquared != 0.0f){
			if(puller->type != SUCKER && puller->type != STRETCHER
				&& puller->type != SHOCKWAVE && puller->type != BIGMAMA){
				diff.normalize();
				puller->vel += diff * ((250000.0f - pulldistsquared) * pullconst);
			}
		}
	}
}


// pushing of other particles
void pushing(particle* shock,SkyrocketSaverSettings * inSettings){
	rsVec diff;
	float pushdistsquared;
	float pushconst = (1.0f - shock->life) * 0.002f * inSettings->frameTime;

	for(unsigned int i=0; i<inSettings->last_particle; ++i){
		particle* pusher(&(inSettings->particles[i]));
		diff = pusher->xyz - shock->xyz;
		pushdistsquared = diff[0]*diff[0] + diff[1]*diff[1] + diff[2]*diff[2];
		if(pushdistsquared < 640000.0f && pushdistsquared != 0.0f){
			if(pusher->type != SUCKER && pusher->type != STRETCHER
				&& pusher->type != SHOCKWAVE && pusher->type != BIGMAMA){
				diff.normalize();
				pusher->vel += diff * ((640000.0f - pushdistsquared) * pushconst);
			}
		}
	}
}


// vertical stretching of other particles (x, z sucking; y pushing)
void stretching(particle* stretch,SkyrocketSaverSettings * inSettings){
	rsVec diff;
	float stretchdistsquared, temp;
	float stretchconst = (1.0f - stretch->life) * 0.002f * inSettings->frameTime;

	for(unsigned int i=0; i<inSettings->last_particle; ++i){
		particle* stretcher(&(inSettings->particles[i]));
		diff = stretch->xyz - stretcher->xyz;
		stretchdistsquared = diff[0]*diff[0] + diff[1]*diff[1] + diff[2]*diff[2];
		if(stretchdistsquared < 640000.0f && stretchdistsquared != 0.0f && stretcher->type != STRETCHER){
			diff.normalize();
			temp = (640000.0f - stretchdistsquared) * stretchconst;
			stretcher->vel[0] += diff[0] * temp * 5.0f;
			stretcher->vel[1] -= diff[1] * temp;
			stretcher->vel[2] += diff[2] * temp * 5.0f;
		}
	}
}


// Makes list of lens flares.  Must be a called even when action is paused
// because camera might still be moving.
void makeFlareList(SkyrocketSaverSettings * inSettings){
	rsVec cameraDir, partDir;
	const float shine(float(inSettings->dFlare) * 0.01f);

	cameraDir = inSettings->lookAt[0] - inSettings->lookFrom[0];
	cameraDir.normalize();
	for(unsigned int i=0; i<inSettings->last_particle; ++i){
		particle* curlight(&(inSettings->particles[i]));
		if(curlight->type == EXPLOSION || curlight->type == SUCKER
			|| curlight->type == SHOCKWAVE || curlight->type == STRETCHER
			|| curlight->type == BIGMAMA){
			double winx, winy, winz;
			gluProject(curlight->xyz[0], curlight->xyz[1], curlight->xyz[2],
				inSettings->modelMat, inSettings->projMat, inSettings->viewport,
				&winx, &winy, &winz);
			partDir = curlight->xyz - inSettings->cameraPos;
			if(partDir.dot(cameraDir) > 1.0f){  // is light source in front of camera?
				if(inSettings->numFlares == inSettings->lensFlares.size())
					inSettings->lensFlares.resize(inSettings->lensFlares.size() + 10);
				inSettings->lensFlares[inSettings->numFlares].x = (float(winx) / float(inSettings->xsize)) * inSettings->aspectRatio;
				inSettings->lensFlares[inSettings->numFlares].y = float(winy) / float(inSettings->ysize);
				rsVec vec = curlight->xyz - inSettings->cameraPos;  // find distance attenuation factor
				if(curlight->type == EXPLOSION){
					inSettings->lensFlares[inSettings->numFlares].r = curlight->rgb[0];
					inSettings->lensFlares[inSettings->numFlares].g = curlight->rgb[1];
					inSettings->lensFlares[inSettings->numFlares].b = curlight->rgb[2];
					float distatten = (10000.0f - vec.length()) * 0.0001f;
					if(distatten < 0.0f)
						distatten = 0.0f;
					inSettings->lensFlares[inSettings->numFlares].a = curlight->bright * shine * distatten;
				}
				else{
					inSettings->lensFlares[inSettings->numFlares].r = 1.0f;
					inSettings->lensFlares[inSettings->numFlares].g = 1.0f;
					inSettings->lensFlares[inSettings->numFlares].b = 1.0f;
					float distatten = (20000.0f - vec.length()) * 0.00005f;
					if(distatten < 0.0f)
						distatten = 0.0f;
					inSettings->lensFlares[inSettings->numFlares].a = curlight->bright * 2.0f * shine * distatten;
				}
				inSettings->numFlares++;
			}
		}
	}
}


void randomLookFrom(int n, SkyrocketSaverSettings *inSettings){
	inSettings->lookFrom[n][0] = rsRandf(6000.0f) - 3000.0f;
	inSettings->lookFrom[n][1] = rsRandf(1200.0f) + 5.0f;
	inSettings->lookFrom[n][2] = rsRandf(6000.0f) - 3000.0f;
}


void randomLookAt(int n, SkyrocketSaverSettings *inSettings){
	inSettings->lookAt[n][0] = rsRandf(1600.0f) - 800.0f;
	inSettings->lookAt[n][1] = rsRandf(800.0f) + 200.0f;
	inSettings->lookAt[n][2] = rsRandf(1600.0f) - 800.0f;
}


void findHeadingAndPitch(rsVec lookFrom, rsVec lookAt, float& heading, float& pitch){
	const float diffx(lookAt[0] - lookFrom[0]);
	const float diffy(lookAt[1] - lookFrom[1]);
	const float diffz(lookAt[2] - lookFrom[2]);
	const float radius(sqrtf(diffx * diffx + diffz * diffz));
	pitch = R2D * atan2f(diffy, radius);
	heading = R2D * atan2f(-diffx, -diffz);
}


__private_extern__ void draw(SkyrocketSaverSettings * inSettings){
	/*static float cameraAngle = 0.0f;
	static const float firstHeading = rsRandf(2.0f * PIx2);
	static const float firstRadius = rsRandf(2000.0f);*/
	static int lastCameraMode = inSettings->kCamera;
	static float cameraTime[3]  // time, elapsed time, step (1.0 - 0.0)
		= {20.0f, 0.0f, 0.0f};
	static float fov = 60.0f;
	static float zoom = 0.0f;  // For interpolating from regular camera view to zoomed in view
	static float zoomTime[2] = {300.0f, 0.0f};  // time until next zoom, duration of zoom
	static float heading, pitch;
	static float zoomHeading = 0.0f;
	static float zoomPitch = 0.0f;

	// Variables for printing text
	static float computeTime = 0.0f;
	static float drawTime = 0.0f;
	//static rsTimer computeTimer, drawTimer;
	// start compute time timer
	//computeTimer.tick();

	// super fast easter egg
	static int superFast = rsRandi(1000);
	if(!superFast)
		inSettings->frameTime *= 5.0f;

	// build viewing matrix
	/*glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0f, aspectRatio, 1.0f, 40000.0f);
	glGetDoublev(GL_PROJECTION_MATRIX, projMat);*/

	////////////////////////////////
	// update camera
	///////////////////////////////
	//static int first = 1;
	if(inSettings->first){
		randomLookFrom(1, inSettings);  // new target position
		// starting camera view is very far away
		inSettings->lookFrom[2] = rsVec(rsRandf(1000.0f) + 6000.0f, 5.0f, rsRandf(4000.0f) - 2000.0f);
		inSettings->textwriter = new rsText;
		inSettings->first = 0;
	}

	// Make new random camera view
	if(inSettings->kNewCamera){
		cameraTime[0] = rsRandf(25.0f) + 5.0f;
		cameraTime[1] = 0.0f;
		cameraTime[2] = 0.0f;
		// choose new positions
		randomLookFrom(1, inSettings);  // new target position
		randomLookAt(1, inSettings);  // new target position
		// cut to a new view
		randomLookFrom(2, inSettings);  // new last position
		randomLookAt(2, inSettings);  // new last position
		findHeadingAndPitch(inSettings->lookFrom[0], inSettings->lookAt[0], heading, pitch);	// add by NZ - update theading and pitch
		inSettings->kNewCamera = 0;
	}

	// Update the camera if it is active
	if(inSettings->kCamera == 1){
		if(lastCameraMode == 2){  // camera was controlled by mouse last frame
			cameraTime[0] = 10.0f;
			cameraTime[1] = 0.0f;
			cameraTime[2] = 0.0f;
			inSettings->lookFrom[2] = inSettings->lookFrom[0];
			randomLookFrom(1, inSettings);  // new target position
			inSettings->lookAt[2] = inSettings->lookAt[0];
			randomLookAt(1, inSettings);  // new target position
		}
		cameraTime[1] += inSettings->frameTime;
		cameraTime[2] = cameraTime[1] / cameraTime[0];
		if(cameraTime[2] >= 1.0f){  // reset camera sequence
			// reset timer
			cameraTime[0] = rsRandf(25.0f) + 5.0f;
			cameraTime[1] = 0.0f;
			cameraTime[2] = 0.0f;
			// choose new positions
			inSettings->lookFrom[2] = inSettings->lookFrom[1];  // last = target
			randomLookFrom(1, inSettings);  // new target position
			inSettings->lookAt[2] = inSettings->lookAt[1];  // last = target
			randomLookAt(1, inSettings);  // new target position
			if(!rsRandi(4) && zoom == 0.0f){  // possibly cut to new view if camera isn't zoomed in
				randomLookFrom(2, inSettings);  // new last position
				randomLookFrom(2, inSettings);  // new last position
			}
		}
		// change camera position and angle
		float cameraStep = 0.5f * (1.0f - cosf(cameraTime[2] * PI));
		inSettings->lookFrom[0] = inSettings->lookFrom[2] + ((inSettings->lookFrom[1] - inSettings->lookFrom[2]) * cameraStep);
		inSettings->lookAt[0] = inSettings->lookAt[2] + ((inSettings->lookAt[1] - inSettings->lookAt[2]) * cameraStep);
		// update variables used for sound and lens flares
		inSettings->cameraVel = inSettings->lookFrom[0] - inSettings->cameraPos;
		inSettings->cameraPos = inSettings->lookFrom[0];
		// find heading and pitch
		findHeadingAndPitch(inSettings->lookFrom[0], inSettings->lookAt[0], heading, pitch);

		// zoom in on rockets with camera
		zoomTime[0] -= inSettings->frameTime;
		if(zoomTime[0] < 0.0f){
			if(inSettings->zoomRocket == ZOOMROCKETINACTIVE){  // try to find a rocket to follow
				for(unsigned int i=0; i<inSettings->last_particle; ++i){
					if(inSettings->particles[i].type == ROCKET){
						inSettings->zoomRocket = i;
						if(inSettings->particles[inSettings->zoomRocket].tr > 4.0f){
							zoomTime[1] = inSettings->particles[inSettings->zoomRocket].tr;
							// get out of for loop if a suitable rocket has been found
							i = inSettings->last_particle;
						}
						else
							inSettings->zoomRocket = ZOOMROCKETINACTIVE;
					}
				}
				if(inSettings->zoomRocket == ZOOMROCKETINACTIVE)
					zoomTime[0] = 5.0f;
			}
			if(inSettings->zoomRocket != ZOOMROCKETINACTIVE){  // zoom in on this rocket
				zoom += inSettings->frameTime * 0.5f;
				if(zoom > 1.0f)
					zoom = 1.0f;
				zoomTime[1] -= inSettings->frameTime;
				float h, p;
				findHeadingAndPitch(inSettings->lookFrom[0], inSettings->particles[inSettings->zoomRocket].xyz, h, p);
				// Don't wrap around
				while(h - heading < -180.0f)
					h += 360.0f;
				while(h - heading > 180.0f)
					h -= 360.0f;
				while(zoomHeading - h < -180.0f)
					zoomHeading += 360.0f;
				while(zoomHeading - h > 180.0f)
					zoomHeading -= 360.0f;
				// Make zoomed heading and pitch follow rocket closely but not exactly.
				// It would look weird because the rockets wobble sometimes.
				zoomHeading += (h - zoomHeading) * 10.0f * inSettings->frameTime;
				zoomPitch += (p - zoomPitch) * 5.0f * inSettings->frameTime;
				// End zooming
				if(zoomTime[1] < 0.0f){
					inSettings->zoomRocket = ZOOMROCKETINACTIVE;
					// Zoom in again no later than 3 minutes from now
					zoomTime[0] = rsRandf(175.0f) + 5.0f;
				}
			}
		}
	}

	// Still counting down to zoom in on a rocket,
	// so keep zoomed out.
	if(zoomTime[0] > 0.0f){
		zoom -= inSettings->frameTime * 0.5f;
		if(zoom < 0.0f)
			zoom = 0.0f;
	}

	// Control camera with the mouse
	if(inSettings->kCamera == 2){
		// find heading and pitch to compute rotation component of modelview matrix
		heading += 100.0f * inSettings->frameTime * inSettings->aspectRatio * float(inSettings->centerx - inSettings->mousex) / float(inSettings->xsize);
		pitch += 100.0f * inSettings->frameTime * float(inSettings->centery - inSettings->mousey) / float(inSettings->ysize);
		if(heading > 180.0f)
			heading -= 360.0f;
		if(heading < -180.0f)
			heading += 360.0f;
		if(pitch > 90.0f)
			pitch = 90.0f;
		if(pitch < -90.0f)
			pitch = -90.0f;
		if(inSettings->mouseButtons & MK_LBUTTON)
			inSettings->mouseSpeed += 400.0f * inSettings->frameTime;
		if(inSettings->mouseButtons & MK_RBUTTON)
			inSettings->mouseSpeed -= 400.0f * inSettings->frameTime;
		if((inSettings->mouseButtons & MK_MBUTTON) || ((inSettings->mouseButtons & MK_LBUTTON) && (inSettings->mouseButtons & MK_RBUTTON)))
			inSettings->mouseSpeed = 0.0f;
		if(inSettings->mouseSpeed > 4000.0f)
			inSettings->mouseSpeed = 4000.0f;
		if(inSettings->mouseSpeed < -4000.0f)
			inSettings->mouseSpeed = -4000.0f;
		// find lookFrom location to compute translation component of modelview matrix
		float ch = cosf(D2R * heading);
		float sh = sinf(D2R * heading);
		float cp = cosf(D2R * pitch);
		float sp = sinf(D2R * pitch);
		inSettings->lookFrom[0][0] -= inSettings->mouseSpeed * sh * cp * inSettings->frameTime;
		inSettings->lookFrom[0][1] += inSettings->mouseSpeed * sp * inSettings->frameTime;
		inSettings->lookFrom[0][2] -= inSettings->mouseSpeed * ch * cp * inSettings->frameTime;
		inSettings->cameraPos = inSettings->lookFrom[0];
		// Calculate new lookAt position so that lens flares will be computed correctly
		// and so that transition back to autonomous camera mode is smooth
		inSettings->lookAt[0][0] = inSettings->lookFrom[0][0] - 500.0f * sh * cp;
		inSettings->lookAt[0][1] = inSettings->lookFrom[0][1] + 500.0f * sp;
		inSettings->lookAt[0][2] = inSettings->lookFrom[0][2] - 500.0f * ch * cp;
	}

	// Interpolate fov, heading, and pitch using zoom value
	// zoom of {0,1} maps to fov of {60,6}
	const float t(0.5f * (1.0f - cosf(RS_PI * zoom)));
	fov = 60.0f - 54.0f * t;
	heading = zoomHeading * t + heading * (1.0f - t);
	pitch = zoomPitch * t + pitch * (1.0f - t);

	// build viewing matrix
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(fov, inSettings->aspectRatio, 1.0f, 40000.0f);
	glGetDoublev(GL_PROJECTION_MATRIX, inSettings->projMat);

	// Build modelview matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glRotatef(-pitch, 1, 0, 0);
	glRotatef(-heading, 0, 1, 0);
	glTranslatef(-(inSettings->lookFrom[0][0]), -(inSettings->lookFrom[0][1]), -(inSettings->lookFrom[0][2]));
	// get modelview matrix for flares
	glGetDoublev(GL_MODELVIEW_MATRIX, inSettings->modelMat);

	// store this frame's camera mode for next frame
	lastCameraMode = inSettings->kCamera;
	// Update mouse idle time
	/*if(kCamera == 2){
		mouseIdleTime += frameTime;
		if(mouseIdleTime > 300.0f)  // return to autonomous camera mode after 5 minutes
			kCamera = 1;
	}*/

	// update billboard rotation matrix for particles
	glPushMatrix();
	glLoadIdentity();
	glRotatef(heading, 0, 1, 0);
	glRotatef(pitch, 1, 0, 0);
	glGetFloatv(GL_MODELVIEW_MATRIX, inSettings->billboardMat);
	glPopMatrix();

	// clear the screen
	glClear(GL_COLOR_BUFFER_BIT);

	// Slows fireworks, but not camera
	if(inSettings->kSlowMotion)
		inSettings->frameTime *= 0.5f;

	// Make more particles if necessary (always keep 1000 extra).
	// Ordinarily, you would resize as needed during the update loop, probably in the
	// addParticle() function.  But that logic doesn't work with this particle system
	// because particles can spawn other particles.  resizing the vector, and, thus, 
	// moving all particle addresses, doesn't work if you are in the middle of
	// updating a particle.
	const unsigned int size(inSettings->particles.size());
	if(inSettings->particles.size() - int(inSettings->last_particle) < 1000)
		inSettings->particles.resize(inSettings->particles.size() + 1000, inSettings);

	// Pause the animation?
	if(inSettings->kFireworks){
		// update world
		inSettings->theWorld->update(inSettings->frameTime, inSettings);
	
		// darken smoke
		static float ambientlight = float(inSettings->dAmbient) * 0.01f;
		for(unsigned int i=0; i<inSettings->last_particle; ++i){
			particle* darkener(&(inSettings->particles[i]));
			if(darkener->type == SMOKE)
				darkener->rgb[0] = darkener->rgb[1] = darkener->rgb[2] = ambientlight;
		}

		// Change rocket firing rate
		static float rocketTimer = 0.0f;
		static float rocketTimeConst = 10.0f / float(inSettings->dMaxrockets);
		static float changeRocketTimeConst = 20.0f;
		changeRocketTimeConst -= inSettings->frameTime;
		if(changeRocketTimeConst <= 0.0f){
			float temp = rsRandf(4.0f);
			rocketTimeConst = (temp * temp) + (10.0f / float(inSettings->dMaxrockets));
			changeRocketTimeConst = rsRandf(30.0f) + 10.0f;
		}
		// add new rocket to list
		rocketTimer -= inSettings->frameTime;
		if((rocketTimer <= 0.0f) || (inSettings->userDefinedExplosion >= 0)){
			if(inSettings->numRockets < inSettings->dMaxrockets){
				particle* rock = addParticle(inSettings);
				if(rsRandi(30) || (inSettings->userDefinedExplosion >= 0)){  // Usually launch a rocket
					rock->initRocket(inSettings);
					if(inSettings->userDefinedExplosion >= 0)
						rock->explosiontype = inSettings->userDefinedExplosion;
					else{
						if(!rsRandi(2500)){  // big ones!
							if(rsRandi(2))
								rock->explosiontype = 19;  // sucker and shockwave
							else
								rock->explosiontype = 20;  // stretcher and bigmama
						}
						else{
							// Distribution of regular explosions
							if(rsRandi(2)){  // 0 - 2 (all types of spheres)
								if(!rsRandi(10))
									rock->explosiontype = 2;
								else
									rock->explosiontype = rsRandi(2);
							}
							else{
								if(!rsRandi(3))  //  ring, double sphere, sphere and ring
									rock->explosiontype = rsRandi(3) + 3;
								else{
									if(rsRandi(2)){  // 6, 7, 8, 9, 10, 11
										if(rsRandi(2))
											rock->explosiontype = rsRandi(2) + 6;
										else
											rock->explosiontype = rsRandi(4) + 8;
									}
									else{
										if(rsRandi(2))  // 12, 13, 14
											rock->explosiontype = rsRandi(3) + 12;
										else  // 15 - 18
											rock->explosiontype = rsRandi(4) + 15;
									}
								}
							}
						}
					}
					inSettings->numRockets++;
				}
				else{  // sometimes make fountains instead of rockets
					rock->initFountain(inSettings);
					int num_fountains = rsRandi(3);
					for(int i=0; i<num_fountains; i++){
						rock = addParticle(inSettings);
						rock->initFountain(inSettings);
					}
				}
			}
			if(inSettings->dMaxrockets)
				rocketTimer = rsRandf(rocketTimeConst);
			else
				rocketTimer = 60.0f;  // arbitrary number since no rockets ever fire
			if(inSettings->userDefinedExplosion >= 0){
				inSettings->userDefinedExplosion = -1;
				rocketTimer = 20.0f;  // Wait 20 seconds after user launches a rocket before launching any more
			}
		}

		// update particles
		inSettings->numRockets = 0;
		for(unsigned int i=0; i<inSettings->last_particle; i++){
			particle* curpart(&(inSettings->particles[i]));
			inSettings->particles[i].update(inSettings);
			if(curpart->type == ROCKET)
				inSettings->numRockets++;
				curpart->findDepth(inSettings);
			if(curpart->life <= 0.0f || curpart->xyz[1] < 0.0f){
				switch(curpart->type){
				case ROCKET:
					if(curpart->xyz[1] <= 0.0f){
						// move above ground for explosion so new particles aren't removed
						curpart->xyz[1] = 0.1f;
						curpart->vel[1] *= -0.7f;
					}
					if(curpart->explosiontype == 18)
						curpart->initSpinner(inSettings);
					else
						curpart->initExplosion(inSettings);
					break;
				case POPPER:
					switch(curpart->explosiontype){
					case STAR:
						curpart->explosiontype = 100;
						curpart->initExplosion(inSettings);
						break;
					case STREAMER:
						curpart->explosiontype = 101;
						curpart->initExplosion(inSettings);
						break;
					case METEOR:
						curpart->explosiontype = 102;
						curpart->initExplosion(inSettings);
						break;
					case POPPER:
						curpart->type = STAR;
						curpart->rgb.set(1.0f, 0.8f, 0.6f);
						curpart->t = inSettings->particles[i].tr = inSettings->particles[i].life = 0.2f;
					}
					break;
				case SUCKER:
					curpart->initShockwave(inSettings);
					break;
				case STRETCHER:
					curpart->initBigmama(inSettings);
				}
			}
		}

		// remove particles from list
		for(unsigned int i=0; i<inSettings->last_particle; i++){
			particle* curpart(&(inSettings->particles[i]));
			if(curpart->life <= 0.0f || curpart->xyz[1] < 0.0f)
				removeParticle(i, inSettings);
		}

		sortParticles();
	}  // kFireworks

	else{
		// Only sort particles if they're not being updated (the camera could still be moving)
		for(unsigned int i=0; i<inSettings->last_particle; i++)
			inSettings->particles[i].findDepth(inSettings);
		sortParticles();
	}

	// measure compute time
	//computeTime += computeTimer.tick();
	// start draw time timer
	//drawTimer.tick();

	// the world
	inSettings->theWorld->draw(inSettings);

	// draw particles
	glEnable(GL_BLEND);
	for(unsigned int i=0; i<inSettings->last_particle; i++)
		inSettings->particles[i].draw(inSettings);

	// draw lens flares
	if(inSettings->dFlare){
		makeFlareList(inSettings);
		for(unsigned int i=0; i<inSettings->numFlares; ++i){
			flare(inSettings->lensFlares[i].x, inSettings->lensFlares[i].y, inSettings->lensFlares[i].r,
				inSettings->lensFlares[i].g, inSettings->lensFlares[i].b, inSettings->lensFlares[i].a, inSettings);
		}
		inSettings->numFlares = 0;
	}

	// measure draw time
	//drawTime += drawTimer.tick();

	// do sound stuff
	if(soundengine){
		float listenerOri[6];
		listenerOri[0] = float(-(inSettings->modelMat[2]));
		listenerOri[1] = float(-(inSettings->modelMat[6]));
		listenerOri[2] = float(-(inSettings->modelMat[10]));
		listenerOri[3] = float(inSettings->modelMat[1]);
		listenerOri[4] = float(inSettings->modelMat[5]);
		listenerOri[5] = float(inSettings->modelMat[9]);
		soundengine->update(inSettings->cameraPos.v, inSettings->cameraVel.v, listenerOri, inSettings->frameTime, inSettings->kSlowMotion);
	}

	//draw_overlay(frameTime);

	// print text
	static float totalTime = 0.0f;
	totalTime += inSettings->frameTime;
	static std::vector<std::string> strvec;
	static int frames = 0;
	++frames;
	if(frames == 20){
		strvec.clear();
		std::string str1 = "         FPS = " + to_string(20.0f / totalTime);
		strvec.push_back(str1);
		std::string str2 = "compute time = " + to_string(computeTime / 20.0f);
		strvec.push_back(str2);
		std::string str3 = "   draw time = " + to_string(drawTime / 20.0f);
		strvec.push_back(str3);
		totalTime = 0.0f;
		computeTime = 0.0f;
		drawTime = 0.0f;
		frames = 0;
	}

	if(inSettings->kStatistics){
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0.0f, 50.0f * inSettings->aspectRatio, 0.0f, 50.0f, -1.0f, 1.0f);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glTranslatef(1.0f, 48.0f, 0.0f);

		glColor3f(1.0f, 0.6f, 0.0f);
		inSettings->textwriter->draw(strvec);

		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
	}

	//wglSwapLayerBuffers(hdc, WGL_SWAP_MAIN_PLANE);
}


/*void IdleProc(){
	// update time
	static rsTimer timer;
	frameTime = timer.tick();

	if(readyToDraw && !isSuspended && !checkingPassword)
		draw();
    glFlush();
	//wglSwapLayerBuffers(hdc, WGL_SWAP_MAIN_PLANE);
}*/


void initSaver(int width, int height,SkyrocketSaverSettings * inSettings){
	//RECT rect;

	// Initialize pseudorandom number generator
	srand((unsigned)time(NULL));
	rand(); rand(); rand(); rand(); rand();
	rand(); rand(); rand(); rand(); rand();
	rand(); rand(); rand(); rand(); rand();
	rand(); rand(); rand(); rand(); rand();
	
	// NZ: Set up defaults in inSettings:
	inSettings->readyToDraw = 0;
	inSettings->lookAt[0] = rsVec(0.0f, 1000.0f, 0.0f);
	inSettings->lookAt[1] = rsVec(0.0f, 1000.0f, 0.0f);
	inSettings->lookAt[2] = rsVec(0.0f, 1000.0f, 0.0f);
	//inSettings->soundengine = NULL;
	inSettings->numRockets = 0;
	inSettings->numFlares = 0;
	inSettings->kFireworks = 1;
	inSettings->kNewCamera = 0;
	inSettings->userDefinedExplosion = -1;
	inSettings->zoomRocket = ZOOMROCKETINACTIVE;
	inSettings->first = 1;

	// Window initialization
	//hdc = GetDC(hwnd);
	//SetBestPixelFormat(hdc);
	//hglrc = wglCreateContext(hdc);
	//GetClientRect(hwnd, &rect);
	//wglMakeCurrent(hdc, hglrc);
	//xsize = rect.right - rect.left;
	//ysize = rect.bottom - rect.top;
	inSettings->xsize = width;
	inSettings->ysize = height;
	inSettings->centerx = inSettings->xsize / 2;
	inSettings->centery = inSettings->ysize / 2;
	glViewport(0, 0, inSettings->xsize, inSettings->ysize);
	glGetIntegerv(GL_VIEWPORT, inSettings->viewport);
	inSettings->aspectRatio = float(width) / float(height);

	//glMatrixMode(GL_MODELVIEW_MATRIX);

	// Set OpenGL state
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glFrontFace(GL_CCW);
	glEnable(GL_CULL_FACE);

	// Initialize data structures
	initFlares(inSettings);
	//initRockets();
	if(inSettings->dSmoke)
		initSmoke(inSettings);
	inSettings->theWorld = new World(inSettings);
	initShockwave();
	if(inSettings->dSound && soundengine == NULL)
		soundengine = new SoundEngine(float(inSettings->dSound) * 0.01f);

	//init_overlay();

	//outfile.open("outfile");
	
	// Addition by NZ: If we're using a static camera, then randomize the camera.
	if (inSettings->kCamera == 0)
	{
		inSettings->kNewCamera = 1;
	}
}

__private_extern__ void setDefaults(SkyrocketSaverSettings * inSettings)
{
	inSettings->dMaxrockets = 8;
	inSettings->dSmoke = 5;
	inSettings->dExplosionsmoke = 0;
	inSettings->dWind = 20;
	inSettings->dAmbient = 10;
	inSettings->dStardensity = 20;
	inSettings->dFlare = 20;
	inSettings->dMoonglow = 20;
	inSettings->dSound = 0;
        inSettings->kCamera = 1;
	inSettings->dMoon = 1;
	inSettings->dClouds = 1;
	inSettings->dEarth = 1;
	inSettings->dIllumination = 1;
	inSettings->kSlowMotion = false;
}

__private_extern__ void cleanup(SkyrocketSaverSettings * inSettings)
{
	// Free memory
	inSettings->particles.clear();
	
	// clean up sound data structures
	if(inSettings->dSound)
	{
		delete soundengine;
		soundengine = NULL;
	}
	
	// Kill device context
	/*ReleaseDC(hwnd, hdc);
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hglrc);*/
}


//LONG ScreenSaverProc(HWND hwnd, UINT msg, WPARAM wpm, LPARAM lpm){
long ScreenSaverProc(unsigned int msg, unsigned int wpm, unsigned long lpm, SkyrocketSaverSettings *inSettings)
{
	switch(msg){
		/*case WM_CREATE:
			readRegistry();
			initSaver(hwnd);
			readyToDraw = 1;
			break;*/
		case WM_KEYDOWN:
			switch(int(wpm)){
				// pause the motion of the fireworks
				case 'f':
				case 'F':
					if(inSettings->kFireworks)
						inSettings->kFireworks = 0;
					else
						inSettings->kFireworks = 1;
					if(inSettings->kSlowMotion)
						inSettings->kSlowMotion = false;
						return(0);
					// pause the motion of the camera
				case 'c':
				case 'C':
					if(inSettings->kCamera == 0)
						inSettings->kCamera = 1;
					else{
						if(inSettings->kCamera == 1)
							inSettings->kCamera = 0;
						else{
							if(inSettings->kCamera == 2)
								inSettings->kCamera = 1;
						}
					}
						return(0);
					// toggle mouse camera control
				case 'm':
				case 'M':
					if(inSettings->kCamera == 2)
						inSettings->kCamera = 1;
					else{
						inSettings->kCamera = 2;
						inSettings->mouseSpeed = 0.0f;
						inSettings->mouseIdleTime = 0.0f;
					}
						return(0);
					// new camera view
				case 'n':
				case 'N':
					inSettings->kNewCamera = 1;
					return(0);
					// slow the motion of the fireworks
				case 'g':
				case 'G':
					if(inSettings->kSlowMotion)
						inSettings->kSlowMotion = false;
					else
						inSettings->kSlowMotion = true;
					if(!inSettings->kFireworks)
						inSettings->kFireworks = 1;
						return(0);
					// choose which type of rocket explosion
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					inSettings->userDefinedExplosion = int(wpm) - 49;  // explosions 0 - 8
					return(0);
				case '0':
					inSettings->userDefinedExplosion = 9;
					return(0);
				case 'q':
				case 'Q':
					inSettings->userDefinedExplosion = 10;
					return(0);
				case 'w':
				case 'W':
					inSettings->userDefinedExplosion = 11;
					return(0);
				case 'e':
				case 'E':
					inSettings->userDefinedExplosion = 12;
					return(0);
				case 'r':
				case 'R':
					inSettings->userDefinedExplosion = 13;
					return(0);
				case 't':
				case 'T':
					inSettings->userDefinedExplosion = 14;
					return(0);
				case 'y':
				case 'Y':
					inSettings->userDefinedExplosion = 15;
					return(0);
				case 'u':
				case 'U':
					inSettings->userDefinedExplosion = 16;
					return(0);
				case 'i':
				case 'I':
					inSettings->userDefinedExplosion = 17;
					return(0);
				case 'o':
				case 'O':
					inSettings->userDefinedExplosion = 18;
					return(0);
				// NZ: Added these two myself:
				case '[':
					inSettings->userDefinedExplosion = 19;
					return(0);
				case ']':
					inSettings->userDefinedExplosion = 20;
					return(0);
					// The rest of the letters do nothing, so overriding them--and
					// therefore disabling them--makes it harder to make mistakes
				case 'a':
				case 'A':
				case 'b':
				case 'B':
				case 'd':
				case 'D':
				case 'h':
				case 'H':
				case 'j':
				case 'J':
				case 'k':
				case 'K':
				case 'l':
				case 'L':
				case 'p':
				case 'P':
					//case 's':  These are used by rsWin32Saver
					//case 'S':  to toggle kStatistics
				case 'v':
				case 'V':
				case 'x':
				case 'X':
				case 'z':
				case 'Z':
					return(0);
			}
			break;
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			if(inSettings->kCamera == 2){
				inSettings->mouseButtons = wpm;        // key flags 
				inSettings->mousex = LOWORD(lpm);  // horizontal position of cursor 
				inSettings->mousey = HIWORD(lpm);
				inSettings->mouseIdleTime = 0.0f;
				return(0);
			}
			break;
		/*case WM_DESTROY:
			readyToDraw = 0;
			cleanup(hwnd);
			break;*/
	}
	//return DefScreenSaverProc(hwnd, msg, wpm, lpm);
	return 1;
}
