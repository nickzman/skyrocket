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


#ifndef WORLD_H
#define WORLD_H



#define STARMESH 12
#define STARTEXSIZE 1024
#define MOONGLOWTEXSIZE 128
#define CLOUDMESH 70



/*extern int dWind;
extern int dAmbient;
extern int dStardensity;
extern int dMoonglow;
extern int dMoon;
extern int dClouds;
extern int dEarth;*/
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include "Skyrocket.h"


class World{
public:
	int doSunset;
	float moonRotation, moonHeight;
	float cloudShift;
	float stars[STARMESH+1][STARMESH/2][6];  // 6 = x,y,z,u,v,bright
	float clouds[CLOUDMESH+1][CLOUDMESH+1][9];  // 9 = x,y,z,u,v,std bright,r,g,b
	unsigned int starlist;
	GLuint startex;
	unsigned int moonlist;
	GLuint moontex;
	unsigned int moonglowlist;
	GLuint moonglowtex;
	GLuint cloudtex;
	GLuint sunsettex;
	unsigned int sunsetlist;
	GLuint earthneartex;
	GLuint earthfartex;
	GLuint earthlighttex;
	unsigned int earthlist;
	unsigned int earthnearlist;
	unsigned int earthfarlist;

	World(SkyrocketSaverSettings *inSettings);
	~World(){}
	// For building mountain sillohettes in sunset
	void makeHeights(int first, int last, int *h);
	void update(float frameTime, SkyrocketSaverSettings *inSettings);
	void draw(SkyrocketSaverSettings *inSettings);
};



#endif