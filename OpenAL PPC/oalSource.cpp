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

/*
	Each OALSource object maintains a BufferQueue (list) and an ACMap (multimap). The buffer queue is an ordered list of BufferInfo structs.
	These structs contain an OAL buffer and other pertinent data. The ACMap is a multimap of ACInfo structs. These structs each contain an
	AudioConverter and the input format of the AudioConverter. The AudioConverters are created as needed each time a buffer with a new 
    format is added to the queue. This allows differently formatted data to be queued seemlessly. The correct AC is used for each 
    buffer as the BufferInfo keeps track of the appropriate AC to use.
*/

#include "oalSource.h"
#include "CADebugger.h"

#define		LOG_PLAYBACK				0
#define		LOG_VERBOSE					0
#define		LOG_BUFFER_QUEUEING			0

#define		CALCULATE_POSITION	1	// this should be true except for testing
#define		USE_MUTEXES			1	// this should be true except for testing
#define		WARM_THE_BUFFERS	1	// when playing, touch all the audio data in memory once before it is needed in the render proc  (RealTime thread)

// These are macros for locking around play/stop code:
#define TRY_PLAY_MUTEX										\
	bool wasLocked = false;									\
	try 													\
	{														\
		if (CallingInRenderThread()) {						\
			if (mPlayGuard.Try(wasLocked) == false)	{		\
				goto home;									\
            }												\
		} else {											\
			wasLocked = mPlayGuard.Lock();					\
		}
		

#define UNLOCK_PLAY_MUTEX									\
	home:;													\
        if (wasLocked)										\
			mPlayGuard.Unlock();							\
															\
	} catch (OSStatus stat) {								\
        if (wasLocked)										\
			mPlayGuard.Unlock();							\
        throw stat;                                         \
	} catch (...) {											\
        if (wasLocked)										\
			mPlayGuard.Unlock();							\
        throw -1;                                         	\
	}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// TEMPORARY
// This build flag should be on if you do not have a copy of AudioUnitProperties.h that
// defines the struct MixerDistanceParams and the constant kAudioUnitProperty_3DMixerDistanceParams

#define MIXER_PARAMS_UNDEFINED 0

#if MIXER_PARAMS_UNDEFINED
typedef struct MixerDistanceParams {
			Float32		mReferenceDistance;
			Float32		mMaxDistance;
			Float32		mMaxAttenuation;
} MixerDistanceParams;

