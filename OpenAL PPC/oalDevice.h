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

#ifndef __OAL_DEVICE__
#define __OAL_DEVICE__

#include "oalImp.h"
#include <Carbon/Carbon.h>
#include <CoreAudio/AudioHardware.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <map>
#include  "CAStreamBasicDescription.h"

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Some build flags for gathering performance and data flow information

#if USE_AU_TRACER
	#include "AUTracer.h"
#endif

#if DEBUG
	#define AUHAL_LOG_OUTPUT 0
#endif

#if AUHAL_LOG_OUTPUT
	#include "AudioLogger.h"
#endif
     
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Device Constants
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define kDefaultReferenceDistance   1.0
#define kDefaultMaximumDistance     1000000.0
#define kDefaultRolloff             1.0

#define kPreferredMixerVersion 		0x21000 
#define kMinimumMixerVersion 		0x10300

// Default Mixer Output Sample Rate Setting:
#define	kDefaultMixerRate       44100.0

// Default Low Quality Stereo Spatial Setting:
#define	kDefaultLowQuality      kSpatializationAlgorithm_EqualPowerPanning

// Default High Quality Stereo Spatial Setting:
#define	kDefaultHighQuality     kSpatializationAlgorithm_HRTF

// Default MultiChannel Spatial Setting:
#define	kDefaultMultiChannelQuality kSpatializationAlgorithm_SoundField

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Device Structs
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

struct BusInfo {
					bool		mIsAvailable;       // is an OALSource using this bus?
					UInt32		mNumberChannels;    // mono/stereo setting of the bus
                    UInt32      mReverbSetting;     // unused until Reverb extension is added to the implementation
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALDevices
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class OALDevice
{
	public:

	OALDevice(const char* 	 inDeviceName, UInt32   inSelfToken, UInt32  &inBusCount);
	~OALDevice();
	
	// graph methods
	void 			InitializeGraph (const char* 	inDeviceName);
	void			SetupGraph();
	void			TeardownGraph();
	void			ConfigureGraphForChannelLayout();

    // bus methods
	UInt32 			GetAvailableMonoBus ();
	UInt32 			GetAvailableStereoBus ();
	UInt32 			GetBusCount() const { return mBusCount; }
	void			SetBusAsAvailable (UInt32 inBusIndex) const;
	void 			InitRenderQualityOnBusses(UInt32	&inRenderQuality);

	// set info
	void			SetEnvironment(OALEnvironmentInfo	&inEnvironment);
	void			SetMixerRate (Float64	inSampleRate);
	void 			SetRenderQuality (UInt32 inRenderQuality);
	void 			SetRenderChannelCount (UInt32 inRenderChannelCount);
    void            SetReverb(UInt32  inReverbSetting);
    void            SetDistanceAttenuation(UInt32    inBusIndex, Float64 inRefDist, Float64 inMaxDist, Float64 inRolloff);

	// get info
	UInt32          GetDeviceToken () const { return mSelfToken; }
	Float64			GetMixerRate () const { return mMixerOutputRate; }
	UInt32			GetRenderQuality() {return mRenderQuality;}
	UInt32			GetRenderChannelCount() {return  mRenderChannelCount; }
	UInt32 			GetReverbSetting() const { return mReverbSetting; }
	AudioUnit		GetMixerUnit() { return mMixerUnit;}
	UInt32 			GetDesiredRenderChannelCount ();
	bool            IsDistanceScalingRequired() { return mDistanceScalingRequired;}
	Float32         GetDefaultReferenceDistance() { return mDefaultReferenceDistance;}
	Float32         GetDefaultMaxDistance() { return mDefaultMaxDistance;}

	// misc.
    bool            IsPreferredMixerAvailable() { return mPreferred3DMixerExists; }
	bool 			IsPlayable () { return mCanScheduleEvents; }
	bool 			IsValidRenderQuality (UInt32 inRenderQuality);
	void            ResetChannelLayout();
    static void     GraphFormatPropertyListener 	(	void 					*inRefCon, 
                                                        AudioUnit 				ci, 
                                                        AudioUnitPropertyID 	inID, 
                                                        AudioUnitScope 			inScope, 
                                                        AudioUnitElement 		inElement);

	bool	IsGraphStillRunning ()
	{
		Boolean running;
		OSStatus result = AUGraphIsRunning (mAUGraph, &running);
			THROW_RESULT
		return bool(running);	
	}

	void 	Print () const 
	{
#if	DEBUG || CoreAudio_Debug
		CAShow (mAUGraph); 
#endif
	}


#pragma mark __________ Private_Class_Members

	private:
		UInt32          mSelfToken;
        AudioDeviceID   mHALDevice;                     // the HAL device used to render audio to the user		
        bool            mPreferred3DMixerExists;
        bool            mDistanceScalingRequired;
        Float32         mDefaultReferenceDistance;
        Float32         mDefaultMaxDistance;
        AUGraph 		mAUGraph;
        UInt32          mBusCount;
        BusInfo			*mBusInfo;
        AUNode			mOutputNode;
        AudioUnit 		mOutputUnit;
        AUNode			mMixerNode;
        AudioUnit 		mMixerUnit;
        bool 			mCanScheduleEvents;
        Float64		 	mMixerOutputRate;
        UInt32			mCurrentMixerChannelCount;
        UInt32          mRenderChannelCount;         // currently either stereo or multichannel
        UInt32			mRenderQuality;                 // Hi or Lo for now
        UInt32			mSpatialSetting;
        UInt32          mReverbSetting;
#if LOG_BUS_CONNECTIONS
        UInt32			mMonoSourcesConnected;
        UInt32			mStereoSourcesConnected;
#endif
#if AUHAL_LOG_OUTPUT
        AudioLogger     mLogger;
#endif
#if USE_AU_TRACER
        AUTracer		mAUTracer;
#endif

};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark _____OALDeviceMap_____
class OALDeviceMap : std::multimap<UInt32, OALDevice*, std::less<UInt32> > {
public:
    
    void Add (const	UInt32	inDeviceToken, OALDevice **inDevice)  {
		iterator it = upper_bound(inDeviceToken);
		insert(it, value_type (inDeviceToken, *inDevice));
	}

    OALDevice* Get(UInt32	inDeviceToken) {
        iterator	it = find(inDeviceToken);
        if (it != end())
            return ((*it).second);
		return (NULL);
    }
    
    void Remove (const	UInt32	inDeviceToken) {
        iterator 	it = find(inDeviceToken);
        if (it != end())
            erase(it);
    }
	
    UInt32 Size () const { return size(); }
    bool Empty () const { return empty(); }
};

#endif
