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


/*#include <Skyrocket/particle.h>
#include <Skyrocket/world.h>*/
#include "particle.h"
#include "world.h"



particle::particle(SkyrocketSaverSettings *inSettings){
	type = STAR;
	displayList = inSettings->flarelist[0];
	drag = 0.612f;  // terminal velocity of 20 ft/s
	t = 2.0f;
	tr = t;
	bright = 1.0f;
	life = bright;
	size = 30.0f;
	makeSmoke = 0;

	smokeTimeIndex = 0;
	smokeTrailLength = 0.0f;
	sparkTrailLength = 0.0f;
	depth = 0.0f;
}

void particle::randomColor(rsVec& color){
	int i, j, k;
	//rsVec color;

	switch(rsRandi(6)){
	case 0:
		i=0; j=1; k=2;
		break;
	case 1:
		i=0; j=2; k=1;
		break;
	case 2:
		i=1; j=0; k=2;
		break;
	case 3:
		i=1; j=2; k=0;
		break;
	case 4:
		i=2; j=0; k=1;
		break;
	case 5:
		i=2; j=1; k=0;
	}

	color[i] = 1.0f;
	color[j] = rsRandf(1.0f);
	color[k] = rsRandf(0.2f);

	//return(color);
}

void particle::initRocket(SkyrocketSaverSettings *inSettings){
	// Thrust, initial velocity, and t (time) should send
	// rockets to about 800 to 1200 feet before exploding.

	type = ROCKET;
	//displayList = rocketlist;
	xyz[0] = rsRandf(200.0f) - 100.0f;
	xyz[1] = 5.0f;
	xyz[2] = rsRandf(200.0f) - 100.0f;
	lastxyz[0] = xyz[0];
	lastxyz[1] = 4.0f;
	lastxyz[2] = xyz[2];
	vel.set(0.0f, 100.0f, 0.0f);
	rgb.set(rsRandf(0.7f) + 0.3f, rsRandf(0.7f) + 0.3f, 0.3f);
	size = 1.0f;
	drag = 0.281f;  // terminal velocity of 50 ft/s
	t = tr = rsRandf(2.0f) + 5.0f;
	life = 1.0f;
	bright = 0.0f;
	thrust = 185.0f;
	endthrust = rsRandf(0.1f) + 0.3f;
	spin = rsRandf(40.0f) - 20.0f;
	tilt = rsRandf(30.0f * float(fabs(spin)));
	tiltvec.set(cosf(spin), 0.0f, sinf(spin));
	if(!rsRandi(200)){  // crash the occasional rocket
		spin = 0.0f;
		tilt = rsRandf(100.0f) + 75.0f;
		float temp = rsRandf(PIx2);
		tiltvec.set(cosf(temp), 0.0f, sinf(temp));
	}
	makeSmoke = 1;
	smokeTrailLength = 0.0f;
	sparkTrailLength = 0.0f;
	explosiontype = 0;

	if(soundengine){
		if(rsRandi(2))
			soundengine->insertSoundNode(LAUNCH1SOUND, xyz, inSettings->cameraPos);
		else
			soundengine->insertSoundNode(LAUNCH2SOUND, xyz, inSettings->cameraPos);
	}
}

void particle::initFountain(SkyrocketSaverSettings *inSettings){
	type = FOUNTAIN;
	displayList = inSettings->flarelist[0];
	size = 30.0f;
	// position can be defined here because these are always on the ground
	xyz[0] = rsRandf(300.0f) - 150.0f;
	xyz[1] = 5.0f;
	xyz[2] = rsRandf(300.0f) - 150.0f;
	randomColor(rgb);
	t = tr = rsRandf(5.0f) + 10.0f;
	bright = 0.0f;
	life = 1.0f;
	makeSmoke = 0;

	if(soundengine){
		if(rsRandi(2))
			soundengine->insertSoundNode(LAUNCH1SOUND, xyz, inSettings->cameraPos);
		else
			soundengine->insertSoundNode(LAUNCH2SOUND, xyz, inSettings->cameraPos);
	}
}

void particle::initSpinner(SkyrocketSaverSettings *inSettings){
	type = SPINNER;
	displayList = inSettings->flarelist[0];
	drag = 0.612f;  // terminal velocity of 20 ft/s
	randomColor(rgb);
	spin = rsRandf(3.0f) + 12.0f;  // radial velocity
	tilt = rsRandf(PIx2);  // original rotation
	tiltvec.set(rsRandf(2.0f) - 1.0f, rsRandf(2.0f) - 1.0f, rsRandf(2.0f) - 1.0f);
	tiltvec.normalize();  // vector around which this spinner spins
	t = tr = rsRandf(2.0f) + 6.0f;
	bright = 0.0f;
	life = 1.0f;
	size = 20.0f;
	makeSmoke = 1;
	sparkTrailLength = 0.0f;

	if(soundengine){
		if(rsRandi(2))
			soundengine->insertSoundNode(LAUNCH1SOUND, xyz, inSettings->cameraPos);
		else
			soundengine->insertSoundNode(LAUNCH2SOUND, xyz, inSettings->cameraPos);
	}
}

void particle::initSmoke(rsVec pos, rsVec speed, SkyrocketSaverSettings *inSettings){
	type = SMOKE;
	displayList = inSettings->smokelist[rsRandi(5)];
	xyz = pos;
	vel = speed;
	rgb[0] = rgb[1] = rgb[2] = 0.01f * float(inSettings->dAmbient);
	drag = 2.0f;
	// time for each smoke particle varies and must be assigned by the particle that produces the smoke
	size = 0.1f;
	makeSmoke = 0;
}

void particle::initStar(SkyrocketSaverSettings *inSettings){
	type = STAR;
	displayList = inSettings->flarelist[0];
	drag = 0.612f;  // terminal velocity of 20 ft/s
	size = 30.0f;
	t = tr = rsRandf(1.0f) + 2.0f;
	life = 1.0f;
	static int someSmoke = 0;
	makeSmoke = inSettings->whichSmoke[someSmoke];
	smokeTrailLength = 0.0f;
	++someSmoke;
	if(someSmoke >= WHICHSMOKES)
		someSmoke = 0;
}

void particle::initStreamer(SkyrocketSaverSettings *inSettings){
	type = STREAMER;
	displayList = inSettings->flarelist[0];
	drag = 0.612f;  // terminal velocity of 20 ft/s
	size = 30.0f;
	t = tr = rsRandf(1.0f) + 3.0f;
	life = 1.0f;
	sparkTrailLength = 0.0f;
}