enum {
	kAudioUnitProperty_3DMixerDistanceParams   = 3010
};
#endif
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALSource
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** OALSources *****
OALSource::OALSource (const UInt32 	 	inSelfToken, OALContext	*inOwningContext)
	: 	mSelfToken (inSelfToken),
		mOwningContext(inOwningContext),
		mOwningDevice(NULL),
		mCalculateDistance(true),
		mResetBusFormat(true),
        mResetPitch(true),
		mResetDoppler(true),	
		mBufferQueue(NULL),                     
		mCurrentBufferIndex(0),
		mQueueIsProcessed(true),
		mState(AL_INITIAL),
		mRenderThreadID (0),
		mPlayGuard ("OALAudioPlayer::Guard"),
		mCurrentPlayBus (kSourceNeedsBus),
		mShouldFade (false),
		mACMap(NULL),
		mPitch(1.0),                            // this is the user pitch setting changed via the OAL APIs
		mGain(1.0),
		mOutputSilence(false),
		mMaxDistance(kDefaultMaximumDistance),                // ***** should be MAX_FLOAT
		mMinDistance(1.0),
		mMinGain(0.0),
		mMaxGain(1.0),
		mRollOffFactor(kDefaultRolloff),
		mReferenceDistance(kDefaultReferenceDistance),
		mLooping(AL_FALSE),
		mSourceRelative(AL_FALSE),
		mConeInnerAngle(360.0),
		mConeOuterAngle(360.0),
		mConeOuterGain(0.0),
        mReadIndex(0.0),
        mTempSourceStorageBufferSize(2048)      // only used if preferred mixer is unavailable
{		

        mOwningDevice = mOwningContext->GetOwningDevice();

		mPosition[0] = 0.0;
		mPosition[1] = 0.0;
		mPosition[2] = 0.0;
		
        mVelocity[0] = 0.0;
		mVelocity[1] = 0.0;
		mVelocity[2] = 0.0;

		mDirection[0] = 0.0;
		mDirection[1] = 0.0;
		mDirection[2] = 0.0;

		mBufferQueue = new BufferQueue();
		mACMap = new ACMap();

        mReferenceDistance = mOwningDevice->GetDefaultReferenceDistance();
		mMaxDistance = mOwningDevice->GetDefaultMaxDistance();
         
        if (!mOwningDevice->IsPreferredMixerAvailable())
		{
			// since the preferred mixer is not available, some temporary storgae will be needed for SRC
            // for now assume that sources will not have more than 2 channels of data
			mTempSourceStorage = (AudioBufferList *) malloc ((offsetof(AudioBufferList, mBuffers)) + (2 * sizeof(AudioBuffer)));
			mTempSourceStorage->mBuffers[0].mDataByteSize = mTempSourceStorageBufferSize;
			mTempSourceStorage->mBuffers[0].mData = malloc(mTempSourceStorageBufferSize);
			
			mTempSourceStorage->mBuffers[1].mDataByteSize = mTempSourceStorageBufferSize;
			mTempSourceStorage->mBuffers[1].mData = malloc(mTempSourceStorageBufferSize);
		}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OALSource::~OALSource()
{
#if LOG_VERBOSE
	DebugMessageN1("OALSource::~OALSource() - OALSource = %ld\n", mSelfToken);
#endif

	Stop();
	if (mCurrentPlayBus != kSourceNeedsBus)
	{
		mOwningDevice->SetBusAsAvailable (mCurrentPlayBus);
		mCurrentPlayBus = kSourceNeedsBus;		
	}
	
	Reset();
	
	// empty the queue
	if (mBufferQueue)
	{
		RemoveBuffersFromQueue(mBufferQueue->Size(), NULL);				
		delete (mBufferQueue);
	}
	
	if (mACMap)
	{
		mACMap->RemoveAllConverters();
		delete (mACMap);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetPitch (float	inPitch)
{
    if ((inPitch == mPitch) && (mResetPitch == false))
		return;			// nothing to do
	
	mPitch = inPitch;

    if (!mOwningDevice->IsPreferredMixerAvailable())
        return;         // 1.3 3DMixer does not work properly when doing SRC on a mono bus

	if (mCurrentPlayBus != kSourceNeedsBus)
	{
		BufferInfo		*oalBuffer = mBufferQueue->Get(mCurrentBufferIndex);
		
		if (oalBuffer != NULL)
		{
#if LOG_GRAPH_AND_MIXER_CHANGES
	DebugMessageN1("OALSource::SetPitch: k3DMixerParam_PlaybackRate called - OALSource = %ld\n", mSelfToken);
#endif            
			
			OSStatus    result = AudioUnitSetParameter (	mOwningDevice->GetMixerUnit(), k3DMixerParam_PlaybackRate, kAudioUnitScope_Input, mCurrentPlayBus, mPitch, 0);
            if (result != noErr)
                DebugMessageN1("OALSource::SetPitch: k3DMixerParam_PlaybackRate failed - mPitch = %f2\n", mPitch);
        }

		mResetPitch = false;
	}
	else
		mResetPitch = true; // the source is not currently connected to a bus, so make this change when play is called
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetDoppler ()
{				
	if ((mCurrentPlayBus != kSourceNeedsBus) && (mResetDoppler == true))
	{
		UInt32		doppler = mOwningContext->GetDopplerFactor() > 0.0 ? 1 : 0;
#if LOG_GRAPH_AND_MIXER_CHANGES
	DebugMessageN1("OALSource::SetDoppler: kAudioUnitProperty_DopplerShift called - OALSource = %ld\n", mSelfToken);
#endif
		OSStatus	result = AudioUnitSetProperty(	mOwningDevice->GetMixerUnit(), kAudioUnitProperty_DopplerShift, kAudioUnitScope_Input, 
													mCurrentPlayBus, &doppler, sizeof(doppler));
			THROW_RESULT

		mResetDoppler = false;	
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetBusGain ()
{
	if (mCurrentPlayBus != kSourceNeedsBus)
	{
		Float32		busGain;
		
		// figure out which gain to actually use
		if (mMinGain > mGain)
			busGain = mMinGain;
		else if (mMaxGain < mGain)
			busGain = mMaxGain;
		else
			busGain = mGain;
		
		// clamp the gain used to 0.0-1.0
        if (busGain > 1.0)
			busGain = 1.0;
		else if (busGain < 0.0)
			busGain = 0.0;
	
		mOutputSilence = busGain > 0.0 ? false : true;

		if (busGain > 0.0)
		{
			Float32	db = 20.0 * log10(busGain); 	// convert to db
			if (db < -120.0)
				db = -120.0;						// clamp minimum audible level at -120db
			
#if LOG_GRAPH_AND_MIXER_CHANGES
	DebugMessageN2("OALSource::SetBusGain: k3DMixerParam_Gain called - OALSource = %ld, mBusGain = %f2\n", mSelfToken, busGain );
#endif
			OSStatus	result = AudioUnitSetParameter (	mOwningDevice->GetMixerUnit(), k3DMixerParam_Gain, kAudioUnitScope_Input, mCurrentPlayBus, db, 0);
				THROW_RESULT
		}
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetBusFormat ()
{
 	if (!mOwningDevice->IsPreferredMixerAvailable())	// the pre-2.0 3DMixer cannot change stream formats once initialized
		return;
		
    if (mResetBusFormat)
    {
        CAStreamBasicDescription    desc;
        UInt32  outSize = sizeof(desc);
        OSStatus result = AudioUnitGetProperty(mOwningDevice->GetMixerUnit(), kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, mCurrentPlayBus, &desc, &outSize);
        if (result == noErr)
        {
            BufferInfo	*buffer = mBufferQueue->Get(mCurrentBufferIndex);	
            if (buffer != NULL)
            {
                desc.mSampleRate = buffer->mBuffer->mDataFormat.mSampleRate;
                AudioUnitSetProperty(mOwningDevice->GetMixerUnit(), kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, mCurrentPlayBus, &desc, sizeof(desc));
                mResetBusFormat = false;
            }
        }
    }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetGain (float	inGain)
{	
#if LOG_VERBOSE
	DebugMessageN2("OALSource::SetGain - inGain/OALSource = %f/%ld\n", inGain, mSelfToken);
#endif
	if (mGain != inGain)
	{
		mGain = inGain;
		SetBusGain();
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetMinGain (Float32	inMinGain)
{
#if LOG_VERBOSE
	DebugMessageN1("OALSource::SetMinGain - inMinGain = %f\n", inMinGain);
#endif
	if (mMinGain != inMinGain)
	{
		mMinGain = inMinGain;
		SetBusGain();
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetMaxGain (Float32	inMaxGain)
{
#if LOG_VERBOSE
	DebugMessageN1("OALSource::SetMaxGain SetMaxGain - inMaxGain = %f\n", inMaxGain);
#endif
	if (mMaxGain != inMaxGain)
	{
		mMaxGain = inMaxGain;
		SetBusGain();
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetRollOffFactor (Float32	inRollOffFactor)
{
#if LOG_VERBOSE
	DebugMessageN2("OALSource::SetRollOffFactor - inRollOffFactor/OALSource = %f/%ld\n", inRollOffFactor, mSelfToken);
#endif
		        
	if (inRollOffFactor == GetRollOffFactor())
		return; // nothing to do

	mRollOffFactor = inRollOffFactor;
 	
    if (mCurrentPlayBus != kSourceNeedsBus)
    {
        if (!mOwningDevice->IsPreferredMixerAvailable())	
        {
            // the pre-2.0 3DMixer does not accept kAudioUnitProperty_3DMixerDistanceParams, it has do some extra work and use the DistanceAtten property instead
            mOwningDevice->SetDistanceAttenuation(mCurrentPlayBus, GetReferenceDistance(), GetMaxDistance(), GetRollOffFactor());
        }
        else
        {
			MixerDistanceParams		distanceParams;
			UInt32					outSize = sizeof(distanceParams);
			OSStatus	result = AudioUnitGetProperty(mOwningDevice->GetMixerUnit(), kAudioUnitProperty_3DMixerDistanceParams, kAudioUnitScope_Input, mCurrentPlayBus, &distanceParams, &outSize);
			if (result == noErr)
			{
                Float32     rollOff = GetRollOffFactor();

                if (mOwningDevice->IsDistanceScalingRequired())
                {
                    // scale the reference distance
                    distanceParams.mReferenceDistance = (GetReferenceDistance()/GetMaxDistance()) * kDistanceScalar;
                    // limit the max distance
                    distanceParams.mMaxDistance = kDistanceScalar;
                    // scale the rolloff
                    rollOff *= (kDistanceScalar/GetMaxDistance());
                }
				
                distanceParams.mMaxAttenuation = 20 * log10(distanceParams.mReferenceDistance / (distanceParams.mReferenceDistance + (rollOff * (distanceParams.mMaxDistance -  distanceParams.mReferenceDistance))));

				if (distanceParams.mMaxAttenuation < 0.0)
					distanceParams.mMaxAttenuation *= -1.0;
				else 
					distanceParams.mMaxAttenuation = 0.0;   // if db result was positive, clamp it to zero
				
                AudioUnitSetProperty(mOwningDevice->GetMixerUnit(), kAudioUnitProperty_3DMixerDistanceParams, kAudioUnitScope_Input, mCurrentPlayBus, &distanceParams, sizeof(distanceParams));
			}
        }
    }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetMaxDistance (Float32	inMaxDistance)
{
#if LOG_VERBOSE
	DebugMessageN2("OALSource::SetMaxDistance - inMaxDistance/OALSource = %f/%/d\n", inMaxDistance, mSelfToken);
#endif
			
	if (inMaxDistance == GetMaxDistance())
		return; // nothing to do

	mMaxDistance = inMaxDistance;

    if (mCurrentPlayBus != kSourceNeedsBus)
    {
        if (!mOwningDevice->IsPreferredMixerAvailable())	
        {
            // the pre-2.0 3DMixer does not accept kAudioUnitProperty_3DMixerDistanceParams, it has do some extra work and use the DistanceAtten property instead
            mOwningDevice->SetDistanceAttenuation(mCurrentPlayBus, GetReferenceDistance(), GetMaxDistance(), GetRollOffFactor());
        }
        else
        {
            MixerDistanceParams		distanceParams;
            UInt32					outSize = sizeof(distanceParams);
            OSStatus	result = AudioUnitGetProperty(mOwningDevice->GetMixerUnit(), kAudioUnitProperty_3DMixerDistanceParams, kAudioUnitScope_Input, mCurrentPlayBus, &distanceParams, &outSize);
            if (result == noErr)
            {
                Float32     rollOff = GetRollOffFactor();

                if (mOwningDevice->IsDistanceScalingRequired())
                {
                    // scale the reference distance
                    distanceParams.mReferenceDistance = (GetReferenceDistance()/GetMaxDistance()) * kDistanceScalar;
                    // limit the max distance
                    distanceParams.mMaxDistance = kDistanceScalar;
                    // scale the rolloff
                    rollOff *= (kDistanceScalar/GetMaxDistance());
                }
                else
                    distanceParams.mMaxDistance = GetMaxDistance();

                distanceParams.mMaxAttenuation = 20 * log10(distanceParams.mReferenceDistance / (distanceParams.mReferenceDistance + (rollOff * (distanceParams.mMaxDistance -  distanceParams.mReferenceDistance))));
                if (distanceParams.mMaxAttenuation < 0.0)
                    distanceParams.mMaxAttenuation *= -1.0;
                else 
                    distanceParams.mMaxAttenuation = 0.0;   // if db result was positive, clamp it to zero
                
                AudioUnitSetProperty(mOwningDevice->GetMixerUnit(), kAudioUnitProperty_3DMixerDistanceParams, kAudioUnitScope_Input, mCurrentPlayBus, &distanceParams, sizeof(distanceParams));
            }
        }
    }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetReferenceDistance (Float32	inReferenceDistance)
{
#if LOG_VERBOSE
	DebugMessageN2("OALSource::SetReferenceDistance - inReferenceDistance/OALSource = %f/%ld\n", inReferenceDistance, mSelfToken);
#endif
				
	if (inReferenceDistance == GetReferenceDistance())
		return; // nothing to do

	mReferenceDistance = inReferenceDistance;
 	
    if (mCurrentPlayBus != kSourceNeedsBus)
    {
        if (!mOwningDevice->IsPreferredMixerAvailable())	
        {
            // the pre-2.0 3DMixer does not accept kAudioUnitProperty_3DMixerDistanceParams, it has do some extra work and use the DistanceAtten property instead
            mOwningDevice->SetDistanceAttenuation(mCurrentPlayBus, GetReferenceDistance(), GetMaxDistance(), GetRollOffFactor());
        }
        else
        {
            MixerDistanceParams		distanceParams;
            UInt32					outSize = sizeof(distanceParams);
            OSStatus	result = AudioUnitGetProperty(mOwningDevice->GetMixerUnit(), kAudioUnitProperty_3DMixerDistanceParams, kAudioUnitScope_Input, mCurrentPlayBus, &distanceParams, &outSize);
            if (result == noErr)
            {
                Float32     rollOff = GetRollOffFactor();

                if (mOwningDevice->IsDistanceScalingRequired())
                {
                    // scale the reference distance
                    distanceParams.mReferenceDistance = (GetReferenceDistance()/GetMaxDistance()) * kDistanceScalar;
                    // limit the max distance
                    distanceParams.mMaxDistance = kDistanceScalar;
                    // scale the rolloff
                    rollOff *= (kDistanceScalar/GetMaxDistance());
                }
                else
                    distanceParams.mReferenceDistance = GetReferenceDistance();

                distanceParams.mMaxAttenuation = 20 * log10(distanceParams.mReferenceDistance / (distanceParams.mReferenceDistance + (rollOff * (distanceParams.mMaxDistance -  distanceParams.mReferenceDistance))));
                if (distanceParams.mMaxAttenuation < 0.0)
                    distanceParams.mMaxAttenuation *= -1.0;
                else 
                    distanceParams.mMaxAttenuation = 0.0;   // if db result was positive, clamp it to zero
                
                AudioUnitSetProperty(mOwningDevice->GetMixerUnit(), kAudioUnitProperty_3DMixerDistanceParams, kAudioUnitScope_Input, mCurrentPlayBus, &distanceParams, sizeof(distanceParams));
            }
        }
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetPosition (Float32 inX, Float32 inY, Float32 inZ)
{
#if LOG_VERBOSE
	DebugMessageN4("OALSource::SetPosition called - OALSource = %f:%f:%f/%ld\n", inX, inY, inZ, mSelfToken);
#endif
	mPosition[0] = inX;
	mPosition[1] = inY;
	mPosition[2] = inZ;

	mCalculateDistance = true;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::GetPosition (Float32 &inX, Float32 &inY, Float32 &inZ)
{
	inX= mPosition[0];
	inY = mPosition[1];
	inZ = mPosition[2];
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetVelocity (Float32 inX, Float32 inY, Float32 inZ)
{
#if LOG_VERBOSE
	DebugMessageN1("OALSource::SetVelocity called - OALSource = %ld\n", mSelfToken);
#endif
	mVelocity[0] = inX;
	mVelocity[1] = inY;
	mVelocity[2] = inZ;

	mCalculateDistance = true;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::GetVelocity (Float32 &inX, Float32 &inY, Float32 &inZ)
{
	inX = mVelocity[0];
	inY = mVelocity[1];
	inZ = mVelocity[2];
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetDirection (Float32 inX, Float32 inY, Float32 inZ)
{
#if LOG_VERBOSE
	DebugMessageN1("OALSource::SetDirection called - OALSource = %ld\n", mSelfToken);
#endif
	mDirection[0] = inX;
	mDirection[1] = inY;
	mDirection[2] = inZ;

	mCalculateDistance = true;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::GetDirection (Float32 &inX, Float32 &inY, Float32 &inZ)
{
	inX = mDirection[0];
	inY = mDirection[1];
	inZ = mDirection[2];
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetBuffer (UInt32 inBufferToken, OALBuffer	*inBuffer)
{
	// empty the queue and start over
	RemoveBuffersFromQueue(mBufferQueue->Size(), NULL);
	
	// if inBufferToken == 0, do nothing, passing NONE to this method is legal and should merely empty the queue
	if (inBufferToken != 0)
	{
		AppendBufferToQueue(inBufferToken, inBuffer);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	OALSource::GetBuffer ()
{
	// for now, return the currently playing buffer in an active source, or the 1st buffer of the Q in an inactive source
	return (mBufferQueue->GetBufferTokenByIndex(mCurrentBufferIndex));
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::SetSourceRelative (UInt32	inSourceRelative)
{
#if LOG_VERBOSE
	DebugMessageN1("OALSource::SetSourceRelative called - OALSource = %ld\n", mSelfToken);
#endif
	mSourceRelative = inSourceRelative;
	mCalculateDistance = true;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	OALSource::BuffersProcessed()
{
#if LOG_BUFFER_QUEUEING
	DebugMessageN1("OALSource::BuffersProcessed called - OALSource = %ld\n", mSelfToken);
#endif
	return(mBufferQueue->ProcessedBufferCount());
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	OALSource::GetQLength()
{
#if LOG_BUFFER_QUEUEING
	DebugMessageN1("OALSource::GetQLength called - OALSource = %ld\n", mSelfToken);
#endif
	return (mBufferQueue->Size());
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::AppendBufferToQueue(UInt32	inBufferToken, OALBuffer	*inBuffer)
{
#if LOG_BUFFER_QUEUEING
	DebugMessageN2("OALSource::AppendBufferToQueue called - OALSource = %ld, inBufferToken = %ld\n", this, inBufferToken);
#endif

	OSStatus	result = noErr;	
	try {
		if (mBufferQueue->Empty())
		{
			mCurrentBufferIndex = 0;	// this is the first buffer in the queue
			mQueueIsProcessed = false;
		}
								
		// do we need an AC for the format of this buffer?
		if (inBuffer->mDataHasBeenConverted)
		{
			// the data was already convertered to the mixer format, no AC is necessary (as indicated by the ACToken setting of zero)
			mBufferQueue->AppendBuffer(inBufferToken, inBuffer, 0);
		}
		else
		{			
			// check the format against the real format of the data, NOT the input format of the converter which may be subtly different
			// both in SampleRate and Interleaved flags
			UInt32		outToken = 0;
			mACMap->GetACForFormat(inBuffer->mDataFormat, outToken);
			if (outToken == 0)
			{
				// create an AudioConverter for this format because there isn't one yet
				AudioConverterRef				converter = 0;
				CAStreamBasicDescription		inFormat;
				
				inFormat.SetFrom(inBuffer->mDataFormat);
					
				// if the source is mono, set the flags to be non interleaved, so a reinterleaver does not get setup when
				// completely unnecessary - since the flags on output are always set to non interleaved
				if (inFormat.NumberChannels() == 1)
					inFormat.mFormatFlags |= kAudioFormatFlagIsNonInterleaved; 
						
				// output should have actual number of channels, but frame/packet size is for single channel
				// this is to make de interleaved data work correctly with > 1 channel
				CAStreamBasicDescription 	outFormat;
				outFormat.mChannelsPerFrame = inFormat.NumberChannels();
				outFormat.mSampleRate = inFormat.mSampleRate;
				outFormat.mFormatID = kAudioFormatLinearPCM;
				outFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
				outFormat.mBytesPerPacket = sizeof (Float32);	
				outFormat.mFramesPerPacket = 1;	
				outFormat.mBytesPerFrame = sizeof (Float32);
				outFormat.mBitsPerChannel = sizeof (Float32) * 8;	
	
				result = AudioConverterNew(&inFormat, &outFormat, &converter);
					THROW_RESULT
				
				ACInfo	info;
				info.mInputFormat = inBuffer->mDataFormat;
				info.mConverter = converter;
				
				// add this AudioConverter to the source's ACMap
				UInt32	newACToken = GetNewToken();
				mACMap->Add(newACToken, &info);
				// add the buffer to the queue - each buffer now knows which AC to use when it is converted in the render proc
				mBufferQueue->AppendBuffer(inBufferToken, inBuffer, newACToken);
			}
			else
			{
				// there is already an AC for this buffer's data format, so just append the buffer to the queue
				mBufferQueue->AppendBuffer(inBufferToken, inBuffer, outToken);
			}
		}
	}
	catch (OSStatus	 result) {
		DebugMessageN1("APPEND BUFFER FAILED %ld\n", mSelfToken);
		throw (result);
	}

	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::RemoveBuffersFromQueue(UInt32	inCount, UInt32	*outBufferTokens)
{
	if ((inCount == 0) || mBufferQueue->Empty())
		return;

#if LOG_BUFFER_QUEUEING
	DebugMessageN2("OALSource::RemoveBuffersFromQueue called - OALSource = %ld, inCount = %ld\n", mSelfToken, inCount);
#endif
	
	try {
		// 1) do nothing if queue is playing and in loop mode - to risky that a requested unqueue buffer will be needed
		// 2) do nothing if any of the requested buffers is in progress or not processed and queue is PLAYING
		// 3) if source state is not playing or paused, do the request if the buffer count is valid
		
		if ((mState == AL_PLAYING) || (mState == AL_PAUSED))
		{			
			if (mLooping == true)
			{
				// case #1
				throw ((OSStatus) AL_INVALID_OPERATION);		
			}
			
			for (UInt32	i = 0; i < inCount; i++)
			{
				// case #2
				if (mBufferQueue->IsBufferProcessed(i) != true)
				{
					DebugMessageN1("OALSource::RemoveBuffersFromQueue() - Trying to Unqueue in progress\n", this);
					throw ((OSStatus) AL_INVALID_OPERATION);		
				}
			}
		}
			
		for (UInt32	i = 0; i < inCount; i++)
		{	
			UInt32		outToken = mBufferQueue->RemoveQueueEntryByIndex(0);
								
			if (outBufferTokens)
				outBufferTokens[i] = outToken;
			
			mCurrentBufferIndex--;	// the total buffers in the queue just went down, so the index for the next buffer to play will be wrong without this
		}
	}
	catch (OSStatus	 result) {
		DebugMessageN1("REMOVE BUFFER FAILED, OALSource = %ld\n", mSelfToken);
		throw (result);
	}

	if (mBufferQueue->Empty())
	{
		mCurrentBufferIndex = 0;
		mQueueIsProcessed = true;
	}

	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::ChangeChannelSettings()
{
#if CALCULATE_POSITION

	if (mCalculateDistance == true)
	{
        BufferInfo	*bufferInfo = mBufferQueue->Get(mCurrentBufferIndex);	
		if (bufferInfo)
		{		
#if LOG_GRAPH_AND_MIXER_CHANGES
	DebugMessageN1("OALSource::ChangeChannelSettings: k3DMixerParam_Azimuth/k3DMixerParam_Distance called - OALSource = %ld\n", mSelfToken);
#endif
			// only calculate position if sound is mono - stereo sounds get no location changes
			if ( bufferInfo->mBuffer->mDataFormat.NumberChannels() == 1)
			{
				Float32 	rel_azimuth, rel_distance;
				
				CalculateDistanceAndAzimuth(&rel_distance, &rel_azimuth);
				if (isnan(rel_azimuth))
				{
					rel_azimuth = 0.0;	// DO NOT pass a NAN to the mixer for azimuth
					DebugMessage("ERROR: OALSOurce::ChangeChannelSettings - CalculateDistanceAndAzimuth returned a NAN for rel_azimuth\n");
				}
				
				AudioUnitSetParameter(mOwningDevice->GetMixerUnit(), k3DMixerParam_Azimuth, kAudioUnitScope_Input, mCurrentPlayBus, rel_azimuth, 0);
				
                // do not change the distance if the user has set the distance model to AL_NONE
                if (mOwningContext->GetDistanceModel() != AL_NONE)
                {
                    if (mOwningDevice->IsDistanceScalingRequired())
                        rel_distance *= (kDistanceScalar/GetMaxDistance());
                }
                else
                    rel_distance = GetReferenceDistance() * (kDistanceScalar/GetMaxDistance());
                                        
                AudioUnitSetParameter(mOwningDevice->GetMixerUnit(), k3DMixerParam_Distance, kAudioUnitScope_Input, mCurrentPlayBus, rel_distance, 0);
			}
		}
		
		mCalculateDistance = false;
	}

	SetDoppler ();
		
#endif	// end CALCULATE_POSITION

	SetPitch (mPitch);
    SetBusFormat();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::Reset()
{
	RewindToStart();
	mState = AL_INITIAL;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::RewindToStart()
{	
    //mShouldFade = true;
	mBufferQueue->ResetBuffers(); 	// mark all the buffers as unprocessed
	mCurrentBufferIndex = 0;		// start at the first buffer in the queue
	mQueueIsProcessed = false;					
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::Play()
{
    UInt32  outSize;
    
	if (!mOwningDevice->IsPlayable())
		return;
		
	try {

#if LOG_PLAYBACK
		DebugMessageN3("OALSource::Play called - ID = %ld, QSize = %ld, Looping = %ld\n", mSelfToken, mBufferQueue->Size(), mLooping);
#endif
	
		OSStatus result = noErr;
		if (mBufferQueue->Empty())
			return; // there isn't anything to do
		
		// ******* Get the format for the first buffer in the queue
		mCurrentBufferIndex = 0;
		
		// find the first real buffer in the queue because it is legal to start the queue with an AL_NONE buffer
		while (mBufferQueue->GetBufferTokenByIndex(mCurrentBufferIndex) == AL_NONE)
		{
			// mark buffer as processed
			mBufferQueue->SetBufferAsProcessed(mCurrentBufferIndex);
			mCurrentBufferIndex++;
		}
	
		BufferInfo		*buffer = mBufferQueue->Get(mCurrentBufferIndex);
        if (buffer == NULL)
            throw AL_INVALID_OPERATION;
            
#if LOG_PLAYBACK
			DebugMessage("OALSource::Play called - Format of 1st buffer in the Q = \n");
			buffer->mBuffer->mDataFormat.Print();
#endif
	
#if WARM_THE_BUFFERS
		// touch the memory now instead of in the render proc, so audio data will be paged in if it's currently in VM
		volatile UInt32	X;
		UInt32		*start = (UInt32 *)buffer->mBuffer->mData;
		UInt32		*end = (UInt32 *)(buffer->mBuffer->mData + (buffer->mBuffer->mDataSize &0xFFFFFFFC));
		while (start < end)
		{
			X = *start; 
			start += 1024;
		}		
#endif
		
		if (mCurrentPlayBus == kSourceNeedsBus)
		{
			// the bus stream format will get set if necessary while getting the available bus
            mCurrentPlayBus = (buffer->mBuffer->mDataFormat.NumberChannels() == 1) ? mOwningDevice->GetAvailableMonoBus() : mOwningDevice->GetAvailableStereoBus();                
            
			Float32     rollOff = GetRollOffFactor();
			Float32     refDistance = GetReferenceDistance();
			Float32     maxDistance = GetMaxDistance();

            if (mOwningDevice->IsDistanceScalingRequired())
            {
                refDistance = (GetReferenceDistance()/GetMaxDistance()) * kDistanceScalar;
                maxDistance = kDistanceScalar;
                rollOff *= (kDistanceScalar/GetMaxDistance());
            }
            
            Float32	testAttenuation  = 20 * log10(GetReferenceDistance() / (GetReferenceDistance() + (GetRollOffFactor() * (GetMaxDistance() -  GetReferenceDistance()))));
            if (testAttenuation < 0.0)
                testAttenuation *= -1.0;
            else 
                testAttenuation = 0.0;   // if db result was positive, clamp it to zero

			if (mOwningDevice->IsPreferredMixerAvailable())
			{
				// Set the MixerDistanceParams for the new bus if necessary
				MixerDistanceParams		distanceParams;
				outSize = sizeof(distanceParams);
				result = AudioUnitGetProperty(mOwningDevice->GetMixerUnit(), kAudioUnitProperty_3DMixerDistanceParams, kAudioUnitScope_Input, mCurrentPlayBus, &distanceParams, &outSize);

				if  ((result == noErr) 	&& ((distanceParams.mReferenceDistance != refDistance)      ||
											(distanceParams.mMaxDistance != maxDistance)            ||
											(distanceParams.mMaxAttenuation != testAttenuation)))

				{
					distanceParams.mMaxAttenuation = testAttenuation;
                
                    if (mOwningDevice->IsDistanceScalingRequired())
                    {

                        distanceParams.mReferenceDistance = (GetReferenceDistance()/GetMaxDistance()) * kDistanceScalar;
                        // limit the max distance
                        distanceParams.mMaxDistance = kDistanceScalar;
                        distanceParams.mMaxAttenuation = testAttenuation;
                    }
                    else
                    {
                        distanceParams.mReferenceDistance = GetReferenceDistance();
                        distanceParams.mMaxDistance = GetMaxDistance();
                    }
					
 					AudioUnitSetProperty(mOwningDevice->GetMixerUnit(), kAudioUnitProperty_3DMixerDistanceParams, kAudioUnitScope_Input, mCurrentPlayBus, &distanceParams, sizeof(distanceParams));
				}
			}
            else
            {
                // the pre-2.0 3DMixer does not accept kAudioUnitProperty_3DMixerDistanceParams, it has do some extra work and use the DistanceAtten property instead
                mOwningDevice->SetDistanceAttenuation(mCurrentPlayBus, GetReferenceDistance(), GetMaxDistance(), GetRollOffFactor());
            }
		}	

        // get the sample rate of the bus
        CAStreamBasicDescription    desc;
        outSize = sizeof(desc);
        result = AudioUnitGetProperty(mOwningDevice->GetMixerUnit(), kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, mCurrentPlayBus, &desc, &outSize);
        
		mResetPitch = true;	
		mCalculateDistance = true;				
        if (desc.mSampleRate != buffer->mBuffer->mDataFormat.mSampleRate)
            mResetBusFormat = true;     // only reset the bus stream format if it is different than sample rate of the data   		
		
		// *************************** Set properties for the mixer bus
		
#if USE_MUTEXES
		TRY_PLAY_MUTEX
#endif												
		// reset some of the class members before we start playing
		Reset();			
					
		ChangeChannelSettings();
		
		SetBusGain();			

		mShouldFade = false;
		mState = AL_PLAYING;
		mQueueIsProcessed = false;

		// set the render callback for the player AU
		mPlayCallback.inputProc = SourceInputProc;
		mPlayCallback.inputProcRefCon = this;
		result = AudioUnitSetProperty (	mOwningDevice->GetMixerUnit(), kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 
										mCurrentPlayBus, &mPlayCallback, sizeof(mPlayCallback));
			THROW_RESULT
		
		result = AudioUnitAddRenderNotify(mOwningDevice->GetMixerUnit(), SourceNotificationProc, this);
			THROW_RESULT	
						
#if USE_MUTEXES
			UNLOCK_PLAY_MUTEX	
#endif
	}
	catch (OSStatus	result) {
		DebugMessageN2("PLAY FAILED source = %ld, err = %ld\n", mSelfToken, result);
		throw (result);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::Pause()
{
#if LOG_PLAYBACK
	DebugMessageN1("OALSource::Pause called - OALSource = %ld\n", mSelfToken);
#endif
#if USE_MUTEXES
	TRY_PLAY_MUTEX
#endif
	if (mCurrentPlayBus != kSourceNeedsBus)
	{            		
		AudioUnitRemoveRenderNotify(mOwningDevice->GetMixerUnit(), SourceNotificationProc,this);
					
		mPlayCallback.inputProc = 0;
		mPlayCallback.inputProcRefCon = 0;
		AudioUnitSetProperty (	mOwningDevice->GetMixerUnit(), kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 
								mCurrentPlayBus, &mPlayCallback, sizeof(mPlayCallback));

		mState = AL_PAUSED;
	}
	
#if USE_MUTEXES
	UNLOCK_PLAY_MUTEX
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::Resume()
{
#if LOG_PLAYBACK
	DebugMessageN1("OALSource::Resume called - OALSource = %ld\n", mSelfToken);
#endif

#if USE_MUTEXES
	TRY_PLAY_MUTEX
#endif
	
	if (mState = AL_PAUSED)
	{			
		OSStatus result = noErr;

		mPlayCallback.inputProc = SourceInputProc;
		mPlayCallback.inputProcRefCon = this;
		// set the render callback for the player AU
		result = AudioUnitSetProperty (	mOwningDevice->GetMixerUnit(), kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 
										mCurrentPlayBus, &mPlayCallback, sizeof(mPlayCallback));
			THROW_RESULT
		
		result = AudioUnitAddRenderNotify(mOwningDevice->GetMixerUnit(), SourceNotificationProc, this);
			THROW_RESULT	
				
		mState = AL_PLAYING;
	} 

#if USE_MUTEXES
	UNLOCK_PLAY_MUTEX
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::Stop()
{
#if LOG_PLAYBACK
	DebugMessageN1("OALSource::Stop called - OALSource = %ld\n", mSelfToken);
#endif

#if USE_MUTEXES
	TRY_PLAY_MUTEX
#endif
	if ((mCurrentPlayBus != kSourceNeedsBus) && (mState != AL_STOPPED))
	{            
		mState = AL_STOPPED;

		AudioUnitRemoveRenderNotify(mOwningDevice->GetMixerUnit(), SourceNotificationProc,this);
					
#if LOG_GRAPH_AND_MIXER_CHANGES
	DebugMessageN1("OALSource::Stop: set kAudioUnitProperty_SetRenderCallback (removing) called - OALSource = %ld\n", mSelfToken);
#endif
		mPlayCallback.inputProc = 0;
		mPlayCallback.inputProcRefCon = 0;
		AudioUnitSetProperty (	mOwningDevice->GetMixerUnit(), kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 
								mCurrentPlayBus, &mPlayCallback, sizeof(mPlayCallback));
							
		mOwningDevice->SetBusAsAvailable (mCurrentPlayBus);
		mCurrentPlayBus = kSourceNeedsBus;		
	}
	
#if USE_MUTEXES
	UNLOCK_PLAY_MUTEX
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALSource::PlayFinished()
{
#if LOG_PLAYBACK
	DebugMessageN1("OALSource::PlayFinished called - OALSource = %ld\n", mSelfToken);
#endif
#if USE_MUTEXES
	TRY_PLAY_MUTEX
#endif
	if (mCurrentPlayBus != kSourceNeedsBus)
	{                        	
		AudioUnitRemoveRenderNotify(mOwningDevice->GetMixerUnit(), SourceNotificationProc,this);
					
		mPlayCallback.inputProc = 0;
		mPlayCallback.inputProcRefCon = 0;
#if LOG_GRAPH_AND_MIXER_CHANGES
	DebugMessageN1("OALSource::Stop: set kAudioUnitProperty_SetRenderCallback (removing) called - OALSource = %ld\n", mSelfToken);
#endif
		AudioUnitSetProperty (	mOwningDevice->GetMixerUnit(), kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 
								mCurrentPlayBus, &mPlayCallback, sizeof(mPlayCallback));

		mOwningDevice->SetBusAsAvailable (mCurrentPlayBus);
		mCurrentPlayBus = kSourceNeedsBus;		
	}

#if USE_MUTEXES
	UNLOCK_PLAY_MUTEX
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus	OALSource::SourceNotificationProc (	void 						*inRefCon, 
												AudioUnitRenderActionFlags 	*inActionFlags,
												const AudioTimeStamp 		*inTimeStamp, 
												UInt32 						inBusNumber,
												UInt32 						inNumberFrames, 
												AudioBufferList 			*ioData)
{
	OALSource* THIS = (OALSource*)inRefCon;
	
	if (*inActionFlags & kAudioUnitRenderAction_PreRender)
		return THIS->DoPreRender();
		
	if (*inActionFlags & kAudioUnitRenderAction_PostRender)
		return THIS->DoPostRender();
		
	return (noErr);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#if USE_AU_TRACER
static UInt32 tracerStart = 0xca3d0000;
static UInt32 tracerEnd = 0xca3d0004;
#include <sys/syscall.h>
#include <unistd.h>
#endif
OSStatus	OALSource::SourceInputProc (	void 						*inRefCon, 
											AudioUnitRenderActionFlags 	*inActionFlags,
											const AudioTimeStamp 		*inTimeStamp, 
											UInt32 						inBusNumber,
											UInt32 						inNumberFrames, 
											AudioBufferList 			*ioData)
{
	OALSource* THIS = (OALSource*)inRefCon;
		
	if (THIS->mOutputSilence)
		*inActionFlags |= kAudioUnitRenderAction_OutputIsSilence;	
	else
		*inActionFlags &= 0xEF; // the mask for the kAudioUnitRenderAction_OutputIsSilence bit
		
#if USE_AU_TRACER
	syscall(180, tracerStart, inBusNumber, ioData->mNumberBuffers, 0, 0);
#endif

	OSStatus result = noErr;

    if (THIS->mOwningDevice->IsPreferredMixerAvailable())
        result = THIS->DoRender (ioData);       // normal case
    else
        result = THIS->DoSRCRender (ioData);    // pre 2.0 mixer case

#if USE_AU_TRACER
	syscall(180, tracerEnd, inBusNumber, ioData->mNumberBuffers, 0, 0);
#endif

	return (result);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus OALSource::DoPreRender ()
{
	BufferInfo	*bufferInfo = mBufferQueue->Get(mCurrentBufferIndex);
	if (bufferInfo == NULL)
		return -1;	// there are no buffers

	mRenderThreadID = (int) pthread_self();
		
	// there is a need to accomodate the spec which allows a 'NONE' buffer to be queued. In our case, just move on to the
	// next buffer in the queue until a non-NONE buffer is found
	while (bufferInfo->mBufferToken == AL_NONE)
	{
		mCurrentBufferIndex++;
		bufferInfo->mProcessedState = kProcessed;

		bufferInfo = mBufferQueue->Get(mCurrentBufferIndex);
		if (bufferInfo == NULL)
			return -1; // there are no more real buffers
	}

	ChangeChannelSettings();
				
	if (mShouldFade) 
		return MuteCurrentPlayBus();
	
	return (noErr);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus OALSource::ACComplexInputDataProc	(	AudioConverterRef				inAudioConverter,
												UInt32							*ioNumberDataPackets,
												AudioBufferList					*ioData,
												AudioStreamPacketDescription	**outDataPacketDescription,
												void*							inUserData)
{
	OSStatus		err = noErr;
	OALSource* 		THIS = (OALSource*)inUserData;
    BufferInfo		*bufferInfo = THIS->mBufferQueue->Get(THIS->mCurrentBufferIndex);
    UInt32			sourcePacketsLeft = (bufferInfo->mBuffer->mDataSize - bufferInfo->mOffset) / bufferInfo->mBuffer->mDataFormat.mBytesPerPacket;

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// BUFFER EMPTY: If the buffer is empty now, decide on returning an error based on what gets played next in the queue
	if (sourcePacketsLeft == 0)
	{
		if(!THIS->mLooping)
			THIS->mBufferQueue->SetBufferAsProcessed(THIS->mCurrentBufferIndex);

		bufferInfo->mProcessedState = kProcessed;	// this buffer is done
		bufferInfo->mOffset = 0;					// will be ready for the next time

		// see if there is a next buffer or if the queue is looping and should return to the start
		BufferInfo	*nextBufferInfo = THIS->mBufferQueue->Get(THIS->mCurrentBufferIndex + 1);
		if ((nextBufferInfo != NULL) || (THIS->mLooping == true))
		{
			if (nextBufferInfo != NULL)
			{
				THIS->mCurrentBufferIndex++;
			}
			else
			{
				THIS->RewindToStart();
				// see if the 1st buffer in the queue is the same format, return an error if so, so we get called again
				nextBufferInfo = THIS->mBufferQueue->Get(THIS->mCurrentBufferIndex);
			}
			
			if (nextBufferInfo->mBuffer->mDataFormat.mFormatID == bufferInfo->mBuffer->mDataFormat.mFormatID)
				err = OALSourceError_CallConverterAgain;

			if (nextBufferInfo->mBuffer->mDataFormat.mSampleRate != bufferInfo->mBuffer->mDataFormat.mSampleRate)
            {
  				THIS->mResetBusFormat = true;
          	}
            
		}
		else
		{
			// looping is false and there are no more buffers so we are really out of data
			// return what we have and no error, the AC should then be reset in the RenderProc
			// and what ever data is in the AC should get returned
			THIS->mBufferQueue->SetBufferAsProcessed(THIS->mCurrentBufferIndex);
			THIS->mQueueIsProcessed = true;		// we are done now, the Q is dry
		}

		ioData->mBuffers[0].mData = NULL;				// return nothing			
		ioData->mBuffers[0].mDataByteSize = 0;			// return nothing
	}
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// BUFFER HAS DATA
	else
	{
		// return the entire request or the remainder of the buffer
		if (sourcePacketsLeft < *ioNumberDataPackets)
			*ioNumberDataPackets = sourcePacketsLeft;
		
		ioData->mBuffers[0].mData = bufferInfo->mBuffer->mData + bufferInfo->mOffset;	// point to the data we are providing		
		ioData->mBuffers[0].mDataByteSize = *ioNumberDataPackets * bufferInfo->mBuffer->mDataFormat.mBytesPerPacket;
		bufferInfo->mOffset += ioData->mBuffers[0].mDataByteSize;
	}

	return (err);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus OALSource::DoRender (AudioBufferList 			*ioData)
{
	OSStatus   			err = noErr;
	UInt32				packetsRequestedFromRenderProc = ioData->mBuffers[0].mDataByteSize / sizeof(Float32);
	UInt32				packetsObtained = 0;
	UInt32				packetCount;
	AudioBufferList		*tempBufferList = ioData;
	UInt32				dataByteSize = ioData->mBuffers[0].mDataByteSize;
	void				*dataStarts[2];

	// save the data ptrs to restore later
	for (UInt32	i = 0; i < tempBufferList->mNumberBuffers; i++)
	{
		dataStarts[i] = tempBufferList->mBuffers[i].mData;
	}
		
	// walk through as many buffers as needed to satisfy the request AudioConverterFillComplexBuffer will
	// get called each time the data format changes until enough packets have been obtained or the q is empty.
	while ((packetsObtained < packetsRequestedFromRenderProc) && (mQueueIsProcessed == false))
	{
		BufferInfo	*bufferInfo = mBufferQueue->Get(mCurrentBufferIndex);
		if (bufferInfo == NULL)
			return -1;

		bufferInfo->mProcessedState = kInProgress;

		for (UInt32	i = 0; i < tempBufferList->mNumberBuffers; i++)
		{
			tempBufferList->mBuffers[i].mDataByteSize = dataByteSize - (packetsObtained * sizeof(Float32));
			tempBufferList->mBuffers[i].mData = (Byte *) ioData->mBuffers[i].mData + (packetsObtained * sizeof(Float32));
		}		
		
		if (bufferInfo->mBuffer->mDataHasBeenConverted == false)
		{
			// CONVERT THE BUFFER DATA 
			AudioConverterRef	converter = mACMap->Get(bufferInfo->mACToken);
	
			packetCount = packetsRequestedFromRenderProc - packetsObtained;
			// if OALSourceError_CallConverterAgain is returned, there is nothing to do, just go around again and try and get the data
			err = AudioConverterFillComplexBuffer(converter, ACComplexInputDataProc, this, &packetCount, tempBufferList, NULL);
			packetsObtained += packetCount;
	
			if ((packetsObtained < packetsRequestedFromRenderProc) && (err == noErr))
			{
				// we didn't get back what we asked for, but no error implies we have used up the data of this format
				// so reset this converter so it will be ready for the next time
				AudioConverterReset(converter);
#if	LOG_VERBOSE
                DebugMessageN1("OALSource::DoRender: Resetting AudioConverter - OALSource = %ld\n", mSelfToken);
#endif
            }
        }
		else
		{
			// Data has already been converted to the mixer's format, so just do a copy (should be mono only)
			UInt32	bytesRemaining = bufferInfo->mBuffer->mDataSize - bufferInfo->mOffset;
			UInt32	framesRemaining = bytesRemaining / sizeof(Float32);
			UInt32	bytesToCopy = 0;
			UInt32	framesToCopy = packetsRequestedFromRenderProc - packetsObtained;
			
			if (framesRemaining < framesToCopy)
				framesToCopy = framesRemaining;
			
			bytesToCopy = framesToCopy * sizeof(Float32);
			memcpy(tempBufferList->mBuffers->mData, bufferInfo->mBuffer->mData + bufferInfo->mOffset, bytesToCopy);
			bufferInfo->mOffset += bytesToCopy;
			packetsObtained += framesToCopy;
						
			// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			// this block of code is the same as that found in the fill proc - 
			// it is for determining what to do when a buffer runs out of data
			if (bufferInfo->mOffset == bufferInfo->mBuffer->mDataSize)
			{
				if (!mLooping)
					mBufferQueue->SetBufferAsProcessed(mCurrentBufferIndex);

				// see if there is a next buffer or if the queue is looping and should return to the start
				BufferInfo	*nextBufferInfo = mBufferQueue->Get(mCurrentBufferIndex + 1);
				if ((nextBufferInfo != NULL) || (mLooping == true))
				{
					if (nextBufferInfo != NULL)
					{
						mCurrentBufferIndex++;
					}
					else
					{
						RewindToStart();
						nextBufferInfo = mBufferQueue->Get(mCurrentBufferIndex);
					}
					
					// if the format of the next buffer is the same, return an error so we get called again,
					// else, return what we have and no error, the AC should then be reset in the RenderProc
					if (nextBufferInfo->mBuffer->mDataFormat.mSampleRate != bufferInfo->mBuffer->mDataFormat.mSampleRate)
					{
	                    // if the next buffer is a different sample rate, the bus format will need to be changed
						mResetBusFormat = true;
					}
				}
				else
				{
					// looping is false and there are no more buffers so we are really out of data
					// return what we have and no error, the AC should then be reset in the RenderProc
					// and what ever data is in the AC should get returned
					mBufferQueue->SetBufferAsProcessed(mCurrentBufferIndex);
					mQueueIsProcessed = true;		// we are done now, the Q is dry
				}
				// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			}
		}
	}
	
	// if there wasn't enough data left, be sure to silence the end of the buffer
	if (packetsObtained < packetsRequestedFromRenderProc)
	{
		for (UInt32	i = 0; i < tempBufferList->mNumberBuffers; i++)
		{
			tempBufferList->mBuffers[i].mDataByteSize = dataByteSize - (packetsObtained * sizeof(Float32));
			tempBufferList->mBuffers[i].mData = (Byte *) ioData->mBuffers[i].mData + (packetsObtained * sizeof(Float32));
			memset(tempBufferList->mBuffers[i].mData, 0, tempBufferList->mBuffers[i].mDataByteSize);
		}		
	}
		
	for (UInt32	i = 0; i < tempBufferList->mNumberBuffers; i++)
	{
		tempBufferList->mBuffers[i].mData = dataStarts[i];
		tempBufferList->mBuffers[i].mDataByteSize = dataByteSize;
	}

	return (noErr);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus OALSource::DoPostRender ()
{
	if ((mQueueIsProcessed == false) && (mShouldFade == false))
		return (noErr);
	
#if USE_MUTEXES
	TRY_PLAY_MUTEX
#endif

	if (mShouldFade) 
    {
		SetBusGain ();  // it was turned down, so reset it now
        mShouldFade = false;
	}
	
	if (mQueueIsProcessed == true)
	{
		PlayFinished();
		mState = AL_STOPPED;
	}
	
#if USE_MUTEXES
	UNLOCK_PLAY_MUTEX
#endif	
	return noErr;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// CalculateDistanceAndAzimuth() support
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void aluCrossproduct(ALfloat *inVector1,ALfloat *inVector2,ALfloat *outVector)
{
	outVector[0]=(inVector1[1]*inVector2[2]-inVector1[2]*inVector2[1]);
	outVector[1]=(inVector1[2]*inVector2[0]-inVector1[0]*inVector2[2]);
	outVector[2]=(inVector1[0]*inVector2[1]-inVector1[1]*inVector2[0]);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Float32 aluDotproduct(ALfloat *inVector1,ALfloat *inVector2)
{
	return (inVector1[0]*inVector2[0]+inVector1[1]*inVector2[1]+inVector1[2]*inVector2[2]);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void aluNormalize(ALfloat *inVector)
{
	ALfloat length,inverse_length;

	length=(ALfloat)sqrt(aluDotproduct(inVector,inVector));
	if (length != 0)
	{
		inverse_length=(1.0f/length);
		inVector[0]*=inverse_length;
		inVector[1]*=inverse_length;
		inVector[2]*=inverse_length;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void aluMatrixVector(ALfloat *vector,ALfloat matrix[3][3])
{
	ALfloat result[3];

	result[0]=vector[0]*matrix[0][0]+vector[1]*matrix[1][0]+vector[2]*matrix[2][0];
	result[1]=vector[0]*matrix[0][1]+vector[1]*matrix[1][1]+vector[2]*matrix[2][1];
	result[2]=vector[0]*matrix[0][2]+vector[1]*matrix[1][2]+vector[2]*matrix[2][2];
	memcpy(vector,result,sizeof(result));
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALSource::CalculateDistanceAndAzimuth(Float32 *outDistance, Float32 *outAzimuth)
{

    Float32 	ListenerOrientation[6],
        ListenerPosition[3],
        ListenerVelocity[3],
        Angle = 0.0,
        Distance = 2.0,
        Distance_squared = 4.0,
        front_back,
        SourceToListener[3],
        Look_Norm[3],
        U[3],
        tPosition[3];
        
    tPosition[0] = mPosition[0];
	tPosition[1] = mPosition[1];
	tPosition[2] = mPosition[2];

#if LOG_VERBOSE
        //DebugMessageN3("CalculateDistanceAndAzimuth source position = %f2/%f2/%f2\n", mPosition[0], mPosition[1], mPosition[2]);
#endif
			
	//Get listener properties
	mOwningContext->GetListenerPosition(&ListenerPosition[0], &ListenerPosition[1], &ListenerPosition[2]);
	mOwningContext->GetListenerVelocity(&ListenerVelocity[0], &ListenerVelocity[1], &ListenerVelocity[2]);
	mOwningContext->GetListenerOrientation(&ListenerOrientation[0], &ListenerOrientation[1], &ListenerOrientation[2],
											&ListenerOrientation[3], &ListenerOrientation[4], &ListenerOrientation[5]);

#if LOG_VERBOSE
    //DebugMessageN3("CalculateDistanceAndAzimuth listener position = %f2/%f2/%f2\n", ListenerPosition[0], ListenerPosition[1], ListenerPosition[2]);
#endif

	// Get buffer properties
	BufferInfo	*bufferInfo = mBufferQueue->Get(mCurrentBufferIndex);
	if (bufferInfo == NULL)
	{
        // Not sure if this should be the error case
        *outDistance = 0.0;
        *outAzimuth = 0.0;
        return;	// there are no buffers
	}
    
	// Only apply 3D calculations for mono buffers
	if (bufferInfo->mBuffer->mDataFormat.NumberChannels() == 1)
	{
		//1. Translate Listener to origin (convert to head relative)
		if (mSourceRelative == AL_FALSE)
		{
			tPosition[0] -= ListenerPosition[0];
			tPosition[1] -= ListenerPosition[1];
			tPosition[2] -= ListenerPosition[2];
		}
        //2. Align coordinate system axes
        aluCrossproduct(&ListenerOrientation[0],&ListenerOrientation[3],U); // Right-ear-vector
        aluNormalize(U); // Normalized Right-ear-vector
        Look_Norm[0] = ListenerOrientation[0];
        Look_Norm[1] = ListenerOrientation[1];
        Look_Norm[2] = ListenerOrientation[2];
        aluNormalize(Look_Norm);
                
       //3. Calculate distance attenuation
        Distance_squared = aluDotproduct(tPosition,tPosition);
		Distance = sqrt(Distance_squared);
                                                                                                       
        Angle = 0.0f;
        //4. Determine Angle of source relative to listener
        if(Distance>0.0f){
            SourceToListener[0]=tPosition[0];	
            SourceToListener[1]=tPosition[1];
            SourceToListener[2]=tPosition[2];
            aluNormalize(SourceToListener);
            
            Angle = 180.0 * acos (aluDotproduct(SourceToListener,U))/3.141592654f;
            
            //is the source infront of the listener or behind?
            front_back = aluDotproduct(SourceToListener,Look_Norm);
            if(front_back<0.0f)
                Angle = 360.0f - Angle;
                
            //translate from cartesian angle to 3d mixer angle
            if((Angle>=0.0f)&&(Angle<=270.0f)) Angle = 90.0f - Angle;
            else Angle = 450.0f - Angle;
        }
    }
    else
    {
        Angle=0.0;
        Distance=0.0;
    }
        
    if (!mOwningDevice->IsPreferredMixerAvailable() && (GetReferenceDistance() > 1.0))
    {
        // the pre 2.0 mixer does not have the DistanceParam property so to compensate,
        // set the DistanceAtten property correctly for refDist, maxDist, and rolloff,
        // and then scale our calculated distance to a reference distance of 1.0 before passing to the mixer
        Distance = Distance/GetReferenceDistance();
        if (Distance > GetMaxDistance()/GetReferenceDistance()) 
            Distance = GetMaxDistance()/GetReferenceDistance(); // clamp the distance to the max distance
    }

    *outDistance = Distance;
    *outAzimuth = Angle;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Support for Pre 2.0 3DMixer
//
// Pull the audio data by using DoRender(), and then Sample Rate Convert it to the mixer's 
// output sample rate so the 1.3 mixer doesn't have to do any SRC.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OSStatus	OALSource::DoSRCRender(	AudioBufferList 			*ioData )
{
    BufferInfo	*bufferInfo = mBufferQueue->Get(mCurrentBufferIndex);
    if (bufferInfo == NULL)
        return -1;

	double srcSampleRate = bufferInfo->mBuffer->mDataFormat.mSampleRate;
	double dstSampleRate = mOwningDevice->GetMixerRate();
	double ratio = (srcSampleRate / dstSampleRate) * mPitch;
	
	int nchannels = ioData->mNumberBuffers;

	if (ratio == 1.0)
	{
		// no SRC necessary so just call the normal render proc and let it fill out the buffers
        return (DoRender(ioData));
 	}

	// otherwise continue on to do dirty linear interpolation
	UInt32      inFramesToProcess = ioData->mBuffers[0].mDataByteSize / sizeof(Float32);
	float readIndex = mReadIndex;

	int readUntilIndex = (int) (2.0 + readIndex + inFramesToProcess * ratio );
	int framesToPull = readUntilIndex - 2;
	
	if (framesToPull == 0) 
        return -1;
	
    // set the buffer size so DoRender will get the correct amount of frames
    mTempSourceStorage->mBuffers[0].mDataByteSize = framesToPull * sizeof(UInt32);
    mTempSourceStorage->mBuffers[1].mDataByteSize = framesToPull * sizeof(UInt32);
    
    // if the size of the buffers are too small, reallocate them now
    if (mTempSourceStorageBufferSize < (framesToPull * sizeof(UInt32)))
    {
        if (mTempSourceStorage->mBuffers[0].mData != NULL)
            free(mTempSourceStorage->mBuffers[0].mData);

        if (mTempSourceStorage->mBuffers[1].mData != NULL)
            free(mTempSourceStorage->mBuffers[1].mData);
            
        mTempSourceStorageBufferSize = (framesToPull * sizeof(UInt32));
        mTempSourceStorage->mBuffers[0].mData = malloc(mTempSourceStorageBufferSize);
        mTempSourceStorage->mBuffers[1].mData = malloc(mTempSourceStorageBufferSize);
    }
       
	// get input source audio
    mTempSourceStorage->mNumberBuffers = ioData->mNumberBuffers;
    for (UInt32 i = 0; i < mTempSourceStorage->mNumberBuffers; i++)
    {
        mTempSourceStorage->mBuffers[i].mDataByteSize = framesToPull * sizeof(UInt32);
    }
    
    OSStatus result = DoRender(mTempSourceStorage);
	if (result != noErr ) 
        return result;		// !!@ something bad happened (could return error code)

	float *pullL = (float *) mTempSourceStorage->mBuffers[0].mData;
	float *pullR = nchannels > 1 ? (float *) mTempSourceStorage->mBuffers[1].mData: NULL;

	// setup a small array of the previous two cached values, plus the first new input frame
	float tempL[4];
	float tempR[4];
	tempL[0] = mCachedInputL1;
	tempL[1] = mCachedInputL2;
	tempL[2] = pullL[0];

	if (pullR)
	{
		tempR[0] = mCachedInputR1;
		tempR[1] = mCachedInputR2;
		tempR[2] = pullR[0];
	}

	// in first loop start out getting source from this small array, then change sourceL/sourceR to point
	// to the buffers containing the new pulled input for the main loop
	float *sourceL = tempL;
	float *sourceR = tempR;
	if(!pullR) 
        sourceR = NULL;

	// keep around for next time
	mCachedInputL1 = pullL[framesToPull - 2];
	mCachedInputL2 = pullL[framesToPull - 1];
	
	if(pullR)
	{
		mCachedInputR1 = pullR[framesToPull - 2];
		mCachedInputR2 = pullR[framesToPull - 1];
	}

	// quick-and-dirty linear interpolation
	int n = inFramesToProcess;
	
	float *destL = (float *) ioData->mBuffers[0].mData;
	float *destR = (float *) ioData->mBuffers[1].mData;
	
	if (!sourceR)
	{
		// mono input
		
		// generate output based on previous cached values
		while (readIndex < 2.0 &&  n > 0)
		{
			int iReadIndex = (int)readIndex;
			int iReadIndex2 = iReadIndex + 1;
			
			float frac = readIndex - float(iReadIndex);

			float s1 = sourceL[iReadIndex];
			float s2 = sourceL[iReadIndex2];
			float left  = s1 + frac * (s2-s1);
			
			*destL++ = left;
			
			readIndex += ratio;
			
			n--;
		}

		// generate output based on new pulled input

		readIndex -= 2.0;

		sourceL = pullL;

		while (n--)
		{
			int iReadIndex = (int)readIndex;
			int iReadIndex2 = iReadIndex + 1;
			
			float frac = readIndex - float(iReadIndex);

			float s1 = sourceL[iReadIndex];
			float s2 = sourceL[iReadIndex2];
			float left  = s1 + frac * (s2-s1);
			
			*destL++ = left;
			
			readIndex += ratio;
		}

		readIndex += 2.0;
	}
	else
	{
		// stereo input
		// generate output based on previous cached values
		while(readIndex < 2.0 &&  n > 0)
		{
			int iReadIndex = (int)readIndex;
			int iReadIndex2 = iReadIndex + 1;
			
			float frac = readIndex - float(iReadIndex);
			
			float s1 = sourceL[iReadIndex];
			float s2 = sourceL[iReadIndex2];
			float left  = s1 + frac * (s2-s1);
			
			float s3 = sourceR[iReadIndex];
			float s4 = sourceR[iReadIndex2];
			float right  = s3 + frac * (s4-s3);
			
			*destL++ = left;
			*destR++ = right;

			readIndex += ratio;
			
			n--;
		}

		// generate output based on new pulled input

		readIndex -= 2.0;

		sourceL = pullL;
		sourceR = pullR;

		while (n--)
		{
			int iReadIndex = (int)readIndex;
			int iReadIndex2 = iReadIndex + 1;
			
			float frac = readIndex - float(iReadIndex);
			
			float s1 = sourceL[iReadIndex];
			float s2 = sourceL[iReadIndex2];
			float left  = s1 + frac * (s2-s1);
			
			float s3 = sourceR[iReadIndex];
			float s4 = sourceR[iReadIndex2];
			float right  = s3 + frac * (s4-s3);
			
			*destL++ = left;
			*destR++ = right;

			readIndex += ratio;
		}

		readIndex += 2.0;

	}
	
	// normalize read index back to start of buffer for next time around...
	
	readIndex -= float(framesToPull);
    
	mReadIndex = readIndex;
	
	return noErr;
}