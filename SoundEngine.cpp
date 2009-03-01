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


/*#include <Skyrocket/SoundEngine.h>
#include <Skyrocket/launchsound.h>
#include <Skyrocket/boomsound.h>
#include <Skyrocket/poppersound.h>
#include <Skyrocket/sucksound.h>
#include <Skyrocket/nukesound.h>
#include <Skyrocket/whistlesound.h>*/
#include "SoundEngine.h"
/*#include "launchsound.h"
#include "boomsound.h"
#include "poppersound.h"
#include "sucksound.h"
#include "nukesound.h"
#include "whistlesound.h"*/
#include "MacHelperFunctions.h"

// sound is about halfway attenuated at reference distance
static float reference_distance[NUM_BUFFERS] = 
	{10.0f,  // launch sounds
	10.0f,
	1000.0f,  // booms
	1000.0f,
	1000.0f,
	1200.0f,
	700.0f,  // poppers
	1500.0f,  // suck
	2000.0f,  // nuke
	700.0f  // whistle
};


SoundEngine::SoundEngine(float volume){
	ALvoid *launch1SoundData, *launch2SoundData, *boom1SoundData, *boom2SoundData, *boom3SoundData, *boom4SoundData, *popperSoundData, *suckSoundData, *nukeSoundData, *whistleSoundData;
	ALenum launch1SoundFormat, launch2SoundFormat, boom1SoundFormat, boom2SoundFormat, boom3SoundFormat, boom4SoundFormat, popperSoundFormat, suckSoundFormat, nukeSoundFormat, whistleSoundFormat;
	ALsizei launch1SoundSize, launch2SoundSize, boom1SoundSize, boom2SoundSize, boom3SoundSize, boom4SoundSize, popperSoundSize, suckSoundSize, nukeSoundSize, whistleSoundSize;
	ALsizei launch1SoundFreq, launch2SoundFreq, boom1SoundFreq, boom2SoundFreq, boom3SoundFreq, boom4SoundFreq, popperSoundFreq, suckSoundFreq, nukeSoundFreq, whistleSoundFreq;
	
	// Open device
	//device = alcOpenDevice(ALubyte*)"DirectSound3D");  // specific device
 	device = alcOpenDevice(NULL);  // default device
	if(device == NULL)
		return;
	// Create context
	context = alcCreateContext(device, NULL);
	if(context == NULL)
		return;
	// Set active context
	alcMakeContextCurrent(context);

	alDistanceModel(AL_INVERSE_DISTANCE);
	alDopplerVelocity(1130.0f);  // Sound travels at 1130 feet/sec
	alListenerf(AL_GAIN, volume);  // Volume
	
	alutLoadWAVFile((ALbyte *)PathForResourceOfType("launch1", "wav"), &launch1SoundFormat, &launch1SoundData, &launch1SoundSize, &launch1SoundFreq);
	alutLoadWAVFile((ALbyte *)PathForResourceOfType("launch2", "wav"), &launch2SoundFormat, &launch2SoundData, &launch2SoundSize, &launch2SoundFreq);
	alutLoadWAVFile((ALbyte *)PathForResourceOfType("boom1", "wav"), &boom1SoundFormat, &boom1SoundData, &boom1SoundSize, &boom1SoundFreq);
	alutLoadWAVFile((ALbyte *)PathForResourceOfType("boom2", "wav"), &boom2SoundFormat, &boom2SoundData, &boom2SoundSize, &boom2SoundFreq);
	alutLoadWAVFile((ALbyte *)PathForResourceOfType("boom3", "wav"), &boom3SoundFormat, &boom3SoundData, &boom3SoundSize, &boom3SoundFreq);
	alutLoadWAVFile((ALbyte *)PathForResourceOfType("boom4", "wav"), &boom4SoundFormat, &boom4SoundData, &boom4SoundSize, &boom4SoundFreq);
	alutLoadWAVFile((ALbyte *)PathForResourceOfType("poppers1", "wav"), &popperSoundFormat, &popperSoundData, &popperSoundSize, &popperSoundFreq);
	alutLoadWAVFile((ALbyte *)PathForResourceOfType("sucker", "wav"), &suckSoundFormat, &suckSoundData, &suckSoundSize, &suckSoundFreq);
	alutLoadWAVFile((ALbyte *)PathForResourceOfType("nuke", "wav"), &nukeSoundFormat, &nukeSoundData, &nukeSoundSize, &nukeSoundFreq);
	alutLoadWAVFile((ALbyte *)PathForResourceOfType("whistle1", "wav"), &whistleSoundFormat, &whistleSoundData, &whistleSoundSize, &whistleSoundFreq);

	// Initialize sound data
	alGenBuffers(NUM_BUFFERS, buffers);
	/*alBufferData(buffers[LAUNCH1SOUND], AL_FORMAT_MONO16, launch1SoundData, launch1SoundSize, 44100);
	alBufferData(buffers[LAUNCH2SOUND], AL_FORMAT_MONO16, launch2SoundData, launch2SoundSize, 44100);
	alBufferData(buffers[BOOM1SOUND], AL_FORMAT_MONO16, boom1SoundData, boom1SoundSize, 44100);
	alBufferData(buffers[BOOM2SOUND], AL_FORMAT_MONO16, boom2SoundData, boom2SoundSize, 44100);
	alBufferData(buffers[BOOM3SOUND], AL_FORMAT_MONO16, boom3SoundData, boom3SoundSize, 44100);
	alBufferData(buffers[BOOM4SOUND], AL_FORMAT_MONO16, boom4SoundData, boom4SoundSize, 44100);
	alBufferData(buffers[POPPERSOUND], AL_FORMAT_MONO16, popperSoundData, popperSoundSize, 44100);
	alBufferData(buffers[SUCKSOUND], AL_FORMAT_MONO16, suckSoundData, suckSoundSize, 44100);
	alBufferData(buffers[NUKESOUND], AL_FORMAT_MONO16, nukeSoundData, nukeSoundSize, 44100);
	alBufferData(buffers[WHISTLESOUND], AL_FORMAT_MONO16, whistleSoundData, whistleSoundSize, 44100);*/
	alEnable(ALC_MAC_OSX_CONVERT_DATA_UPON_LOADING);	// if you're getting a build error on this line, it means you need to upgrade to Xcode 2.4
	alBufferData(buffers[LAUNCH1SOUND], launch1SoundFormat, launch1SoundData, launch1SoundSize, launch1SoundFreq);
	alBufferData(buffers[LAUNCH2SOUND], launch2SoundFormat, launch2SoundData, launch2SoundSize, launch2SoundFreq);
	alBufferData(buffers[BOOM1SOUND], boom1SoundFormat, boom1SoundData, boom1SoundSize, boom1SoundFreq);
	alBufferData(buffers[BOOM2SOUND], boom2SoundFormat, boom2SoundData, boom2SoundSize, boom2SoundFreq);
	alBufferData(buffers[BOOM3SOUND], boom3SoundFormat, boom3SoundData, boom3SoundSize, boom3SoundFreq);
	alBufferData(buffers[BOOM4SOUND], boom4SoundFormat, boom4SoundData, boom4SoundSize, boom4SoundFreq);
	alBufferData(buffers[POPPERSOUND], popperSoundFormat, popperSoundData, popperSoundSize, popperSoundFreq);
	alBufferData(buffers[SUCKSOUND], suckSoundFormat, suckSoundData, suckSoundSize, suckSoundFreq);
	alBufferData(buffers[NUKESOUND], nukeSoundFormat, nukeSoundData, nukeSoundSize, nukeSoundFreq);
	alBufferData(buffers[WHISTLESOUND], whistleSoundFormat, whistleSoundData, whistleSoundSize, whistleSoundFreq);
	
	alutUnloadWAV(launch1SoundFormat, launch1SoundData, launch1SoundSize, launch1SoundFreq);
	alutUnloadWAV(launch2SoundFormat, launch2SoundData, launch2SoundSize, launch2SoundFreq);
	alutUnloadWAV(boom1SoundFormat, boom1SoundData, boom1SoundSize, boom1SoundFreq);
	alutUnloadWAV(boom2SoundFormat, boom2SoundData, boom2SoundSize, boom2SoundFreq);
	alutUnloadWAV(boom3SoundFormat, boom3SoundData, boom3SoundSize, boom3SoundFreq);
	alutUnloadWAV(boom4SoundFormat, boom4SoundData, boom4SoundSize, boom4SoundFreq);
	alutUnloadWAV(popperSoundFormat, popperSoundData, popperSoundSize, popperSoundFreq);
	alutUnloadWAV(suckSoundFormat, suckSoundData, suckSoundSize, suckSoundFreq);
	alutUnloadWAV(nukeSoundFormat, nukeSoundData, nukeSoundSize, nukeSoundFreq);
	alutUnloadWAV(whistleSoundFormat, whistleSoundData, whistleSoundSize, whistleSoundFreq);

	alGenSources(NUM_SOURCES, sources);
	for(int i=0; i<NUM_SOURCES; ++i){
		alSourcef(sources[i], AL_GAIN, 1.0f);
		alSourcef(sources[i], AL_ROLLOFF_FACTOR, 1.0f);
		alSourcei(sources[i], AL_LOOPING, AL_FALSE);
	}
}