void particle::initMeteor(SkyrocketSaverSettings *inSettings){
	type = METEOR;
	displayList = inSettings->flarelist[0];
	drag = 0.612f;  // terminal velocity of 20 ft/s
	t = tr = rsRandf(1.0f) + 3.0f;
	life = 1.0f;
	size = 20.0f;
	sparkTrailLength = 0.0f;
}

void particle::initStarPopper(SkyrocketSaverSettings *inSettings){
	type = POPPER;
	displayList = inSettings->flarelist[0];
	drag = 0.4f;
	t = tr = rsRandf(1.5f) + 3.0f;
	life = 1.0f;
	makeSmoke = 1;
	explosiontype = STAR;
	size = 0.0f;
	smokeTrailLength = 0.0f;
}

void particle::initStreamerPopper(SkyrocketSaverSettings *inSettings){
	type = POPPER;
	displayList = inSettings->flarelist[0];
	size = 0.0f;
	drag = 0.4f;
	t = tr = rsRandf(1.5f) + 3.0f;
	life = 1.0f;
	makeSmoke = 1;
	explosiontype = STREAMER;
	smokeTrailLength = 0.0f;
}

void particle::initMeteorPopper(SkyrocketSaverSettings *inSettings){
	type = POPPER;
	displayList = inSettings->flarelist[0];
	size = 0.0f;
	drag = 0.4f;
	t = tr = rsRandf(1.5f) + 3.0f;
	life = 1.0f;
	makeSmoke = 1;
	explosiontype = METEOR;
	smokeTrailLength = 0.0f;
}

void particle::initLittlePopper(SkyrocketSaverSettings *inSettings){
	type = POPPER;
	displayList = inSettings->flarelist[0];
	drag = 0.4f;
	t = tr = 4.0f * (0.5f - sinf(rsRandf(PI))) + 4.5f;
	life = 1.0f;
	size = rsRandf(3.0f) + 7.0f;
	makeSmoke = 0;
	explosiontype = POPPER;
}

void particle::initBee(SkyrocketSaverSettings *inSettings){
	type = BEE;
	displayList = inSettings->flarelist[0];
	size = 10.0f;
	drag = 0.5f;
	t = tr = rsRandf(2.5f) + 2.5f;
	life = 1.0f;
	makeSmoke = 0;
	sparkTrailLength = 0.0f;

	// these variables will be misused to describe bee acceleration vector
	thrust = rsRandf(PIx2) + PI;
	endthrust = rsRandf(PIx2) + PI;
	spin = rsRandf(PIx2) + PI;
	tiltvec.set(rsRandf(PIx2), rsRandf(PIx2), rsRandf(PIx2));
}

