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


#ifndef SOUND_H
#define SOUND_H



/*#include <windows.h>
#include <al.h>
#include <alc.h>
#include <rsMath/rsMath.h>
#include <math.h>*/
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#include "alut.h"
#include <OpenAL/MacOSX_OALExtensions.h>
#include "rsMath.h"
#include <math.h>


#define NUM_SOUNDNODES 100
#define NUM_SOURCES 16  // 16 is the maximum that works on my computer
#define NUM_BUFFERS 10

#define LAUNCH1SOUND 0
#define LAUNCH2SOUND 1
#define BOOM1SOUND 2
#define BOOM2SOUND 3
#define BOOM3SOUND 4
#define BOOM4SOUND 5
#define POPPERSOUND 6
#define SUCKSOUND 7
#define NUKESOUND 8
#define WHISTLESOUND 9



class SoundEngine{
public:
	ALCcontext* context;
	ALCdevice* device;
	ALuint buffers[NUM_BUFFERS];
	ALuint sources[NUM_SOURCES];

	class SoundNode{
	public:
		int sound;
		float pos[3];
		float dist;
		float time;  // time until sound plays
		bool active;

		SoundNode(){active = false;}
		~SoundNode(){}
	};
	SoundNode soundnodes[NUM_SOUNDNODES];

	SoundEngine(float volume);
	~SoundEngine();
	void insertSoundNode(int sound, rsVec source, rsVec observer);
	void update(float* listenerPos, float* listenerVel, float* listenerOri, float frameTime, bool slowMotion);
};



#endif  // SOUND_H