SoundEngine::~SoundEngine(){
	alDeleteBuffers(NUM_BUFFERS, buffers);
	alDeleteSources(NUM_SOURCES, sources);
	//Release context
	alcDestroyContext(context);
	//Close device
	alcCloseDevice(device);
}

void SoundEngine::insertSoundNode(int sound, rsVec source, rsVec observer){
	rsVec dir = observer - source;

	// find an available SoundNode
	int index = -1;
	int i = 0;
	while(index < 0 && i < NUM_SOUNDNODES){
		if(soundnodes[i].active == false)
			index = i;
		++i;
	}

	// escape if no SoundNode is available
	if(index == -1)
		return;

	soundnodes[index].sound = sound;
	if(soundnodes[index].sound == POPPERSOUND)  // poppers have a little delay
		soundnodes[index].time += 2.5f;
	soundnodes[index].pos[0] = source[0];
	soundnodes[index].pos[1] = source[1];
	soundnodes[index].pos[2] = source[2];
	// distance to sound
	soundnodes[index].dist = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
	// Sound travels at 1130 feet/sec
	soundnodes[index].time = soundnodes[index].dist * 0.000885f;
	soundnodes[index].active = true;
}


void SoundEngine::update(float* listenerPos, float* listenerVel, float* listenerOri, float frameTime, bool slowMotion){
	if(device == NULL || context == NULL)
		return;

	// Set current listener attributes
	alListenerfv(AL_POSITION, listenerPos);
	//alListenerfv(AL_VELOCITY, listenerVel);
	alListenerfv(AL_ORIENTATION, listenerOri);

	for(int i=0; i<NUM_SOUNDNODES; ++i){
		if(soundnodes[i].active == true){
			soundnodes[i].time -= frameTime;
			if(soundnodes[i].time <= 0.0f){
				// find an available source
				ALint value;
				int src_index = -1;
				int si = 0;
				while(si < NUM_SOURCES && src_index == -1){
					alGetSourcei(sources[si], AL_SOURCE_STATE, &value);
					if(value != AL_PLAYING)
						src_index = si;
					++si;
				}
				// play sound if a source is available
				if(src_index > -1){
					alSourcei(sources[src_index], AL_BUFFER, buffers[soundnodes[i].sound]);
					alSourcef(sources[src_index], AL_REFERENCE_DISTANCE, reference_distance[soundnodes[i].sound]);
					alSourcefv(sources[src_index], AL_POSITION, soundnodes[i].pos);
					if(slowMotion)  // Slow down the sound
						alSourcef(sources[src_index], AL_PITCH, 0.5f);
					else  // Sound at regular speed
						alSourcef(sources[src_index], AL_PITCH, 1.0f);
					alSourcePlay(sources[src_index]);
				}
				// deactivate the SoundNode
				soundnodes[i].active = false;
			}
		}
	}
}