void particle::initSucker(SkyrocketSaverSettings *inSettings){
	int i;
	particle* newp;
	rsVec color;
	float temp1, temp2, ch, sh, cp, sp;

	type = SUCKER;
	drag = 0.612f;  // terminal velocity of 20 ft/s
	displayList = inSettings->flarelist[2];
	rgb.set(1.0f, 1.0f, 1.0f);
	size = 300.0f;
	t = tr = 4.0f;
	life = 1.0f;
	makeSmoke = 0;

	// make explosion
	newp = addParticle(inSettings);
	newp->type = EXPLOSION;
	newp->xyz = xyz;
	newp->vel = vel;
	newp->rgb.set(1.0f, 1.0f, 1.0f);
	newp->size = 200.0f;
	newp->t = newp->tr = 4.0f;

	// Make double ring to go along with sucker
	randomColor(color);
	temp1 = rsRandf(PI);  // heading
	temp2 = rsRandf(PI);  // pitch
	ch = cosf(temp1);
	sh = sinf(temp1);
	cp = cosf(temp2);
	sp = sinf(temp2);
	for(i=0; i<90; i++){
		newp = addParticle(inSettings);
		newp->initStar(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = 0.0f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		// pitch
		newp->vel[1] = sp * newp->vel[2];
		newp->vel[2] = cp * newp->vel[2];
		// heading
		temp1 = newp->vel[0];
		newp->vel[0] = ch * temp1 + sh * newp->vel[1];
		newp->vel[1] = -sh * temp1 + ch * newp->vel[1];
		// multiply velocity
		newp->vel[0] *= 350.0f + rsRandf(30.0f);
		newp->vel[1] *= 350.0f + rsRandf(30.0f);
		newp->vel[2] *= 350.0f + rsRandf(30.0f);
		newp->vel[0] += vel[0];
		newp->vel[1] += vel[1];
		newp->vel[2] += vel[2];
		newp->rgb = color;
		newp->t = newp->tr = rsRandf(2.0f) + 2.0f;
		newp->makeSmoke = 0;
	}
	randomColor(color);
	temp1 = rsRandf(PI);  // heading
	temp2 = rsRandf(PI);  // pitch
	ch = cosf(temp1);
	sh = sinf(temp1);
	cp = cosf(temp2);
	sp = sinf(temp2);
	for(i=0; i<90; i++){
		newp = addParticle(inSettings);
		newp->initStar(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = 0.0f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		// pitch
		newp->vel[1] = sp * newp->vel[2];
		newp->vel[2] = cp * newp->vel[2];
		// heading
		temp1 = newp->vel[0];
		newp->vel[0] = ch * temp1 + sh * newp->vel[1];
		newp->vel[1] = -sh * temp1 + ch * newp->vel[1];
		// multiply velocity
		newp->vel[0] *= 600.0f + rsRandf(50.0f);
		newp->vel[1] *= 600.0f + rsRandf(50.0f);
		newp->vel[2] *= 600.0f + rsRandf(50.0f);
		newp->vel[0] += vel[0];
		newp->vel[1] += vel[1];
		newp->vel[2] += vel[2];
		newp->rgb = color;
		newp->t = newp->tr = rsRandf(2.0f) + 2.0f;
		newp->makeSmoke = 0;
	}

	if(soundengine)
		soundengine->insertSoundNode(SUCKSOUND, xyz, inSettings->cameraPos);
}

void particle::initShockwave(SkyrocketSaverSettings *inSettings){
	int i;
	particle* newp;
	rsVec color;

	type = SHOCKWAVE;
	drag = 0.612f;  // terminal velocity of 20 ft/s
	rgb.set(1.0f, 1.0f, 1.0f);
	size = 0.01f;
	t = tr = 8.0f;
	life = 1.0f;
	bright = life;

	// make explosion
	newp = addParticle(inSettings);
	newp->type = EXPLOSION;
	newp->xyz = xyz;
	newp->vel = vel;
	newp->rgb.set(1.0f, 1.0f, 1.0f);
	newp->size = 300.0f;
	newp->t = newp->tr = 2.0f;
	life = 1.0f;
	makeSmoke = 0;

	// Little sphere without smoke
	randomColor(color);
	for(i=0; i<75; i++){
		newp = addParticle(inSettings);
		newp->initStar(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = rsRandf(1.0f) - 0.5f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		newp->vel *= (rsRandf(10.0f) + 100.0f);
		newp->vel += vel;
		newp->rgb = color;
		newp->size = 100.0f;
		newp->t = newp->tr = rsRandf(2.0f) + 2.0f;
		newp->makeSmoke = 0;
	}

	// Disk of stars without smoke
	randomColor(color);
	for(i=0; i<150; i++){
		newp = addParticle(inSettings);
		newp->initStar(inSettings);
		newp->drag = 0.2f;
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = rsRandf(0.03f) - 0.005f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		// multiply velocity
		newp->vel *= (rsRandf(30.0f) + 500.0f);
		newp->vel += vel;
		newp->rgb = color;
		newp->size = 50.0f;
		newp->t = newp->tr = rsRandf(2.0f) + 3.0f;
		newp->makeSmoke = 0;
	}

	if(soundengine)
		soundengine->insertSoundNode(NUKESOUND, xyz, inSettings->cameraPos);
}

void particle::initStretcher(SkyrocketSaverSettings *inSettings){
	int i;
	particle* newp;
	rsVec color;

	type = STRETCHER;
	drag = 0.612f;  // terminal velocity of 20 ft/s
	displayList = inSettings->flarelist[3];
	rgb.set(1.0f, 1.0f, 1.0f);
	size = 0.0f;
	t = tr = 4.0f;
	life = 1.0f;
	makeSmoke = 0;

	// explosion
	newp = addParticle(inSettings);
	newp->type = EXPLOSION;
	newp->displayList = inSettings->flarelist[0];
	newp->xyz = xyz;
	newp->vel = vel;
	newp->rgb.set(1.0f, 0.8f, 0.6f);
	newp->size = 400.0f;
	newp->t = newp->tr = 4.0f;
	life = 1.0f;
	newp->makeSmoke = 0;

	// Make triple ring to go along with stretcher
	randomColor(color);
	for(i=0; i<80; i++){
		newp = addParticle(inSettings);
		newp->initStar(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = 0.0f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		newp->vel[0] *= 400.0f + rsRandf(30.0f);
		newp->vel[1] += rsRandf(70.0f) - 35.0f;
		newp->vel[2] *= 400.0f + rsRandf(30.0f);
		newp->vel[0] += vel[0];
		newp->vel[1] += vel[1];
		newp->vel[2] += vel[2];
		newp->rgb = color;
		newp->t = newp->tr = rsRandf(2.0f) + 2.0f;
		newp->makeSmoke = 0;
	}
	randomColor(color);
	for(i=0; i<80; i++){
		newp = addParticle(inSettings);
		newp->initStar(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = 0.0f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		newp->vel[0] *= 550.0f + rsRandf(40.0f);
		newp->vel[1] += rsRandf(70.0f) - 35.0f;
		newp->vel[2] *= 550.0f + rsRandf(40.0f);
		newp->vel[0] += vel[0];
		newp->vel[1] += vel[1];
		newp->vel[2] += vel[2];
		newp->rgb = color;
		newp->t = newp->tr = rsRandf(2.0f) + 2.0f;
		newp->makeSmoke = 0;
	}
	randomColor(color);
	for(i=0; i<80; i++){
		newp = addParticle(inSettings);
		newp->initStar(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = 0.0f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		newp->vel[0] *= 700.0f + rsRandf(50.0f);
		newp->vel[1] += rsRandf(70.0f) - 35.0f;
		newp->vel[2] *= 700.0f + rsRandf(50.0f);
		newp->vel[0] += vel[0];
		newp->vel[1] += vel[1];
		newp->vel[2] += vel[2];
		newp->rgb = color;
		newp->t = newp->tr = rsRandf(2.0f) + 2.0f;
		newp->makeSmoke = 0;
	}

	if(soundengine)
		soundengine->insertSoundNode(SUCKSOUND, xyz, inSettings->cameraPos);
}

void particle::initBigmama(SkyrocketSaverSettings *inSettings){
	int i;
	particle* newp;
	rsVec color;
	float temp;

	type = BIGMAMA;
	drag = 0.612f;  // terminal velocity of 20 ft/s
	displayList = inSettings->flarelist[2];
	rgb.set(0.6f, 0.6f, 1.0f);
	size = 0.0f;
	t = tr = 5.0f;
	life = 1.0f;
	bright = life;
	makeSmoke = 0;

	// explosion
	newp = addParticle(inSettings);
	newp->type = EXPLOSION;
	newp->xyz = xyz;
	newp->vel = vel;
	newp->drag = 0.0f;
	newp->rgb.set(0.8f, 0.8f, 1.0f);
	newp->size = 200.0f;
	newp->t = newp->tr = 2.5f;
	life = 1.0f;
	newp->makeSmoke = 0;

	// vertical stars
	newp = addParticle(inSettings);
	newp->initStar(inSettings);
	newp->xyz = xyz;
	newp->vel = vel;
	newp->drag = 0.0f;
	newp->vel[1] += 15.0f;
	newp->rgb.set(1.0f, 1.0f, 0.9f);
	newp->size = 400.0f;
	newp->t = newp->tr = 3.0f;
	newp->makeSmoke = 0;
	newp = addParticle(inSettings);
	newp->initStar(inSettings);
	newp->xyz = xyz;
	newp->vel = vel;
	newp->drag = 0.0f;
	newp->vel[1] -= 15.0f;
	newp->rgb.set(1.0f, 1.0f, 0.9f);
	newp->size = 400.0f;
	newp->t = newp->tr = 3.0f;
	newp->makeSmoke = 0;
	newp = addParticle(inSettings);
	newp->initStar(inSettings);
	newp->xyz = xyz;
	newp->vel = vel;
	newp->drag = 0.0f;
	newp->vel[1] += 45.0f;
	newp->rgb.set(1.0f, 1.0f, 0.6f);
	newp->size = 400.0f;
	newp->t = newp->tr = 3.5f;
	newp->makeSmoke = 0;
	newp = addParticle(inSettings);
	newp->initStar(inSettings);
	newp->xyz = xyz;
	newp->vel = vel;
	newp->drag = 0.0f;
	newp->vel[1] -= 45.0f;
	newp->rgb.set(1.0f, 1.0f, 0.6f);
	newp->size = 400.0f;
	newp->t = newp->tr = 3.5f;
	newp->makeSmoke = 0;
	newp = addParticle(inSettings);
	newp->initStar(inSettings);
	newp->xyz = xyz;
	newp->vel = vel;
	newp->drag = 0.0f;
	newp->vel[1] += 75.0f;
	newp->rgb.set(1.0f, 0.5f, 0.3f);
	newp->size = 400.0f;
	newp->t = newp->tr = 4.0f;
	newp->makeSmoke = 0;
	newp = addParticle(inSettings);
	newp->initStar(inSettings);
	newp->xyz = xyz;
	newp->vel = vel;
	newp->drag = 0.0f;
	newp->vel[1] -= 75.0f;
	newp->rgb.set(1.0f, 0.5f, 0.3f);
	newp->size = 400.0f;
	newp->t = newp->tr = 4.0f;
	newp->makeSmoke = 0;
	newp = addParticle(inSettings);
	newp->initStar(inSettings);
	newp->xyz = xyz;
	newp->vel = vel;
	newp->drag = 0.0f;
	newp->vel[1] += 105.0f;
	newp->rgb.set(1.0f, 0.0f, 0.0f);
	newp->size = 400.0f;
	newp->t = newp->tr = 4.5f;
	newp->makeSmoke = 0;
	newp = addParticle(inSettings);
	newp->initStar(inSettings);
	newp->xyz = xyz;
	newp->vel = vel;
	newp->drag = 0.0f;
	newp->vel[1] -= 105.0f;
	newp->rgb.set(1.0f, 0.0f, 0.0f);
	newp->size = 400.0f;
	newp->t = newp->tr = 4.5f;
	newp->makeSmoke = 0;

	// Sphere without smoke
	randomColor(color);
	for(i=0; i<75; i++){
		newp = addParticle(inSettings);
		newp->initStar(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = rsRandf(1.0f) - 0.5f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		temp = 600.0f + rsRandf(100.0f);
		newp->vel[0] *= temp;
		newp->vel[1] *= temp;
		newp->vel[2] *= temp;
		newp->vel[0] += vel[0];
		newp->vel[1] += vel[1];
		newp->vel[2] += vel[2];
		newp->rgb = color;
		newp->t = newp->tr = rsRandf(2.0f) + 2.0f;
		newp->makeSmoke = 0;
	}

	// disk of big streamers
	randomColor(color);
	for(i=0; i<50; i++){
		newp = addParticle(inSettings);
		newp->initStreamer(inSettings);
		newp->drag = 0.3f;
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = 0.0f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		newp->vel[0] *= 1000.0f + rsRandf(100.0f);
		newp->vel[1] += rsRandf(100.0f) - 50.0f;
		newp->vel[2] *= 1000.0f + rsRandf(100.0f);
		newp->vel[0] += vel[0];
		newp->vel[1] += vel[1];
		newp->vel[2] += vel[2];
		newp->rgb = color;
		newp->size = 100.0f;
		newp->t = newp->tr = rsRandf(6.0f) + 3.0f;
		newp->makeSmoke = 0;
	}

	if(soundengine)
		soundengine->insertSoundNode(NUKESOUND, xyz, inSettings->cameraPos);
}

void particle::initExplosion(SkyrocketSaverSettings *inSettings){
	type = EXPLOSION;
	displayList = inSettings->flarelist[0];
	drag = 0.612f;
	t = tr = 0.5f;
	bright = 1.0f;
	life = 1.0f;
	size = 100.0f;
	makeSmoke = 0;
	const float num_part_scale = 1.5f;

	// Don't do massive explosions too close to the ground
	if((explosiontype == 19 || explosiontype == 20) && (xyz[1] < 600.0f))
		explosiontype = 0;

	rsVec rgb2;
	switch(explosiontype){
	case 0:
		randomColor(rgb);
		if(!rsRandi(10))  // big sphere
			popSphere(int(225.0f * num_part_scale), 1000.0f, rgb, inSettings);
		else  // regular sphere
			popSphere(int(175.0f * num_part_scale), rsRandf(100.0f) + 400.0f, rgb, inSettings);
		break;
	case 1:
		randomColor(rgb);
		if(!rsRandi(10))  // big split sphere
			popSplitSphere(int(225.0f * num_part_scale), 1000.0f, rgb, inSettings);
		else  // regular split sphere
			popSplitSphere(int(175.0f * num_part_scale), rsRandf(100.0f) + 400.0f, rgb, inSettings);
		break;
	case 2:
		rgb.set(1.0f, 1.0f, 1.0f);
		if(!rsRandi(10))  // big multicolored sphere
			popMultiColorSphere(int(225.0f * num_part_scale), 1000.0f, inSettings);
		else  // regular multicolored sphere
			popMultiColorSphere(int(175.0f * num_part_scale), rsRandf(100.0f) + 400.0f, inSettings);
		break;
	case 3:  // ring
		randomColor(rgb);
		popRing(80, rsRandf(100.0f) + 400.0f, rgb, inSettings);
		break;
	case 4:  // double sphere
		randomColor(rgb);
		randomColor(rgb2);
		popSphere(int(90.0f * num_part_scale), rsRandf(50.0f) + 200.0f, rgb2, inSettings);
		popSphere(int(150.0f * num_part_scale), rsRandf(100.0f) + 500.0f, rgb, inSettings);
		break;
	case 5:  // sphere and ring
		randomColor(rgb);
		randomColor(rgb2);
		popRing(int(80.0f * num_part_scale), rsRandf(100.0f) + 500.0f, rgb2, inSettings);
		popSphere(int(150.0f * num_part_scale), rsRandf(50.0f) + 200.0f, rgb, inSettings);
		break;
	case 6:  // Sphere of streamers
		randomColor(rgb);
		popStreamers(int(40.0f * num_part_scale), rsRandf(100.0f) + 400.0f, rgb, inSettings);
		break;
	case 7:  // Sphere of meteors
		randomColor(rgb);
		popMeteors(int(40.0f * num_part_scale), rsRandf(100.0f) + 400.0f, rgb, inSettings);
		break;
	case 8:  // Small sphere of stars and large sphere of streamers
		randomColor(rgb);
		randomColor(rgb2);
		popStreamers(int(30.0f * num_part_scale), rsRandf(100.0f) + 500.0f, rgb, inSettings);
		popSphere(int(90.0f * num_part_scale), rsRandf(50.0f) + 200.0f, rgb2, inSettings);
		break;
	case 9:  // Small sphere of stars and large sphere of meteors
		randomColor(rgb);
		randomColor(rgb2);
		popMeteors(int(30.0f * num_part_scale), rsRandf(100.0f) + 500.0f, rgb, inSettings);
		popSphere(int(90.0f * num_part_scale), rsRandf(50.0f) + 200.0f, rgb2, inSettings);
		break;
	case 10:  // Sphere of streamers inside sphere of stars
		randomColor(rgb);
		randomColor(rgb2);
		popStreamers(int(30.0f * num_part_scale), rsRandf(100.0f) + 450.0f, rgb, inSettings);
		popSphere(int(150.0f * num_part_scale), rsRandf(50.0f) + 500.0f, rgb2, inSettings);
		break;
	case 11:  // Sphere of meteors inside sphere of stars
		randomColor(rgb);
		randomColor(rgb2);
		popMeteors(int(30.0f * num_part_scale), rsRandf(100.0f) + 450.0f, rgb, inSettings);
		popSphere(int(150.0f * num_part_scale), rsRandf(50.0f) + 500.0f, rgb2, inSettings);
		break;
	case 12:  // a few bombs that fall and explode into stars
		randomColor(rgb);
		popStarPoppers(8, rsRandf(100.0f) + 300.0f, rgb, inSettings);
		break;
	case 13:  // a few bombs that fall and explode into streamers
		randomColor(rgb);
		popStreamerPoppers(8, rsRandf(100.0f) + 300.0f, rgb, inSettings);
		break;
	case 14:  // a few bombs that fall and explode into meteors
		randomColor(rgb);
		popMeteorPoppers(8, rsRandf(100.0f) + 300.0f, rgb, inSettings);
		break;
	case 15:  // lots of little falling firecrackers
		popLittlePoppers(int(250.0f * num_part_scale), rsRandf(50.0f) + 150.0f, inSettings);
		break;
	case 16:
		randomColor(rgb);
		popBees(int(50.0f * num_part_scale), 10.0f, rgb, inSettings);
		break;
	case 17:  // Boom!  (loud noise and flash of light)
		rgb.set(1.0f, 1.0f, 1.0f);
		size = 150.0f;
		break;
	// 18 is a spinner, which doesn't require explosion
	case 19:
		rgb.set(1.0f, 1.0f, 1.0f);
		initSucker(inSettings);
		break;
	case 20:
		rgb.set(1.0f, 1.0f, 1.0f);
		initStretcher(inSettings);
		break;
	case 100:  // these three are little explosions for poppers
		popSphere(int(30.0f * num_part_scale), 100.0f, rgb, inSettings);
		break;
	case 101:
		popStreamers(10, 100.0f, rgb, inSettings);
		break;
	case 102:
		popMeteors(10, 100.0f, rgb, inSettings);
	}

	if(soundengine){
		if(explosiontype == 17)  // extra resounding boom
			soundengine->insertSoundNode(BOOM4SOUND, xyz, inSettings->cameraPos);
		// make bees and big booms whistle sometimes
		if(explosiontype == 16 || explosiontype == 17)
			if(rsRandi(2))
				soundengine->insertSoundNode(WHISTLESOUND, xyz, inSettings->cameraPos);
		// regular booms
		if(explosiontype <= 16 || explosiontype >= 100)
			soundengine->insertSoundNode(BOOM1SOUND + rsRandi(3), xyz, inSettings->cameraPos);
	// sucker and stretcher take care of their own sounds
	}
}

void particle::popSphere(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings){
	particle* newp;

	for(int i=0; i<numParts; ++i){
		newp = addParticle(inSettings);
		newp->initStar(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = rsRandf(1.0f) - 0.5f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		newp->vel *= v0 + rsRandf(50.0f);
		newp->vel += vel;
		newp->rgb = color;
	}

	if(!rsRandi(100))
		newp->t = newp->tr = rsRandf(20.0f) + 5.0f;
}

void particle::popSplitSphere(int numParts, float v0, rsVec color1, SkyrocketSaverSettings *inSettings){
	particle* newp;
	rsVec color2;
	rsVec planeNormal;

	randomColor(color2);
	planeNormal[0] = rsRandf(1.0f) - 0.5f;
	planeNormal[1] = rsRandf(1.0f) - 0.5f;
	planeNormal[2] = rsRandf(1.0f) - 0.5f;
	planeNormal.normalize();
	for(int i=0; i<numParts; i++){
		newp = addParticle(inSettings);
		newp->initStar(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = rsRandf(1.0f) - 0.5f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		if(planeNormal.dot(newp->vel) > 0.0f)
			newp->rgb = color1;
		else
			newp->rgb = color2;
		newp->vel *= v0 + rsRandf(50.0f);
		newp->vel += vel;
	}

	if(!rsRandi(100))
		newp->t = newp->tr = rsRandf(20.0f) + 5.0f;
}

void particle::popMultiColorSphere(int numParts, float v0, SkyrocketSaverSettings *inSettings){
	particle* newp;
	rsVec color[3];

	randomColor(color[0]);
	randomColor(color[1]);
	randomColor(color[2]);
	int j(0);
	for(int i=0; i<numParts; ++i){
		newp = addParticle(inSettings);
		newp->initStar(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = rsRandf(1.0f) - 0.5f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		newp->vel *= v0 + rsRandf(30.0f);
		newp->vel += vel;
		newp->rgb = color[j];
		++j;
		if(j >= 3)
			j = 0;
	}

	if(!rsRandi(100))
		newp->t = newp->tr = rsRandf(20.0f) + 5.0f;
}

void particle::popRing(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings){
	particle* newp;

	float temph = rsRandf(PI);  // heading
	float tempp = rsRandf(PI);  // pitch
	const float ch(cosf(temph));
	const float sh(sinf(temph));
	const float cp(cosf(tempp));
	const float sp(sinf(tempp));
	for(int i=0; i<numParts; ++i){
		newp = addParticle(inSettings);
		newp->initStar(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = 0.0f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		// pitch
		newp->vel[1] = sp * newp->vel[2];
		newp->vel[2] = cp * newp->vel[2];
		// heading
		const float temp(newp->vel[0]);
		newp->vel[0] = ch * temp + sh * newp->vel[1];
		newp->vel[1] = -sh * temp + ch * newp->vel[1];
		// multiply velocity
		newp->vel[0] *= v0 + rsRandf(50.0f);
		newp->vel[1] *= v0 + rsRandf(50.0f);
		newp->vel[2] *= v0 + rsRandf(50.0f);
		newp->vel += vel;
		newp->rgb = color;
	}

	if(!rsRandi(100))
		newp->t = newp->tr = rsRandf(20.0f) + 5.0f;
}

void particle::popStreamers(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings){
	particle* newp;

	for(int i=0; i<numParts; ++i){
		newp = addParticle(inSettings);
		newp->initStreamer(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = rsRandf(1.0f) - 0.5f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		newp->vel *= v0 + rsRandf(50.0f);
		newp->vel += vel;
		newp->rgb = color;
	}
}

void particle::popMeteors(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings){
	particle* newp;

	for(int i=0; i<numParts; ++i){
		newp = addParticle(inSettings);
		newp->initMeteor(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = rsRandf(1.0f) - 0.5f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel.normalize();
		newp->vel *= v0 + rsRandf(50.0f);
		newp->vel += vel;
		newp->rgb = color;
	}
}

void particle::popStarPoppers(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings){
	particle* newp;
	const float v0x2(v0 * 2.0f);

	for(int i=0; i<numParts; ++i){
		newp = addParticle(inSettings);
		newp->initStarPopper(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = vel[0] + rsRandf(v0x2) - v0;
		newp->vel[1] = vel[1] + rsRandf(v0x2) - v0;
		newp->vel[2] = vel[2] + rsRandf(v0x2) - v0;
		newp->rgb = color;
	}
}

void particle::popStreamerPoppers(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings){
	particle* newp;
	const float v0x2(v0 * 2.0f);

	for(int i=0; i<numParts; ++i){
		newp = addParticle(inSettings);
		newp->initStreamerPopper(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = vel[0] + rsRandf(v0x2) - v0;
		newp->vel[1] = vel[1] + rsRandf(v0x2) - v0;
		newp->vel[2] = vel[2] + rsRandf(v0x2) - v0;
		newp->rgb = color;
	}
}

void particle::popMeteorPoppers(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings){
	particle* newp;
	const float v0x2(v0 * 2.0f);

	for(int i=0; i<numParts; ++i){
		newp = addParticle(inSettings);
		newp->initMeteorPopper(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = vel[0] + rsRandf(v0x2) - v0;
		newp->vel[1] = vel[1] + rsRandf(v0x2) - v0;
		newp->vel[2] = vel[2] + rsRandf(v0x2) - v0;
		newp->rgb = color;
	}
}

void particle::popLittlePoppers(int numParts, float v0, SkyrocketSaverSettings *inSettings){
	particle* newp;
	const float v0x2(v0 * 2.0f);

	for(int i=0; i<numParts; ++i){
		newp = addParticle(inSettings);
		newp->initLittlePopper(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = vel[0] + rsRandf(v0x2) - v0;
		newp->vel[1] = vel[1] + rsRandf(v0x2) - v0;
		newp->vel[2] = vel[2] + rsRandf(v0x2) - v0;
	}

	if(soundengine)
		soundengine->insertSoundNode(POPPERSOUND, xyz, inSettings->cameraPos);
}

void particle::popBees(int numParts, float v0, rsVec color, SkyrocketSaverSettings *inSettings){
	particle* newp;

	for(int i=0; i<numParts; ++i){
		newp = addParticle(inSettings);
		newp->initBee(inSettings);
		newp->xyz = xyz;
		newp->vel[0] = rsRandf(1.0f) - 0.5f;
		newp->vel[1] = rsRandf(1.0f) - 0.5f;
		newp->vel[2] = rsRandf(1.0f) - 0.5f;
		newp->vel *= v0;
		newp->vel += vel;
		newp->rgb = color;
	}
}

void particle::findDepth(SkyrocketSaverSettings *inSettings){
	// This isn't the actual distance from the camera.  It is the the
	// distance along the view vector coming straight out of the camera.
	// This is calculated with a simple dot product.  The billboards don't
	// actually face the camera; they all face the same direction (straight
	// down the view vector of the camera, so sorting is done a little
	// differently than one might expect).
	depth = (inSettings->cameraPos[0] - xyz[0]) * float(inSettings->billboardMat[8])
			+ (inSettings->cameraPos[1] - xyz[1]) * float(inSettings->billboardMat[9])
			+ (inSettings->cameraPos[2] - xyz[2]) * float(inSettings->billboardMat[10]);
}


//******************************************
//  Update particles
//******************************************
void particle::update(SkyrocketSaverSettings *inSettings){
	int i;
	float temp;
	rsVec dir, crossvec;
	rsQuat spinquat;
	rsMatrix spinmat;
	particle *newp;
	rsVec rocketEjection;

	// update velocities
	if(type == ROCKET && life > endthrust){
		dir = vel;
		dir.normalize();
		crossvec.cross(dir, tiltvec);  // correct sidevec
		tiltvec.cross(crossvec, dir);
		tiltvec.normalize();
		spinquat.make(spin * inSettings->frameTime, dir[0], dir[1], dir[2]);  // twist tiltvec
		spinmat.fromQuat(spinquat);
		tiltvec.transVec(spinmat);
		vel += dir * (thrust * inSettings->frameTime);  // apply thrust
		vel += tiltvec * (tilt * inSettings->frameTime);  // apply tilt
	}
	if(type == BEE){
		vel[0] += 500.0f * cosf(tiltvec[0]) * inSettings->frameTime;
		vel[1] += 500.0f * (cosf(tiltvec[1]) - 0.2f) * inSettings->frameTime;
		vel[2] += 500.0f * cosf(tiltvec[2]) * inSettings->frameTime;
	}
	if(type != SMOKE)
		vel[1] -= inSettings->frameTime * 32.0f;  // gravity
	// apply air resistance
	temp = 1.0f / (1.0f + drag * inSettings->frameTime);
	vel *= temp * temp;

	// update position
	// (Fountains don't move)
	if(type != FOUNTAIN){
		lastxyz = xyz;
		xyz += vel * inSettings->frameTime;
		// Wind:  1/10 wind on ground; -1/2 wind at 500 feet; full wind at 2000 feet;
		// This value is calculated to coincide with movement of the clouds in world.h
		// Here's the polynomial wind equation that simulates windshear:
		xyz[0] += (0.1f - 0.00175f * xyz[1] + 0.0000011f * xyz[1] * xyz[1]) * inSettings->dWind * inSettings->frameTime;
	}

	// brightness and life
	tr -= inSettings->frameTime;
	switch(type){
	case ROCKET:
		life = tr / t;
		if(life > endthrust){  // Light up rocket gradually after it is launched
			bright += 2.0f * inSettings->frameTime;
			if(bright > 0.5f)
				bright = 0.5f;
		}
		else{  // Darken rocket after it stops thrusting
			bright -= inSettings->frameTime;
			if(bright < 0.0f)
				bright = 0.0f;
		}
		break;
	case SMOKE:
		life = tr / t;
		bright = life * 0.7f;
		size += (30.0f - size) * (1.2f * inSettings->frameTime);
		break;
	case FOUNTAIN:
	case SPINNER:
		life = tr / t;
		bright = life * life;
		// dim newborn fountains and spinners
		temp = t - tr;
		if(temp < 0.5f)
			bright *= temp * 2.0f;
		break;
	case EXPLOSION:
		life = tr / t;
		bright = life * life;
		break;
	case STAR:
	case STREAMER:
	case METEOR:
		temp = (t - tr) / t;
		temp = temp * temp;
		bright = 1.0f - (temp * temp);
		life = bright;
		break;
	case POPPER:
		life = tr;
		break;
	case BEE:
		temp = (t - tr) / t;
		temp = temp * temp;
		bright = 1.0f - (temp * temp);
		life = bright;
		// Update bee acceleration (tiltvec) using misused variables
		tiltvec[0] += thrust * inSettings->frameTime;
		tiltvec[1] += endthrust * inSettings->frameTime;
		tiltvec[2] += spin * inSettings->frameTime;
		break;
	case SUCKER:
		life = tr / t;
		bright = life;
		size = 250.0f * life;
		break;
	case SHOCKWAVE:
		life = tr / t;
		bright = life;
		rgb[2] = life * 0.5f + 0.5f;  // get a little yellow
		size += 400.0f * inSettings->frameTime;
		break;
	case STRETCHER:
		life = tr / t;
		bright = 1.0f - ((1.0f - life) * (1.0f - life));
		size = 400.0f * bright;
		break;
	case BIGMAMA:
		life = tr / t;
		bright = life * 2.0f - 1.0f;
		if(bright < 0.0f)
			bright = 0.0f;
		size += 1500.0f * inSettings->frameTime;
	}

	// Produce smoke from rockets and other particles
	static rsVec velvec;
	if(makeSmoke && inSettings->dSmoke){
		rsVec diff = xyz - lastxyz;
		// distance rocket traveled since last frame
		temp = diff.length();
		smokeTrailLength += temp;
		// number of smoke puffs to release (1 every 2 feet)
		int puffs = int(smokeTrailLength * 0.5f);
		float multiplier = 2.0f / smokeTrailLength;
		smokeTrailLength -= float(puffs) * 2.0f;
		rsVec smkpos = lastxyz;
		if((type == ROCKET) && (life > endthrust)){  // eject the smoke forcefully
			rocketEjection = vel;
			rocketEjection.normalize();
			rocketEjection *= -2.0f * thrust * (life - endthrust);
			for(i=0; i<puffs; ++i){  // make puffs of smoke
				smkpos += diff * multiplier;
				newp = addParticle(inSettings);
				velvec[0] = rocketEjection[0] + rsRandf(20.0f) - 10.0f;
				velvec[1] = rocketEjection[1] + rsRandf(20.0f) - 10.0f;
				velvec[2] = rocketEjection[2] + rsRandf(20.0f) - 10.0f;
				newp->initSmoke(smkpos, velvec, inSettings);
				newp->t = newp->tr = inSettings->smokeTime[smokeTimeIndex];
				++smokeTimeIndex;
				if(smokeTimeIndex >= SMOKETIMES)
					smokeTimeIndex = 0;
			}
		}
		else{  // just form smoke in place
			for(i=0; i<puffs; ++i){
				smkpos += diff * multiplier;
				newp = addParticle(inSettings);
				velvec[0] = rsRandf(20.0f) - 10.0f;
				velvec[1] = rsRandf(20.0f) - 10.0f;
				velvec[2] = rsRandf(20.0f) - 10.0f;
				newp->initSmoke(smkpos, velvec, inSettings);
				newp->t = newp->tr = inSettings->smokeTime[smokeTimeIndex];
				++smokeTimeIndex;
				if(smokeTimeIndex >= SMOKETIMES)
					smokeTimeIndex = 0;
			}
		}
	}

	// Sparks thrusting from rockets
	if((type == ROCKET) && (life > endthrust)){
		rsVec diff = xyz - lastxyz;
		// distance rocket traveled since last frame
		temp = diff.length();
		sparkTrailLength += temp;
		// number of sparks to release
		int sparks = int(sparkTrailLength * 0.4f);
		sparkTrailLength -= float(sparks) * 2.5f;
		rocketEjection = vel;
		rocketEjection.normalize();
		rocketEjection *= -thrust * (life - endthrust);
		for(i=0; i<sparks; ++i){  // make sparks
			newp = addParticle(inSettings);
			newp->initStar(inSettings);
			newp->xyz = xyz - (diff * rsRandf(1.0f));
			newp->vel[0] = rocketEjection[0] + rsRandf(60.0f) - 30.0f;
			newp->vel[1] = rocketEjection[1] + rsRandf(60.0f) - 30.0f;
			newp->vel[2] = rocketEjection[2] + rsRandf(60.0f) - 30.0f;
			newp->rgb = rgb;
			newp->t = rsRandf(0.2f) + 0.1f;
			newp->tr = newp->t;
			newp->size = 8.0f * life;
			newp->displayList = inSettings->flarelist[3];
			newp->makeSmoke = 0;
		}
	}

	// Stars shooting up from fountain
	if(type == FOUNTAIN){
		// spew 10-20 particles per second at maximum brightness
		sparkTrailLength += inSettings->frameTime * bright * (rsRandf(10.0f) + 10.0f);
		int sparks = int(sparkTrailLength);
		sparkTrailLength -= float(sparks);
		for(i=0; i<sparks; ++i){
			newp = addParticle(inSettings);
			newp->initStar(inSettings);
			newp->drag = 0.342f;  // terminal velocity is 40 ft/s
			newp->xyz = xyz;
			newp->xyz[1] += rsRandf(inSettings->frameTime * 100.0f);
			if(newp->xyz[1] > 50.0f)
				newp->xyz[1] = 50.0f;
			newp->vel.set(rsRandf(20.0f) - 10.0f,
				rsRandf(30.0f) + 100.0f,
				rsRandf(20.0f) - 10.0f);
			newp->size = 10.0f;
			newp->rgb = rgb;
			newp->makeSmoke = 0;
		}
	}

	// Stars shooting out from spinner
	if(type == SPINNER){
		dir.set(1.0f, 0.0f, 0.0f);
		crossvec.cross(dir, tiltvec);
		crossvec.normalize();
		crossvec *= 400.0f;
		temp = spin * inSettings->frameTime;  // radius of spin this frame
		// spew 90-100 particles per second at maximum brightness
		sparkTrailLength += inSettings->frameTime * bright * (rsRandf(10.0f) + 90.0f);
		int sparks = int(sparkTrailLength);
		sparkTrailLength -= float(sparks);
		for(i=0; i<sparks; ++i){
			spinquat.make(tilt + rsRandf(temp), tiltvec[0], tiltvec[1], tiltvec[2]);
			spinquat.toMat(spinmat.m);
			newp = addParticle(inSettings);
			newp->initStar(inSettings);
			newp->xyz = xyz;
			newp->vel.set(vel[0] - (spinmat[0]*crossvec[0] + spinmat[4]*crossvec[1] + spinmat[8]*crossvec[2]) + rsRandf(20.0f) - 10.0f,
				vel[1] - (spinmat[1]*crossvec[0] + spinmat[5]*crossvec[1] + spinmat[9]*crossvec[2]) + rsRandf(20.0f) - 10.0f,
				vel[2] - (spinmat[2]*crossvec[0] + spinmat[6]*crossvec[1] + spinmat[10]*crossvec[2]) + rsRandf(20.0f) - 10.0f);
			newp->size = 15.0f;
			newp->rgb = rgb;
			newp->makeSmoke = 0;
			newp->t = newp->tr = rsRandf(0.5f) + 1.5f;
		}
		tilt += temp;
	}

	// trail from streamers
	if(type == STREAMER){
		rsVec diff = xyz - lastxyz;
		// distance streamer traveled since last frame
		sparkTrailLength += diff.length();
		// number of sparks to release each frame
		int sparks = int(sparkTrailLength * 0.1f);
		sparkTrailLength -= float(sparks) * 10.0f;
		for(i=0; i<sparks; ++i){
			newp = addParticle(inSettings);
			newp->initStar(inSettings);
			newp->xyz = xyz - (diff * rsRandf(1.0f));
			newp->vel.set(vel[0] + rsRandf(50.0f) - 25.0f, 
				vel[1] + rsRandf(50.0f) - 25.0f, 
				vel[2] + rsRandf(50.0f) - 25.0f);
			newp->rgb.set(1.0f, 0.7f, 0.4f);
			newp->size = rsRandf(5.0f) + 5.0f;
			newp->drag = 2.0f;
			newp->t = newp->tr = rsRandf(2.0f) + 1.0f;
			newp->tr = newp->t;
			newp->makeSmoke = 0;
		}
	}

	// trail from meteors
	if(type == METEOR){
		rsVec diff = xyz - lastxyz;
		// distance meteor traveled since last frame
		sparkTrailLength += diff.length();
		// number of sparks to release
		int sparks = int(sparkTrailLength * 0.1f);
		sparkTrailLength -= float(sparks) * 10.0f;
		for(i=0; i<sparks; ++i){
			newp = addParticle(inSettings);
			newp->initStar(inSettings);
			newp->xyz = xyz - (diff * rsRandf(1.0f));
			newp->vel.set(vel[0] + rsRandf(100.0f) - 50.0f,
				vel[1] + rsRandf(100.0f) - 50.0f,
				vel[2] + rsRandf(100.0f) - 50.0f);
			newp->rgb = rgb;
			newp->size = rsRandf(5.0f) + 5.0f;
			newp->drag = 2.0f;
			newp->t = newp->tr = rsRandf(0.5f) + 1.5f;
			newp->makeSmoke = 0;
		}
	}

	// trail from bees
	if(type == BEE){
		rsVec diff = xyz - lastxyz;
		// distance rocket traveled since last frame
		sparkTrailLength += diff.length();
		// number of sparks to release
		int sparks = int(sparkTrailLength * 0.1f);
		rsVec smkpos = lastxyz;
		// release sparks every 10 feet
		float multiplier = 10.0f / sparkTrailLength;
		for(i=0; i<sparks; i++){
			smkpos += diff * multiplier;
			newp = addParticle(inSettings);
			newp->initStar(inSettings);
			newp->xyz = smkpos;
			newp->vel.set(rsRandf(100.0f) - 50.0f - vel[0] * 0.5f,
				rsRandf(100.0f) - 50.0f - vel[1] * 0.5f,
				rsRandf(100.0f) - 50.0f - vel[2] * 0.5f);
			newp->rgb = rgb;
			newp->t = newp->tr = rsRandf(0.1f) + 0.15f;
			newp->size = 7.0f;
			newp->displayList = inSettings->flarelist[3];
			newp->makeSmoke = 0;
		}
		sparkTrailLength -= float(sparks) * 10.0f;
	}

	// smoke and cloud illumination from rockets and explosions
	if(inSettings->dIllumination && ((type == ROCKET) || (type == FOUNTAIN) || (type == EXPLOSION)))
		illuminate(this, inSettings);

	// pulling of particles by suckers
	if(type == SUCKER)
		pulling(this, inSettings);

	// pushing of particles by shockwaves
	if(type == SHOCKWAVE)
		pushing(this, inSettings);

	// stretching of particles by stretchers
	if(type == STRETCHER)
		stretching(this, inSettings);

	// thrust sound from rockets
	//if((type == ROCKET) && dSound)
	//	insertSoundNode(THRUSTSOUND, xyz, cameraPos);
}


void particle::draw(SkyrocketSaverSettings *inSettings){
	if(life <= 0.0f)
		return;  // don't draw dead particles

	// cull small particles that are behind camera
	if(depth < 0.0f && type != SHOCKWAVE)
		return;

	// don't draw invisible particles
	if(type == POPPER)
		return;

	glPushMatrix();
	glTranslatef(xyz[0], xyz[1], xyz[2]);

	switch(type){
	case SHOCKWAVE:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glPushMatrix();
			glScalef(size, size, size);
			drawShockwave(life, float(sqrt(size)) * 0.05f, inSettings);
		glPopMatrix();
		glMultMatrixf(inSettings->billboardMat);
		glScalef(size * 0.1f, size * 0.1f, size * 0.1f);
		glColor4f(0.5f, 1.0f, 0.5f, bright);
		glCallList(inSettings->flarelist[0]);
		glScalef(0.35f, 0.35f, 0.35f);
		glColor4f(1.0f, 1.0f, 1.0f, bright);
		glCallList(inSettings->flarelist[0]);
		if(life > 0.7f){  // Big torus just for fun
			//glMultMatrixf(billboardMat);
			glScalef(100.0f, 100.0f, 100.0f);
			glColor4f(1.0f, life, 1.0f, (life - 0.7f) * 3.333f);
			glCallList(inSettings->flarelist[2]);
		}
		break;
	case SMOKE:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glMultMatrixf(inSettings->billboardMat);
		glScalef(size, size, size);
		glColor4f(rgb[0], rgb[1], rgb[2], bright);
		glCallList(displayList);
		break;
	case EXPLOSION:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glMultMatrixf(inSettings->billboardMat);
		glScalef(size, size, size);
		glColor4f(1.0f, 1.0f, 1.0f, bright);
		glScalef(bright, bright, bright);
		glCallList(displayList);
		break;
	default:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glMultMatrixf(inSettings->billboardMat);
		glScalef(size, size, size);
		glColor4f(rgb[0], rgb[1], rgb[2], bright);
		glCallList(displayList);
		glScalef(0.35f, 0.35f, 0.35f);
		glColor4f(1.0f, 1.0f, 1.0f, bright);
		glCallList(displayList);
	}

	glPopMatrix();
}
