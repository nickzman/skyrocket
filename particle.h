/*
 * Copyright (C) 1999-2010  Terence M. Welsh
 *
 * This file is part of Skyrocket.
 *
 * Skyrocket is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
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


#ifndef PARTICLE_H
#define PARTICLE_H



/*#include <rsMath/rsMath.h>
#include <Skyrocket/flare.h>
#include <Skyrocket/smoke.h>
//#include "rocket.h"
//#include "world.h"
#include <Skyrocket/shockwave.h>
#include <Skyrocket/SoundEngine.h>*/
#include "rsMath.h"
#include "flare.h"
#include "smoke.h"
#include "shockwave.h"
#include "SoundEngine.h"

struct SkyrocketSaverSettings;


#define PI 3.14159265359f
#define PIx2 6.28318530718f
#define D2R 0.0174532925f
#define R2D 57.2957795131f
// types of particles
#define ROCKET 0
#define FOUNTAIN 1
#define SPINNER 2
#define SMOKE 3
#define EXPLOSION 4
#define STAR 5
#define STREAMER 6
#define METEOR 7
#define POPPER 8
#define BEE 9
#define SUCKER 10
#define SHOCKWAVE 11
#define STRETCHER 12
#define BIGMAMA 13


class particle;


extern particle* addParticle(SkyrocketSaverSettings *inSettings);

extern void illuminate(particle* ill, SkyrocketSaverSettings *inSettings);
extern void pulling(particle* suck, SkyrocketSaverSettings *inSettings);
extern void pushing(particle* shock, SkyrocketSaverSettings *inSettings);
extern void stretching(particle* stretch, SkyrocketSaverSettings *inSettings);


/*extern int dSmoke;
extern int dExplosionsmoke;
extern int dWind;
extern int dClouds;
extern int dIllumination;
extern int dSound;

extern float frameTime;
extern rsVec cameraPos;  // used for positioning sounds
extern float billboardMat[16];*/

extern SoundEngine* soundengine;


class particle{
public:
	unsigned int type; // choose type from #defines listed above
	unsigned int displayList; // which object to draw (uses flare and rocket models)
	rsVec xyz; // current position
	rsVec lastxyz; // position from previous frame
	rsVec vel; // velocity vector
	rsVec rgb; // particle's color
	float drag; // constant to represent air resistance
	float t; // total time that particle lives
	float tr; // time remaining
	float bright; // intensity at which particle shines
	float life; // life remaining (usually defined from 0.0 to 1.0)
	float size; // scale factor by which to multiply the display list
	// rocket variables
	float thrust; // constant to represent power of rocket
	float endthrust; // point in rockets life at which to stop thrusting
	float spin, tilt; // radial and pitch velocities to make rockets wobble when they go up
	rsVec tiltvec; // vector about which a rocket tilts
	int makeSmoke; // whether or not this particle produces smoke
	int smokeTimeIndex; // which smoke time to use
	float smokeTrailLength; // length that smoke particles must cover from one frame to the next.
		// smokeTrailLength is stored so that remaining length from previous frame can be covered
		// and no gaps are left in the smoke trail
	float sparkTrailLength; // same for sparks from streamers
	int explosiontype; // Type of explosion that a rocket will become when life runs out
	// sorting variable
	float depth;

	// Constructor initializes particles to be stars because that's what most of them are
	particle(SkyrocketSaverSettings *inSettings);
	~particle(){};
	// A handy function for choosing an explosion's color
	void randomColor(rsVec& color);
	// Initialization functions for particle types other than stars
	void initStar(SkyrocketSaverSettings *inSettings);
	void initStreamer(SkyrocketSaverSettings *inSettings);
	void initMeteor(SkyrocketSaverSettings *inSettings);
	void initStarPopper(SkyrocketSaverSettings *inSettings);
	void initStreamerPopper(SkyrocketSaverSettings *inSettings);
	void initMeteorPopper(SkyrocketSaverSettings *inSettings);
	void initLittlePopper(SkyrocketSaverSettings *inSettings);
	void initBee(SkyrocketSaverSettings *inSettings);
	void initRocket(SkyrocketSaverSettings *inSettings);
	void initFountain(SkyrocketSaverSettings *inSettings);
	void initSpinner(SkyrocketSaverSettings *inSettings);
	void initSmoke(rsVec pos, rsVec speed, SkyrocketSaverSettings *inSettings);
	void initSucker(SkyrocketSaverSettings *inSettings); // rare easter egg explosion which is immediately followed by...
	void initShockwave(SkyrocketSaverSettings *inSettings); // a freakin' huge explosion
	void initStretcher(SkyrocketSaverSettings *inSettings); // another rare explosion followed by...
	void initBigmama(SkyrocketSaverSettings *inSettings); // this other massive bomb
	void initExplosion(SkyrocketSaverSettings *inSettings);
	// "pop" functions are used to spawn new particles during explosions
	void popSphere(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings);
	void popSplitSphere(int numParts, float v0, rsVec color1, SkyrocketSaverSettings *inSettings);
	void popMultiColorSphere(int numParts, float v0, SkyrocketSaverSettings *inSettings);
	void popRing(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings);
	void popStreamers(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings);
	void popMeteors(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings);
	void popStarPoppers(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings);
	void popStreamerPoppers(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings);
	void popMeteorPoppers(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings);
	void popLittlePoppers(int numParts, float v0, SkyrocketSaverSettings *inSettings);
	void popBees(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings);
	// Finds depth along camera's coordinate system's -z axis.
	// Can be used for sorting and culling.
	void findDepth(SkyrocketSaverSettings *inSettings);
	// Update a particle according to frameTime
	void update(SkyrocketSaverSettings *inSettings);
	// Draw a particle
	void draw(SkyrocketSaverSettings *inSettings);
	// Return a pointer to this particle
	particle* thisParticle(){return this;};

	// operators used by stl list sorting
	friend bool operator < (const particle &p1, const particle &p2){return(p2.depth < p1.depth);}
	friend bool operator > (const particle &p1, const particle &p2){return(p2.depth > p1.depth);}
	friend bool operator == (const particle &p1, const particle &p2){return(p1.depth == p2.depth);}
	friend bool operator != (const particle &p1, const particle &p2){return(p1.depth != p2.depth);}
};



#endif
