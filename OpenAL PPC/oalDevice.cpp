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

#include "oalDevice.h"

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define LOG_BUS_CONNECTIONS  	0
#define PROFILE_IO_USAGE  		0

#if PROFILE_IO_USAGE
static int debugCounter         = -1;
static int numCyclesToPrint     = 1000;

static UInt64 lastHostTime;
static UInt64 totalHostTime;
static UInt64 minUsage;
static UInt64 maxUsage;
static UInt64 totalUsage;

#define PROFILE_IO_CYCLE 0
#if PROFILE_IO_CYCLE
static UInt64 maxHT;
static UInt64 minHT;
#endif

#include <CoreAudio/HostTime.h>
#endif

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
#if GET_OVERLOAD_NOTIFICATIONS
OSStatus	PrintTheOverloadMessage(	AudioDeviceID			inDevice,
										UInt32					inChannel,
										Boolean					isInput,
										AudioDevicePropertyID	inPropertyID,
										void*					inClientData)
{
	DebugMessage("OVERLOAD OCCURRED");
}
#endif

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALDevices
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** OALDevices *****

/*
	If caller wants a specific HAL device (instead of the default output device), a NULL terminated 
	C-String representation of the CFStringRef returned from the HAL APIs for the 
	kAudioDevicePropertyDeviceUID property
*/
OALDevice::OALDevice (const char* 	 inDeviceName, UInt32   inSelfToken, UInt32  &inBusCount)
	: 	mSelfToken (inSelfToken),
        mHALDevice (0),
        mPreferred3DMixerExists(true),
        mDistanceScalingRequired(false),
        mDefaultReferenceDistance(1.0),
        mDefaultMaxDistance(100000.0),
        mAUGraph(0),
        mBusCount(inBusCount),
		mOutputNode(0), 
		mOutputUnit(0),
		mMixerNode(0), 
		mMixerUnit (0),
		mMixerOutputRate(kDefaultMixerRate),
		mCurrentMixerChannelCount(0),
        mRenderChannelCount(ALC_RENDER_CHANNEL_COUNT_MULTICHANNEL),
		mRenderQuality(ALC_SPATIAL_RENDERING_QUALITY_LOW),
		mSpatialSetting(0),
        mReverbSetting(0)
#if LOG_BUS_CONNECTIONS
		, mMonoSourcesConnected(0),
		mStereoSourcesConnected(0)
