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

#include "oalContext.h"
#include "oalSource.h"

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALContexts
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** OALContexts *****
OALContext::OALContext (const UInt32	inSelfToken, OALDevice    *inOALDevice)
	: 	mSelfToken (inSelfToken),
		mOwningDevice(inOALDevice),
		mSourceMap (NULL),
		mMixerUnit(0),
		mDistanceModel(AL_INVERSE_DISTANCE_CLAMPED),			
		mDopplerFactor(0.0),
		mDopplerVelocity(1.0),
		mListenerGain(1.0)
{
		// initialize  mContextInfo
		mListenerPosition[0] = 0.0;
		mListenerPosition[1] = 0.0;
		mListenerPosition[2] = 0.0;
		
		mListenerVelocity[0] = 0.0;
		mListenerVelocity[1] = 0.0;
		mListenerVelocity[2] = 0.0;
		
		mListenerOrientationForward[0] = 0.0;
		mListenerOrientationForward[1] = 0.0;
		mListenerOrientationForward[2] = -1.0;
		
		mListenerOrientationUp[0] = 0.0;
		mListenerOrientationUp[1] = 1.0;
		mListenerOrientationUp[2] = 0.0;
		
		// get the mixer AU of the device graph
		mMixerUnit = mOwningDevice->GetMixerUnit();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OALContext::~OALContext()
{
	// all the sources that are playing or paused must be stopped before tearing down the context
	mSourceMap->StopAllSources();
	
	// delete all the sources that were created by this context
	for (UInt32  i = 0; i < mSourceMap->Size(); i++)
	{
		OALSource	*oalSource = mSourceMap->GetSourceByIndex(0);
		mSourceMap->Remove(oalSource->GetToken());
		if (oalSource != NULL)
			delete oalSource;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::DisableContext()
{
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::AddSource(UInt32	inSourceToken)
{
	if (mSourceMap == NULL)
		mSourceMap = new OALSourceMap();

	try {
			OALSource	*newSource = new OALSource (inSourceToken, this);
	
			mSourceMap->Add(inSourceToken, &newSource);
	}
	catch (...) {
		throw;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OALSource*		OALContext::GetSource(UInt32	inSourceToken)
{
	return (mSourceMap->Get(inSourceToken));
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::RemoveSource(UInt32	inSourceToken)
{
	OALSource	*oalSource = mSourceMap->Get(inSourceToken);
	if (oalSource != NULL)
	{
		oalSource->Stop();
		mSourceMap->Remove(inSourceToken);
		delete(oalSource);	
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::ProcessContext()
{
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SuspendContext()
{
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SetDistanceModel(UInt32	inDistanceModel)
{
	if (mDistanceModel != inDistanceModel)
	{
		mDistanceModel = inDistanceModel;
		if (mSourceMap)
		{
#if LOG_GRAPH_AND_MIXER_CHANGES
	DebugMessage("OALContext::SetDistanceModel: MarkAllSourcesForRecalculation called\n");
#endif
            // the sources may have already been set and now need to be moved back to the reference position
             mSourceMap->MarkAllSourcesForRecalculation();
		}
	}
}	

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SetDopplerFactor(Float32		inDopplerFactor)
{
	if (mDopplerFactor != inDopplerFactor)
	{
		mDopplerFactor = inDopplerFactor;
		if (mSourceMap)
			mSourceMap->SetDopplerForAllSources();
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SetDopplerVelocity(Float32	inDopplerVelocity)
{
	mDopplerVelocity = inDopplerVelocity;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SetListenerGain(Float32	inGain)
{
	if (mListenerGain != inGain)
	{
		mListenerGain = inGain;
		
		Float32	db = 20.0 * log10(inGain); 				// convert to db
		AudioUnitSetParameter (mMixerUnit, k3DMixerParam_Gain, kAudioUnitScope_Output, 0, db, 0);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32		OALContext::GetSourceCount()
{
	if (mSourceMap)
		return (mSourceMap->Size());

	return (0);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SetListenerPosition(Float32	posX, Float32	posY, Float32	posZ) 
{ 
	mListenerPosition[0] = posX;
	mListenerPosition[1] = posY;
	mListenerPosition[2] = posZ;

	if (mSourceMap)
	{
#if LOG_GRAPH_AND_MIXER_CHANGES
	DebugMessageN4("OALContext::SetListenerPosition called - OALSource = %f:%f:%f/%ld\n", posX, posY, posZ, mSelfToken);
#endif
		// moving the listener effects the coordinate translation for ALL the sources
		mSourceMap->MarkAllSourcesForRecalculation();
	}	
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SetListenerVelocity(Float32	posX, Float32	posY, Float32	posZ) 
{ 
	mListenerVelocity[0] = posX;
	mListenerVelocity[1] = posY;
	mListenerVelocity[2] = posZ;

	if (mSourceMap)
	{
#if LOG_GRAPH_AND_MIXER_CHANGES
	DebugMessage("OALContext::SetListenerVelocity: MarkAllSourcesForRecalculation called\n");
#endif
		// moving the listener effects the coordinate translation for ALL the sources
        mSourceMap->MarkAllSourcesForRecalculation();
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALContext::SetListenerOrientation( Float32	forwardX, 	Float32	forwardY,	Float32	forwardZ,
											Float32	 upX, 		Float32	 upY, 		Float32	 upZ)
{	
	mListenerOrientationForward[0] = forwardX;
	mListenerOrientationForward[1] = forwardY;
	mListenerOrientationForward[2] = forwardZ;
	mListenerOrientationUp[0] = upX;
	mListenerOrientationUp[1] = upY;
	mListenerOrientationUp[2] = upZ;

	if (mSourceMap)
	{
#if LOG_GRAPH_AND_MIXER_CHANGES
	DebugMessage("OALContext::SetListenerOrientation: MarkAllSourcesForRecalculation called\n");
#endif
		// moving the listener effects the coordinate translation for ALL the sources
		mSourceMap->MarkAllSourcesForRecalculation();
	}
}
