/**********************************************************************************************************************************
*
*   OpenAL cross platform audio library
*   Copyright © 2004, Apple Computer, Inc. All rights reserved.
*
*   Redistribution and use in source and binary forms, with or without modification, are permitted provided 
*   that the following conditions are met:
*
*   1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
*   2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following 
*       disclaimer in the documentation and/or other materials provided with the distribution. 
*   3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of its contributors may be used to endorse or promote 
*       products derived from this software without specific prior written permission. 
*
*   THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
*   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS 
*   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED 
*   TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
*   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE 
*   USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
**********************************************************************************************************************************/

#ifndef __OAL_CONTEXT__
#define __OAL_CONTEXT__

#include "oalDevice.h"
#include <Carbon/Carbon.h>
#include <map>

/*
	An OALContext is basically the equivalent of a 'room' or 'scene'. It is attached to an OALDevice which
	is a piece of hardware and contains a single 3DMixer. Each context has it's own source objects
	and a single listener. It also has it's own settings for the 3DMixer, such as Reverb, Doppler, etc.
*/

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class OALSource;        // forward declaration
class OALSourceMap;     // forward declaration

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALContexts
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark _____OALContext_____

class OALContext
{
	public:
	OALContext(const UInt32	inSelfToken, OALDevice *inOALDevice);
	~OALContext();
	
	// get info methods
	OALDevice               *GetOwningDevice () { return mOwningDevice;}
	UInt32					GetDeviceToken () { return mOwningDevice->GetDeviceToken();}
	UInt32					GetDistanceModel() { return mDistanceModel;}
	Float32					GetDopplerFactor() {return (mDopplerFactor);}
	Float32					GetDopplerVelocity() {return (mDopplerVelocity);}
	Float32					GetListenerGain() {return (mListenerGain);}
	void					GetListenerPosition(Float32	*posX, Float32	*posY, Float32	*posZ) {*posX = mListenerPosition[0];
																								*posY = mListenerPosition[1];
																								*posZ = mListenerPosition[2];}
																								
	void					GetListenerVelocity(Float32	*posX, Float32	*posY, Float32	*posZ) {*posX = mListenerVelocity[0];
																								*posY = mListenerVelocity[1];
																								*posZ = mListenerVelocity[2];}
																								
	void					GetListenerOrientation(	Float32	*forwardX, Float32	*forwardY, Float32	*forwardZ,
													Float32	*upX, Float32	*upY, Float32	*upZ) {	*forwardX = mListenerOrientationForward[0];
																									*forwardY = mListenerOrientationForward[1];
																									*forwardZ = mListenerOrientationForward[2];
																									*upX = mListenerOrientationUp[0];
																									*upY = mListenerOrientationUp[1];
																									*upZ = mListenerOrientationUp[2];}
	
	// set info methods
	void					SetDistanceModel(UInt32	inDistanceModel);
	void					SetDopplerFactor(Float32	inDopplerFactor);
	void					SetDopplerVelocity(Float32	inDopplerVelocity);
	void					SetListenerPosition(Float32	posX, Float32	posY, Float32	posZ);
	void					SetListenerVelocity(Float32	posX, Float32	posY, Float32	posZ);
	void					SetListenerGain(Float32 inGain);
	void					SetListenerOrientation( Float32  forwardX,   Float32  forwardY, Float32  forwardZ,
													Float32  upX,        Float32  upY, 		Float32  upZ);
	
	// context activity methods
	void					ProcessContext();
	void					SuspendContext();

	// source methods
	void					AddSource(UInt32 inSourceToken);
	void					RemoveSource(UInt32 inSourceToken);
	OALSource*				GetSource(UInt32 inSourceToken);
	UInt32					GetSourceCount();
	void					DisableContext();
		
	private:
		UInt32				mSelfToken;
		OALDevice			*mOwningDevice;
		OALSourceMap		*mSourceMap;
		AudioUnit			mMixerUnit;
		UInt32				mDistanceModel;
		Float32				mDopplerFactor;
		Float32				mDopplerVelocity;
		Float32				mListenerPosition[3];
		Float32				mListenerVelocity[3];
		Float32				mListenerGain;
		Float32				mListenerOrientationForward[3];
		Float32				mListenerOrientationUp[3];
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALDevices contain a single OALContextMap to keep track of the contexts that belong to it.
#pragma mark _____OALContextMap_____
class OALContextMap : std::multimap<UInt32, OALContext*, std::less<UInt32> > {
public:
    
    void Add (const	UInt32	inContextToken, OALContext **inContext)  {
		iterator it = upper_bound(inContextToken);
		insert(it, value_type (inContextToken, *inContext));
	}

    OALContext* Get (UInt32	inContextToken) {
        iterator	it = find(inContextToken);
        if (it != end())
            return ((*it).second);
		return (NULL);
    }
    
    void Remove (const	UInt32	inContextToken) {
        iterator 	it = find(inContextToken);
        if (it != end())
            erase(it);
    }
	
	OALContext* GetContextByIndex(UInt32	inIndex, UInt32	&outContextToken) {
		iterator	it = begin();		
        std::advance(it, inIndex);		
		if (it != end())
		{
			outContextToken = (*it).first;
			return (*it).second;
		}
		return (NULL);
	}

	UInt32	GetDeviceTokenForContext(UInt32	inContextToken) {
		OALContext	*context = Get(inContextToken);
		if (context != NULL)
			return (context->GetDeviceToken());
		else
			return (0);
	}
    
    UInt32 Size() const { return size(); }
    bool Empty() const { return empty(); }
};

#endif