#endif
{
	OSStatus	result = noErr;
	UInt32		size = 0;
	CFStringRef	cfString = NULL;
    char        *useThisDevice = (char *) inDeviceName;
	
	try {
        // until the ALC_ENUMERATION_EXT extension is supported only use the default output device
        useThisDevice = NULL;
        
		// first, get the requested HAL device's ID
		if (useThisDevice)
		{
			// turn the inDeviceName into a CFString
			cfString = CFStringCreateWithCString(NULL, useThisDevice, kCFStringEncodingUTF8);
			if (cfString)
			{
				AudioValueTranslation	translation;
				
				translation.mInputData = &cfString;
				translation.mInputDataSize = sizeof(cfString);
				translation.mOutputData = &mHALDevice;
				translation.mOutputDataSize = sizeof(mHALDevice);
				
				size = sizeof(AudioValueTranslation);
				result = AudioHardwareGetProperty(kAudioHardwarePropertyDeviceForUID, &size, &translation);
                CFRelease (cfString);
			}
			else
				result = -1; // couldn't get string ref
				
			THROW_RESULT
		}
		else
		{
			size = sizeof(AudioDeviceID);
			result = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &mHALDevice);
				THROW_RESULT
		}
		
		InitializeGraph(useThisDevice);
		
#if PROFILE_IO_USAGE
		debugCounter = -1;
#endif

	}
	catch (...) {
        throw;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OALDevice::~OALDevice()
{		
	TeardownGraph();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALDevice::SetEnvironment(OALEnvironmentInfo	&inEnvironment)
{
    // currently unused, will be useful when applications actually switch between different OAL Contexts
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALDevice::InitializeGraph (const char* 		inDeviceName)
{	
	if (mAUGraph)
		throw static_cast<OSStatus>('init');
	
    OSStatus result = noErr;

    // ~~~~~~~~~~~~~~~~~~~ GET 3DMIXER VERSION

	ComponentDescription	mixerCD;
	mixerCD.componentFlags = 0;        
	mixerCD.componentFlagsMask = 0;     
    mixerCD.componentType = kAudioUnitType_Mixer;          
	mixerCD.componentSubType = kAudioUnitSubType_3DMixer;       
	mixerCD.componentManufacturer = kAudioUnitManufacturer_Apple;  

    ComponentInstance   mixerInstance = OpenComponent(FindNextComponent(0, &mixerCD));
    long  version = GetComponentVersion(mixerInstance);
    CloseComponent(mixerInstance);
    
    if (version < kMinimumMixerVersion)
        throw -1;                           // we do not have a current enough 3DMixer to use, so set an error and throw
    else if (version < 0x20000)
        mPreferred3DMixerExists = false;    // we do not have version 2.0 or greater of the 3DMixer
    else if (version == 0x20000)
        mDistanceScalingRequired = true;        
    
    // ~~~~~~~~~~~~~~~~~~~~ CREATE GRAPH

	result = NewAUGraph(&mAUGraph);
		THROW_RESULT
	 
    // ~~~~~~~~~~~~~~~~~~~~ SET UP BASIC NODES

	ComponentDescription	cd;
	cd.componentFlags = 0;        
	cd.componentFlagsMask = 0;     

	// At this time, only allow the default output device to be used and ignore the inDeviceName parameter
	cd.componentType = kAudioUnitType_Output;          
	cd.componentSubType = kAudioUnitSubType_DefaultOutput;       	
	cd.componentManufacturer = kAudioUnitManufacturer_Apple;  
	result = AUGraphNewNode (mAUGraph, &cd, 0, NULL, &mOutputNode);
		THROW_RESULT
		        
    result = AUGraphNewNode (mAUGraph, &mixerCD, 0, NULL, &mMixerNode);
		THROW_RESULT
		
	// ~~~~~~~~~~~~~~~~~~~~ OPEN AND INITIALIZE GRAPH
	
	result = AUGraphOpen (mAUGraph);
		THROW_RESULT
	
	result = AUGraphGetNodeInfo (mAUGraph, mMixerNode, 0, 0, 0, &mMixerUnit);
		THROW_RESULT   
	
	result = AUGraphGetNodeInfo (mAUGraph, mOutputNode, 0, 0, 0, &mOutputUnit);
		THROW_RESULT   

#if	USE_AU_TRACER
	result = mAUTracer.Establish (mOutputUnit, "/tmp/OALOverloadTraces");
	printf ("\nProfiling Overload Latencies to log files: /tmp/OALOverloadTraces.txt\n");
#endif
	
 	result = AudioUnitInitialize (mOutputUnit);
		THROW_RESULT

	SetupGraph();

    // store some default distance settings
    if (mPreferred3DMixerExists)
    {
        MixerDistanceParams		distanceParams;
        UInt32                  outSize;
        result = AudioUnitGetProperty(GetMixerUnit(), kAudioUnitProperty_3DMixerDistanceParams, kAudioUnitScope_Input, 1, &distanceParams, &outSize);
        if (result == noErr)
        {
            mDefaultReferenceDistance = distanceParams.mReferenceDistance;
            mDefaultMaxDistance = distanceParams.mMaxDistance;
        }
    }
	 	
	mCanScheduleEvents = true;
	Print();

	AUGraphStart(mAUGraph);
		
#if GET_OVERLOAD_NOTIFICATIONS
	AudioDeviceID	device = 0;
	UInt32			size = sizeof(device);
	
	AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &device);
	if (device != 0)
	{
		DebugMessage("********** Adding Overload Notification ***********");
		AudioDeviceAddPropertyListener(	device, 0, false, kAudioDeviceProcessorOverload, PrintTheOverloadMessage, NULL);	
	}
#endif	
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALDevice::TeardownGraph()
{
#if GET_OVERLOAD_NOTIFICATIONS
	AudioDeviceID	device = 0;
	UInt32			size = sizeof(device);
	
	AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &device);
	if (device != 0)
	{
		DebugMessage("********** Removing Overload Notification ***********");
		AudioDeviceRemovePropertyListener(	device, 0, false, kAudioDeviceProcessorOverload, PrintTheOverloadMessage);	
	}
#endif

	if (mAUGraph) 
	{
		AUGraphStop (mAUGraph);

		DisposeAUGraph (mAUGraph);
		mAUGraph = 0;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/*  
    Determine the correct number of channels for the mixer to render to, by discovering what 
    channel layout has been set for the output unit. Account for differences in the 
    1.3/2.0 versions of the 3DMixer, and user setting of for the ALC_RENDER_CHANNEL_COUNT
    OAL extension. This method should only return: 2 (stereo) ,4 (quad) or 5 (5.0).
*/
UInt32 OALDevice::GetDesiredRenderChannelCount ()
{
	UInt32			returnValue = 2;	// return stereo by default
	
    // observe the mRenderChannelCount flag and clamp to stereo if necessary
    // This allows the user to request the libary to render to stereo in the case where only 2 speakers
    // are connected to multichannel hardware
    if (mRenderChannelCount == ALC_RENDER_CHANNEL_COUNT_STEREO)
        return (returnValue);

	// get the HAL device id form the output AU
	AudioDeviceID	deviceID;
	UInt32			outSize =  sizeof(deviceID);
	OSStatus	result = AudioUnitGetProperty(mOutputUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Output, 1, &deviceID, &outSize);
		THROW_RESULT
	
	// get the channel layout set by the user in AMS		
	result = AudioDeviceGetPropertyInfo(deviceID, 0, false, kAudioDevicePropertyPreferredChannelLayout, &outSize, NULL);
    if (result != noErr)
        return (returnValue);   // default to stereo since channel layout could not be obtained

	AudioChannelLayout	*layout = NULL;
	layout = (AudioChannelLayout *) calloc(1, outSize);
	if (layout != NULL)
	{
		result = AudioDeviceGetProperty(deviceID, 0, false, kAudioDevicePropertyPreferredChannelLayout, &outSize, layout);
		if (layout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelDescriptions)
		{
			// no channel layout tag is returned, so walk through the channel descriptions and count 
            // the channels that are associated with a speaker
			if (layout->mNumberChannelDescriptions == 2)
			{	
                returnValue = 2;        // there is no channel info for stereo
			}
			else
			{
				returnValue = 0;
				for (UInt32 i = 0; i < layout->mNumberChannelDescriptions; i++)
				{
					if (layout->mChannelDescriptions[i].mChannelLabel != kAudioChannelLabel_Unknown)
						returnValue++;
				}
			}
		}
		else
		{
			switch (layout->mChannelLayoutTag)
			{
				case kAudioChannelLayoutTag_AudioUnit_5_0:
				case kAudioChannelLayoutTag_AudioUnit_5_1:
				case kAudioChannelLayoutTag_AudioUnit_6:
					returnValue = 5;
					break;
				case kAudioChannelLayoutTag_AudioUnit_4:
					returnValue = 4;
					break;
				default:
					returnValue = 2;
					break;
			}
		}
	
		free(layout);
	}
    
    if ((!mPreferred3DMixerExists) && (returnValue == 4))
    {
        // quad did not work properly before version 2.0 of the 3DMixer, so just render to stereo
        returnValue = 2;
    }
    else if (returnValue < 4)
    {
        // guard against the possibility of multi channel hw that has never been given a preferred channel layout
        // Or, that a 3 channel layout was returned (which is unsupported by the 3DMixer)
        returnValue = 2; 
    } 
    else if (returnValue > 5)
    {
        // 3DMixer currently does not render to more than 5 channels
        returnValue = 5;    
    }
        
	return (returnValue);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/*
    This method should ONLY be called just before the mixer and output AUs are connected. 
    By explicitly setting the mixer's channel layout, the input scope of the output AU will
    get configured correctly at connection time.
*/
void	OALDevice::ConfigureGraphForChannelLayout()
{
	OSStatus	result = noErr;

	// get the channel count that should be set for the mixer's output stream format
	mCurrentMixerChannelCount = GetDesiredRenderChannelCount();

	// set the stream format
	CAStreamBasicDescription	format;
	UInt32                      outSize = sizeof(format);
	result = AudioUnitGetProperty(mOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &format, &outSize);
		THROW_RESULT

	format.SetCanonical (mCurrentMixerChannelCount, false);     // not interleaved
	format.mSampleRate = mMixerOutputRate;
	outSize = sizeof(format);
	result = AudioUnitSetProperty (mOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, outSize);
		THROW_RESULT

	result = AudioUnitSetProperty (mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &format, outSize);
		THROW_RESULT

    // explicitly set the channel layout, pre 2.0 mixer is just 5.0 and stereo
    // the output AU will then be configured correctly when the mixer is connected to it later
    AudioChannelLayout		layout;
     
    if (mCurrentMixerChannelCount == 5)
        layout.mChannelLayoutTag = kAudioChannelLayoutTag_AudioUnit_5_0;
    else if (mCurrentMixerChannelCount == 4)
        layout.mChannelLayoutTag = (mPreferred3DMixerExists == true) ? kAudioChannelLayoutTag_AudioUnit_4 : kAudioChannelLayoutTag_Stereo;
    else
        layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
    
    layout.mChannelBitmap = 0;			
    layout.mNumberChannelDescriptions = 0;

    // it isn't currently necessary to explicitly set the mixer's output channel layout but it might in the future if there were more
    // than one layout usable by the current channel count. It doesn't hurt to set this property.
    outSize = sizeof(layout);
    result = AudioUnitSetProperty (mMixerUnit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Output, 0, &layout, outSize);

	result = AudioUnitSetProperty (mOutputUnit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Input, 0, &layout, outSize);
		THROW_RESULT
    
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALDevice::InitRenderQualityOnBusses(UInt32	&inRenderQuality)
{
	mRenderQuality = inRenderQuality;

	if (GetDesiredRenderChannelCount() > 2)
	{
        // at this time, there is only one spatial quality being used for multi channel hw
        DebugMessage("********** InitRenderQualityOnBusses:kDefaultMultiChannelQuality ***********");
		mSpatialSetting = kDefaultMultiChannelQuality;
	}
	else if (mRenderQuality == ALC_SPATIAL_RENDERING_QUALITY_LOW)
	{
		// this is the default case for stereo
        DebugMessage("********** InitRenderQualityOnBusses:kDefaultLowQuality ***********");
		mSpatialSetting =  kDefaultLowQuality;
	}
	else
	{
		DebugMessage("********** InitRenderQualityOnBusses:kDefaultHighQuality ***********");
		mSpatialSetting = kDefaultHighQuality;
	}
	
	UInt32 		render_flags_3d = k3DMixerRenderingFlags_DistanceAttenuation;
	if (mRenderQuality == ALC_SPATIAL_RENDERING_QUALITY_HIGH)
	{
    	 // off by default, on if the user sets High Quality rendering, as HRTF requires InterAuralDelay to be on
         render_flags_3d += k3DMixerRenderingFlags_InterAuralDelay;     
	}
    
    if (mReverbSetting > 0)
	{
    	 // off by default, on if the user turns on Reverb, as it requires DistanceDiffusion to be on
    	render_flags_3d += k3DMixerRenderingFlags_DistanceDiffusion;
    }
    
	OSStatus                    result = noErr;
    UInt32                      outSize;
    CAStreamBasicDescription	format;
	for (UInt32	i = 0; i < mBusCount; i++)
	{
		outSize = sizeof(format);
		result = AudioUnitGetProperty (	mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, i, &format, &outSize);
		
		// only reset the mono channels, stereo channels are always set to stereo pass thru regardless of render quality setting
        if ((result == noErr) && (format.NumberChannels() == 1)) 
		{
			// Spatialization
			result = AudioUnitSetProperty(	mMixerUnit, kAudioUnitProperty_SpatializationAlgorithm, kAudioUnitScope_Input, 
											i, &mSpatialSetting, sizeof(mSpatialSetting));

			// Render Flags                
            result = AudioUnitSetProperty(	mMixerUnit, kAudioUnitProperty_3DMixerRenderingFlags, kAudioUnitScope_Input, 
											i, &render_flags_3d, sizeof(render_flags_3d));
			
			// Doppler - This must be done AFTER the spatialization setting, because some algorithms explicitly turn doppler on
			UInt32		doppler = kDopplerDefault;
			result = AudioUnitSetProperty(mMixerUnit, kAudioUnitProperty_DopplerShift, kAudioUnitScope_Input, i, &doppler, sizeof(doppler));
		}
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALDevice::SetupGraph()
{
    ConfigureGraphForChannelLayout();	
	
	// connect mixer to output unit
	OSStatus	result = AUGraphConnectNodeInput (mAUGraph, mMixerNode, 0, mOutputNode, 0);
		THROW_RESULT

	// reverb off by default
    result = AudioUnitSetProperty(mMixerUnit, kAudioUnitProperty_UsesInternalReverb, kAudioUnitScope_Global, 0, &mReverbSetting, sizeof(mReverbSetting));
	// ignore result
		
	result = AUGraphInitialize (mAUGraph);
		THROW_RESULT

	// update the graph
	for (;;) {
		result = AUGraphUpdate (mAUGraph, NULL);
		if (result == noErr)
			break;
	}
		
	result = AudioUnitAddPropertyListener (mOutputUnit, kAudioUnitProperty_StreamFormat, GraphFormatPropertyListener, this);
		THROW_RESULT

    // ~~~~~~~~~~~~~~~~~~~~ BUS SETUP

    UInt32	outSize;
    if (!mPreferred3DMixerExists)
    {
        mBusCount = kDefaultMaximumMixerBusCount; // 1.3 version of the mixer did not allow a change in the bus count
    }
    else
    {
        // set the bus count on the mixer if necessary	
        UInt32  currentBusCount;
        outSize = sizeof(currentBusCount);
        result = AudioUnitGetProperty (	mMixerUnit, kAudioUnitProperty_BusCount, kAudioUnitScope_Input, 0, &currentBusCount, &outSize);
        if ((result == noErr) && (mBusCount != currentBusCount))
        {
            result = AudioUnitSetProperty (	mMixerUnit, kAudioUnitProperty_BusCount, kAudioUnitScope_Input, 0, &mBusCount, outSize);
            if (result != noErr)
            {
                // couldn't set the bus count so make sure we know just how many busses there are
                outSize = sizeof(mBusCount);
                AudioUnitGetProperty (	mMixerUnit, kAudioUnitProperty_BusCount, kAudioUnitScope_Input, 0, &mBusCount, &outSize);
            }
        }
    }
    mBusInfo = (BusInfo *) calloc (1, sizeof(BusInfo) * mBusCount);

	// INITIALIZE THE BUSSES - set doppler & distance attenuation, make all busses mono, since this is the more common case
	CAStreamBasicDescription	format;
	for (UInt32	i = 0; i < mBusCount; i++)
	{				
		// Stream Format: kAudioUnitProperty_StreamFormat: mono
		outSize = sizeof(format);
		AudioUnitGetProperty (	mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, i, &format, &outSize);
		if (format.NumberChannels() != 1)
		{
			format.mChannelsPerFrame = 1;
			AudioUnitSetProperty (	mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, i, &format, outSize);
		}

		// Distance Attenuation: for pre v2.0 mixer
        SetDistanceAttenuation (i, kDefaultReferenceDistance, kDefaultMaximumDistance, kDefaultRolloff);			

		mBusInfo[i].mIsAvailable = true;
		mBusInfo[i].mNumberChannels = 1;
		mBusInfo[i].mReverbSetting = mReverbSetting;
	}

	// this call sets the kAudioUnitProperty_SpatializationAlgorithm & kAudioUnitProperty_3DMixerRenderingFlags for all mono busses
	InitRenderQualityOnBusses(mRenderQuality); 	
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32		OALDevice::GetAvailableMonoBus ()
{
	// look for a bus already set for mono
	for (UInt32 i = 0; i < mBusCount; i++)
	{
		if (mBusInfo[i].mIsAvailable == true && mBusInfo[i].mNumberChannels == 1) 
		{
			mBusInfo[i].mIsAvailable = false;
#if LOG_BUS_CONNECTIONS
			mMonoSourcesConnected++;
			DebugMessageN2("GetAvailableMonoBus1: Sources Connected, Mono =  %ld, Stereo = %ld\n", MonoSourcesConnected(), StereoSourcesConnected());
#endif
			return (i);
		}
	}

	// couldn't find a mono bus, so find any available channel and make it mono
	for (UInt32 i = 0; i < mBusCount; i++)
	{
		if (mBusInfo[i].mIsAvailable == true) 
		{
#if LOG_BUS_CONNECTIONS
			mMonoSourcesConnected++;
			DebugMessageN2("GetAvailableMonoBus2: Sources Connected, Mono =  %ld, Stereo = %ld\n", MonoSourcesConnected(), StereoSourcesConnected());
#endif
			CAStreamBasicDescription 	theOutFormat;
			theOutFormat.mChannelsPerFrame = 1;
			theOutFormat.mSampleRate = GetMixerRate();          // as a default, set the bus to the mixer's output rate, it should get reset if necessary later on
			theOutFormat.mFormatID = kAudioFormatLinearPCM;
			theOutFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
			theOutFormat.mBytesPerPacket = sizeof (Float32);	
			theOutFormat.mFramesPerPacket = 1;	
			theOutFormat.mBytesPerFrame = sizeof (Float32);
			theOutFormat.mBitsPerChannel = sizeof (Float32) * 8;	
			OSStatus	result = AudioUnitSetProperty (	mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 
														i, &theOutFormat, sizeof(CAStreamBasicDescription));
				THROW_RESULT

			mBusInfo[i].mIsAvailable = false;
			mBusInfo[i].mNumberChannels = 1; 			
			AudioUnitSetProperty(	mMixerUnit, kAudioUnitProperty_SpatializationAlgorithm, kAudioUnitScope_Input, 
									i, &mSpatialSetting, sizeof(mSpatialSetting));

			UInt32 		render_flags_3d = k3DMixerRenderingFlags_DistanceAttenuation;
			if (mRenderQuality == ALC_SPATIAL_RENDERING_QUALITY_HIGH)
				render_flags_3d += k3DMixerRenderingFlags_InterAuralDelay; // off by default, on if the user sets High Quality rendering

			// Render Flags
			result = AudioUnitSetProperty(	mMixerUnit, kAudioUnitProperty_3DMixerRenderingFlags, kAudioUnitScope_Input, 
											i, &render_flags_3d, sizeof(render_flags_3d));
			
			return (i);
		}
	}
	
	DebugMessage("ERROR: GetAvailableMonoBus: COULD NOT GET A MONO BUS");
	throw (-1); // no inputs available
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32		OALDevice::GetAvailableStereoBus ()
{
	for (UInt32 i = 0; i < mBusCount; i++)
	{
		if (mBusInfo[i].mIsAvailable == true && mBusInfo[i].mNumberChannels == 2) 
		{
			mBusInfo[i].mIsAvailable = false;
#if LOG_BUS_CONNECTIONS
			mStereoSourcesConnected++;
			DebugMessageN2("GetAvailableStereoBus1: Sources Connected, Mono =  %ld, Stereo = %ld\n", MonoSourcesConnected(), StereoSourcesConnected());
			DebugMessageN1("GetAvailableStereoBus1: BUS_NUMBER = %ld\n", i);
#endif
			return (i);
		}
	}

	// couldn't find one, so look for a mono channel, make it stereo and set to kSpatializationAlgorithm_StereoPassThrough
	for (UInt32 i = 0; i < mBusCount; i++)
	{
		if (mBusInfo[i].mIsAvailable == true) 
		{

#if LOG_BUS_CONNECTIONS
			mStereoSourcesConnected++;
			DebugMessageN2("GetAvailableStereoBus2: Sources Connected, Mono =  %ld, Stereo = %ld\n", MonoSourcesConnected(), StereoSourcesConnected());
			DebugMessageN1("GetAvailableStereoBus2: BUS_NUMBER = %ld\n", i);
#endif
			CAStreamBasicDescription 	theOutFormat;
			theOutFormat.mChannelsPerFrame = 2;
			theOutFormat.mSampleRate = GetMixerRate();
			theOutFormat.mFormatID = kAudioFormatLinearPCM;
			theOutFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
			theOutFormat.mBytesPerPacket = sizeof (Float32);	
			theOutFormat.mFramesPerPacket = 1;	
			theOutFormat.mBytesPerFrame = sizeof (Float32);
			theOutFormat.mBitsPerChannel = sizeof (Float32) * 8;	
			OSStatus	result = AudioUnitSetProperty (	mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 
														i, &theOutFormat, sizeof(CAStreamBasicDescription));
				THROW_RESULT

			mBusInfo[i].mIsAvailable = false;
			mBusInfo[i].mNumberChannels = 2; 

			UInt32		spatAlgo = kSpatializationAlgorithm_StereoPassThrough;
			AudioUnitSetProperty(	mMixerUnit, kAudioUnitProperty_SpatializationAlgorithm, kAudioUnitScope_Input, 
									i, &spatAlgo, sizeof(spatAlgo));

			return (i);
		}
	}
	
	DebugMessage("ERROR: GetAvailableStereoBus: COULD NOT GET A STEREO BUS");
	throw (-1); // no inputs available
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALDevice::SetBusAsAvailable (UInt32 inBusIndex) const
{	
	mBusInfo[inBusIndex].mIsAvailable = true;

#if LOG_BUS_CONNECTIONS
	if (mBusInfo[inBusIndex].mNumberChannels == 1)
		mMonoSourcesConnected--;
	else
		mStereoSourcesConnected--;

		DebugMessageN2("SetBusAsAvailable: Sources Connected, Mono =  %ld, Stereo = %ld\n", MonoSourcesConnected(), StereoSourcesConnected());
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALDevice::ResetChannelLayout()
{
	// verify that the channel count has actually changed before doing all this work...
	UInt32	channelCount = GetDesiredRenderChannelCount();

	if (mCurrentMixerChannelCount == channelCount)
		return; // only reset the graph if the channel count has changed

	mCanScheduleEvents = false;

	AUGraphStop (mAUGraph);
	Boolean flag;
	do {
		AUGraphIsRunning (mAUGraph, &flag);
	} while (flag);

	// disconnect the mixer from the  output au
	OSStatus	result = AUGraphDisconnectNodeInput (mAUGraph, mOutputNode, 0);
	// update the graph
	for (;;) {
		result = AUGraphUpdate (mAUGraph, NULL);
		if (result == noErr)
			break;
	}
	
	ConfigureGraphForChannelLayout();
	
	// connect mixer to output unit
	result = AUGraphConnectNodeInput (mAUGraph, mMixerNode, 0, mOutputNode, 0);
		THROW_RESULT
		
	// update the graph
	for (;;) {
		result = AUGraphUpdate (mAUGraph, NULL);
		if (result == noErr)
			break;
	}

	InitRenderQualityOnBusses (mRenderQuality);

	AUGraphStart(mAUGraph);
	
	mCanScheduleEvents = true;

	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALDevice::GraphFormatPropertyListener (	void *inRefCon, 
													AudioUnit ci, 
													AudioUnitPropertyID inID, 
													AudioUnitScope inScope, 
													AudioUnitElement inElement)
{
	try {
		if (inScope == kAudioUnitScope_Output)
		{
			((OALDevice*)inRefCon)->ResetChannelLayout ();
		}		
	} 
	catch (...) {
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALDevice::SetMixerRate (Float64	inSampleRate)
{
	DebugMessage("OALDevice::SetMixerRate called");
	
	if (mMixerOutputRate == inSampleRate)
		return;		// nothing to do	
	
	mCanScheduleEvents = false;
	
	AUGraphDisconnectNodeInput (mAUGraph, mOutputNode, 0);
	OSStatus result;
	for (;;) 
    {
		result = AUGraphUpdate (mAUGraph, NULL);
		if (result == noErr)
			break;
	}

	AudioUnitUninitialize (mMixerUnit);
	
	// reconfigure the graph's mixer...
	CAStreamBasicDescription outFormat;
	UInt32 outSize = sizeof(outFormat);
	result = AudioUnitGetProperty (mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &outFormat, &outSize);
		THROW_RESULT
		
	outFormat.mSampleRate = inSampleRate;
	result = AudioUnitSetProperty (mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &outFormat, sizeof(outFormat));
		THROW_RESULT
		
    // lets just do the output unit format
	result = AudioUnitSetProperty (mOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &outFormat, sizeof(outFormat));
		THROW_RESULT

	AudioUnitInitialize(mMixerUnit);

	result = AUGraphConnectNodeInput (mAUGraph, mMixerNode, 0, mOutputNode, 0);
		THROW_RESULT

	for (;;) 
    {
		result = AUGraphUpdate (mAUGraph, NULL);
		if (result == noErr)
			break;
	}

	mMixerOutputRate = inSampleRate;
	mCanScheduleEvents = true;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALDevice::SetRenderChannelCount (UInt32 inRenderChannelCount)
{
    if ((inRenderChannelCount != ALC_RENDER_CHANNEL_COUNT_MULTICHANNEL) && (inRenderChannelCount != ALC_RENDER_CHANNEL_COUNT_STEREO))
		throw (OSStatus) AL_INVALID_VALUE;
    
    if (inRenderChannelCount == mRenderChannelCount)
        return; //nothing to do
        
    mRenderChannelCount = inRenderChannelCount;
    
    if (inRenderChannelCount == ALC_RENDER_CHANNEL_COUNT_STEREO)
    {
        // clamping to stereo
        if (mCurrentMixerChannelCount == 2)
            return; // already rendering to stereo, so there's nothing to do
    }
    else
    {
        // allowing multi channel now
        if (mCurrentMixerChannelCount > 2)
            return; // already rendering to mc, so there's nothing to do
    }    

    // work to be done now, it is necessary to change the channel layout and stream format from multi channel to stereo
    // this requires the graph to be stopped and reconfigured
    ResetChannelLayout ();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALDevice::SetRenderQuality (UInt32 inRenderQuality)
{
	if (mRenderQuality == inRenderQuality)
		return;	// nothing to do;

	// make sure a valid quality setting is requested
	if (!IsValidRenderQuality(inRenderQuality))
		throw (OSStatus) AL_INVALID_VALUE;
			
	// change the spatialization for all mono busses on the mixer
	InitRenderQualityOnBusses(inRenderQuality); 
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALDevice::SetReverb(UInt32  inReverbSetting)
{
	if (mReverbSetting == inReverbSetting)
		return;	// nothing to do;

    mReverbSetting = inReverbSetting;
    if (mReverbSetting > 1)
        mReverbSetting = 1;
        
    OSStatus    result = AudioUnitSetProperty(mMixerUnit, kAudioUnitProperty_UsesInternalReverb, kAudioUnitScope_Global, 0, &mReverbSetting, sizeof(mReverbSetting));			
	if (result == noErr)
        InitRenderQualityOnBusses(mRenderQuality); // distance diffusion needs to be reset on the busses now
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool OALDevice::IsValidRenderQuality (UInt32 inRenderQuality)
{	
	switch (inRenderQuality)
	{
		case ALC_SPATIAL_RENDERING_QUALITY_HIGH:
		case ALC_SPATIAL_RENDERING_QUALITY_LOW:
			return (true);
			break;
			
		default:
			return (false);
			break;
	}

	return (false);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void    OALDevice::SetDistanceAttenuation(UInt32    inBusIndex, Float64 inRefDist, Float64 inMaxDist, Float64 inRolloff)
{
    if (IsPreferredMixerAvailable())
        return;     // unnecessary with v2.0 mixer
        
    Float64     maxattenuationDB = 20 * log10(inRefDist / (inRefDist + (inRolloff * (inMaxDist - inRefDist))));
    Float64     maxattenuation = pow(10, (maxattenuationDB/20));                    
    Float64     distAttenuation = (log(1/maxattenuation))/(log(inMaxDist)) - 1.0;

#if 0
    DebugMessageN1("SetDistanceAttenuation:Reference Distance =  %f", inRefDist);
    DebugMessageN1("SetDistanceAttenuation:Maximum Distance =  %f", inMaxDist);
    DebugMessageN1("SetDistanceAttenuation:Rolloff =  %f", inRolloff);
    DebugMessageN1("SetDistanceAttenuation:Max Attenuation DB =  %f", maxattenuationDB);
    DebugMessageN1("SetDistanceAttenuation:Max Attenuation Scalar =  %f", maxattenuation);
    DebugMessageN1("SetDistanceAttenuation:distAttenuation =  %f", distAttenuation);

#endif
    
    AudioUnitSetProperty(mMixerUnit, kAudioUnitProperty_3DMixerDistanceAtten, kAudioUnitScope_Input, inBusIndex, &distAttenuation, sizeof(distAttenuation));
    return;
}
