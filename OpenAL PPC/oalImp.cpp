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
	This source file contains all the entry points into the oal state via the defined OpenAL API set. 
*/

#include "oalImp.h"
#include "oalContext.h"
#include "oalDevice.h"
#include "oalSource.h"
#include "oalBuffer.h"

// ~~~~~~~~~~~~~~~~~~~~~~
// development build flags
#define		LOG_API_USAGE	0
#define		LOG_EXTRAS		0
#define		LOG_ERRORS		0

// ~~~~~~~~~~~~~~~~~~~~~~

// AL_STATE info
const char *alVendor="Any";
const char *alVersion="OpenAL 1.2";
const char *alRenderer="Software";
const char *alExtensions="";

const char *alNoError="No Error";
const char *alErrInvalidName="Invalid Name";
const char *alErrInvalidEnum="Invalid Enum";
const char *alErrInvalidValue="Invalid Enum Value";
const char *alErrInvalidOp="Invalid Operation";
const char *alErrOutOfMemory="Out of Memory";

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// globals
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// globals
UInt32		gCurrentContext = 0;                                    // token for the current context
UInt32		gCurrentDevice = 0;                                     // token for the device of the current context
UInt32      gMaximumMixerBusCount = kDefaultMaximumMixerBusCount;   // use gMaximumMixerBusCount for settinmg the bus count when a device is opened

// At this time, only mono CBR formats would work - no problem as only pcm formats are currently valid
// The feature is turned on using ALC_CONVERT_DATA_UPON_LOADING and the alEnable()/alDisable() APIs
bool        gConvertBufferNow = false;                              // do not convert data into mixer format by default

// global object maps
OALDeviceMap*				gOALDeviceMap = NULL;					// this map will be created upon the first call to alcOpenDevice()
OALBufferMap*				gOALBufferMap = NULL;					// this map will be created upon the first call to alcGenBuffers()
OALContextMap*				gOALContextMap = NULL;					// this map will be created upon the first call to alcCreateContext()

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// utility functions
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Use this for getting a unique token when creating all new sources, contexts, devices and buffers
UInt32	GetNewToken (void)
{
	static	UInt32	currentToken = 1;
	UInt32	returnedToken;
	
	returnedToken = currentToken;
	currentToken++;
	
	return (returnedToken);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	InitializeBufferMap()
{
	if (gOALBufferMap == NULL)
	{
		gOALBufferMap = new OALBufferMap ();					// create the buffer map since there isn't one yet
		
		// add the NONE buffer
		OALBuffer	newBuffer;
		
		newBuffer.mData = NULL;
		newBuffer.mDataSize = 0;
		newBuffer.mAttachedCount = 0;
										
		gOALBufferMap->Add(0, &newBuffer);						// add the new buffer to the buffer map
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// walk through the context map and find the ones that are linked to this device
void	DeleteContextsOfThisDevice(UInt32 inDeviceToken)
{
	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

		for (UInt32	i = 0; i < gOALContextMap->Size(); i++)
		{
			UInt32		contextToken = 0;
			OALContext	*oalContext = gOALContextMap->GetContextByIndex(i, contextToken);
			
            if (oalContext == NULL)
                throw ((OSStatus) AL_INVALID_OPERATION);

			if (oalContext->GetDeviceToken() == inDeviceToken)
			{
				// delete this context, it belongs to the device that is going away
				if (contextToken == gCurrentContext)
				{
					// this context is the current context, so remove it as the current context first
					alcMakeContextCurrent(NULL);
				}
				
				delete (oalContext);
				i--; //try this index again since it was just deleted
			}
		}
	}
	catch (OSStatus     result) {
		alSetError(result);
	}
	catch (...) {
		alSetError(AL_INVALID_VALUE);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus	FillInASBD(CAStreamBasicDescription &inASBD, UInt32	inFormatID, UInt32 inSampleRate)
{
	OSStatus	err = noErr;
	
	switch (inFormatID)
	{
		case AL_FORMAT_STEREO16:
			inASBD.mSampleRate = inSampleRate * 1.0;			
			inASBD.mFormatID = kAudioFormatLinearPCM;
			inASBD.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsPacked;
			inASBD.mBytesPerPacket = 4;
			inASBD.mBytesPerFrame = 4;
			inASBD.mFramesPerPacket = 1;
			inASBD.mBitsPerChannel = 16;
			inASBD.mChannelsPerFrame = 2;
			inASBD.mReserved = 0;
			break;
		case AL_FORMAT_MONO16: 
			inASBD.mSampleRate = inSampleRate * 1.0;			
			inASBD.mFormatID = kAudioFormatLinearPCM;
			inASBD.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsPacked;
			inASBD.mBytesPerPacket = 2;
			inASBD.mBytesPerFrame = 2;
			inASBD.mFramesPerPacket = 1;
			inASBD.mBitsPerChannel = 16;
			inASBD.mChannelsPerFrame = 1;
			inASBD.mReserved = 0;
			break;
		case AL_FORMAT_STEREO8:
			inASBD.mSampleRate = inSampleRate * 1.0;			
			inASBD.mFormatID = kAudioFormatLinearPCM;
			inASBD.mFormatFlags = kAudioFormatFlagIsPacked;
			inASBD.mBytesPerPacket = 2;
			inASBD.mBytesPerFrame = 2;
			inASBD.mFramesPerPacket = 1;
			inASBD.mBitsPerChannel = 8;
			inASBD.mChannelsPerFrame = 2;
			inASBD.mReserved = 0;
			break;
		case AL_FORMAT_MONO8 : 
			inASBD.mSampleRate = inSampleRate * 1.0;			
			inASBD.mFormatID = kAudioFormatLinearPCM;
			inASBD.mFormatFlags = kAudioFormatFlagIsPacked;
			inASBD.mBytesPerPacket = 1;
			inASBD.mBytesPerFrame = 1;
			inASBD.mFramesPerPacket = 1;
			inASBD.mBitsPerChannel = 8;
			inASBD.mChannelsPerFrame = 1;
			inASBD.mReserved = 0;
			break;
		default: 
			err = AL_INVALID_VALUE;
			break;
	}
	return (err);	
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// This currently will only work for cbr formats
OSStatus ConvertDataForBuffer(OALBuffer	*inOALBuffer, void *inData, UInt32 inDataSize, UInt32	inDataFormat, UInt32 inDataSampleRate)
{
	OSStatus					result = noErr;

    try {

        AudioConverterRef			converter;
        CAStreamBasicDescription	destFormat;
        UInt32						framesOfSource = 0;

        if ((inOALBuffer == NULL) || (inData == NULL))
            throw ((OSStatus) AL_INVALID_OPERATION);
        
        result = FillInASBD(inOALBuffer->mPreConvertedDataFormat, inDataFormat, inDataSampleRate);
            THROW_RESULT

        // we only should be here for mono sounds for now...
        if (inOALBuffer->mPreConvertedDataFormat.NumberChannels() == 1)
            inOALBuffer->mPreConvertedDataFormat.mFormatFlags |= kAudioFormatFlagIsNonInterleaved; 
                    
        destFormat.mChannelsPerFrame = inOALBuffer->mPreConvertedDataFormat.NumberChannels();
        destFormat.mSampleRate = inOALBuffer->mPreConvertedDataFormat.mSampleRate;
        destFormat.mFormatID = kAudioFormatLinearPCM;
        
        if (inOALBuffer->mPreConvertedDataFormat.NumberChannels() == 1)
            destFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
        else
            destFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked; // leave stereo data interleaved, and an AC will be used for deinterleaving later on
        
        destFormat.mFramesPerPacket = 1;	
        destFormat.mBitsPerChannel = sizeof (Float32) * 8;	
        destFormat.mBytesPerPacket = sizeof (Float32) * destFormat.NumberChannels();	
        destFormat.mBytesPerFrame = sizeof (Float32) * destFormat.NumberChannels();
        
        result = FillInASBD(inOALBuffer->mDataFormat, inDataFormat, UInt32(destFormat.mSampleRate));
            THROW_RESULT
            
        result = AudioConverterNew(&inOALBuffer->mPreConvertedDataFormat, &destFormat, &converter);
            THROW_RESULT

        framesOfSource = inDataSize / inOALBuffer->mPreConvertedDataFormat.mBytesPerFrame; // THIS ONLY WORKS FOR CBR FORMATS
        
        UInt32		dataSize = framesOfSource * sizeof(Float32) * destFormat.NumberChannels();
        inOALBuffer->mDataSize = (UInt32) dataSize;

        if (inOALBuffer->mData != NULL)
        {
            if (inOALBuffer->mDataSize != dataSize)
            {
                inOALBuffer->mDataSize = dataSize;
                void *newDataPtr = realloc(inOALBuffer->mData, inOALBuffer->mDataSize);
                if (newDataPtr == NULL)
                    throw ((OSStatus) AL_INVALID_OPERATION);
                    
                inOALBuffer->mData = (char *) newDataPtr;
            }		
        }
        else
        {
            inOALBuffer->mDataSize = dataSize;
            inOALBuffer->mData = (char *) malloc (inOALBuffer->mDataSize);
            if (inOALBuffer->mData == NULL)
                throw ((OSStatus) AL_INVALID_OPERATION);
        }

        if (inOALBuffer->mData != NULL)
        {
            result = AudioConverterConvertBuffer(converter, inDataSize, inData, &inOALBuffer->mDataSize, inOALBuffer->mData);
            if (result == noErr)
            {
                inOALBuffer->mDataFormat.SetFrom(destFormat);
                if (inOALBuffer->mPreConvertedDataFormat.NumberChannels() == 1)
                    inOALBuffer->mDataHasBeenConverted = true;
                else
                    inOALBuffer->mDataHasBeenConverted = false;				
            }
        }
        
        AudioConverterDispose(converter);
    }
    catch (OSStatus     result) {
        return (result);
    }
    catch (...) {
        result = (OSStatus) AL_INVALID_OPERATION;
    }
    
	return (result);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ALC Methods
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** ALC - METHODS *****

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Device APIs
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Devices *****

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALCdevice* ALCAPIENTRY alcOpenDevice(ALCubyte *deviceName)
{
	UInt32		newDeviceToken = 0;
	OALDevice	*newDevice = NULL;

#if LOG_API_USAGE
	DebugMessage("alcOpenDevice called");
#endif
	
	try {
		if (gOALDeviceMap == NULL)
			gOALDeviceMap = new OALDeviceMap ();                                        // create the device map if there isn't one yet
		        
        newDeviceToken = GetNewToken();                                                 // get a unique token
        newDevice = new OALDevice((const char *) deviceName, newDeviceToken, gMaximumMixerBusCount);	// create a new device object
        if (newDevice != NULL)
            gOALDeviceMap->Add(newDeviceToken, &newDevice);                             // add the new device to the device map
	}
	catch (OSStatus result) {
		DebugMessageN1("alcOpenDevice Failed - err = %ld\n", result);
		if (newDevice) 
			delete (newDevice);
		newDeviceToken = 0;
		alSetError(result);
    }
	catch (...) {
		DebugMessage("alcOpenDevice Failed");
		if (newDevice) 
			delete (newDevice);
		newDeviceToken = 0;
		alSetError(AL_INVALID_OPERATION);
	}
	
	return ((ALCdevice *) newDeviceToken);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALCvoid    ALCAPIENTRY alcCloseDevice(ALCdevice *device)
{
#if LOG_API_USAGE
	DebugMessage("alcCloseDevice called");
#endif

	try {										
        if (gOALDeviceMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
            
        OALDevice		*oalDevice = gOALDeviceMap->Get((UInt32) device);	// get the requested oal device
        if (oalDevice == NULL) 
            throw ((OSStatus) AL_INVALID_VALUE);                            // make sure it is a valid device token

        gOALDeviceMap->Remove((UInt32) device);                             // remove the device from the map
        
        DeleteContextsOfThisDevice((UInt32) device);
        delete (oalDevice);                                                 // destruct the device object
        
        if (gOALDeviceMap->Empty())
        {
            // there are no more devices in the map, so delete the map and create again later if needed
            delete (gOALDeviceMap);
            gOALDeviceMap = NULL;
        }
	}
	catch (OSStatus   result) {
		DebugMessageN1("alcCloseDevice Failed - err = %ld\n", result);
        alSetError(result);
	}
    catch (...) {
		DebugMessage("alcCloseDevice Failed");
        alSetError(AL_INVALID_OPERATION);
	}

	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALCenum  ALCAPIENTRY alcGetError(ALCdevice *device)
{
#if LOG_API_USAGE
	DebugMessage("alcGetError called");
#endif
	// NOTE: Ignore device parameter for now as this has been the common implementation so far
	return (alGetError ());
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Context APIs
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Contexts *****

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// There is no attribute support yet
ALCAPI ALCcontext* 	ALCAPIENTRY alcCreateContext(ALCdevice *device,	ALCint *attrList)
{
	UInt32			newContextToken = 0;
	OALContext		*newContext = NULL;

#if LOG_API_USAGE
	DebugMessage("alcCreateContext called");
#endif
	
	try {
        if (gOALDeviceMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
            
        OALDevice		*oalDevice = gOALDeviceMap->Get((UInt32) device);
        if (oalDevice == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);           // invalid device has been passed

		// create the context map if there isn't one yet
		if (gOALContextMap == NULL)
			gOALContextMap = new OALContextMap();
	
		newContextToken = GetNewToken();
		newContext = new OALContext(newContextToken, oalDevice);
		
		gOALContextMap->Add(newContextToken, &newContext);	
	}
	catch (OSStatus     result){
		DebugMessageN1("alcCreateContext Failed - err = %ld\n", result);
		if (newContext) 
            delete (newContext);
		alSetError(result);
		return (NULL);
	}
    catch (...) {
		DebugMessage("alcCreateContext Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return ((ALCcontext *) newContextToken);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALCboolean  ALCAPIENTRY alcMakeContextCurrent(ALCcontext *context)
{
#if LOG_API_USAGE
	DebugMessage("alcMakeContextCurrent called");
#endif

	if ((UInt32 ) context == gCurrentContext)
		return AL_TRUE;								// no change necessary, already using this context

	try {	
        if (gOALDeviceMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

		OALContext		*currentContext = NULL;
		// get the current context if there is one
		if (gCurrentContext != 0)
			currentContext = gOALContextMap->Get(gCurrentContext);
			
		if (context == 0)
		{
			// caller passed NULL, which means no context should be current
			gCurrentDevice = 0;
			gCurrentContext = 0;
			// disable the current context if there is one
			if (currentContext != NULL)
				currentContext->DisableContext();
		}
		else
		{
			// find the device that owns this context
			UInt32		newCurrentDeviceToken = gOALContextMap->GetDeviceTokenForContext((UInt32) context);
			if (newCurrentDeviceToken != 0)
			{
				OALContext		*oalContext = gOALContextMap->Get((UInt32) context);
				if (oalContext == NULL)
                    throw ((OSStatus) AL_INVALID_VALUE);    // fail because the context is invalid

                // new context is obtained so disable the old one now
                if (currentContext)
                    currentContext->DisableContext();
            
                // store the new current context and device
                gCurrentDevice = newCurrentDeviceToken;
                gCurrentContext = (UInt32) context;
			}
		}
		
		return AL_TRUE;
	}
	catch (OSStatus result) {
		DebugMessageN1("alcMakeContextCurrent Failed - err = %ld\n", result);
        alSetError(result);
	}
    catch (...) {
		DebugMessage("alcMakeContextCurrent Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return AL_FALSE;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALCvoid	  ALCAPIENTRY alcProcessContext(ALCcontext *context)
{
#if LOG_API_USAGE
	DebugMessage("alcProcessContext called");
#endif

	try {
        if (gOALDeviceMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALContext		*oalContext = gOALContextMap->Get((UInt32) context);
        if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
        
        oalContext->ProcessContext();
    }
    catch (OSStatus result) {
		DebugMessageN1("alcProcessContext Failed - err = %ld\n", result);
        alSetError(result);        
    }
    catch (...) {
		DebugMessage("alcProcessContext Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
    return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALCcontext* ALCAPIENTRY alcGetCurrentContext(ALCvoid)
{
#if LOG_API_USAGE
	DebugMessage("alcGetCurrentContext called");
#endif

	return ((ALCcontext *) gCurrentContext);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// find out what device the context uses
ALCAPI ALCdevice*  ALCAPIENTRY alcGetContextsDevice(ALCcontext *context)
{
	UInt32	returnValue = 0;

#if LOG_API_USAGE
	DebugMessage("alcGetContextsDevice called");
#endif
	
	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
        
        returnValue = gOALContextMap->GetDeviceTokenForContext((UInt32) context);
    }
    catch (OSStatus result) {
		DebugMessageN1("alcGetContextsDevice Failed - err = %ld\n", result);
        alSetError(result);
    }
    catch (...) {
		DebugMessage("alcGetContextsDevice Failed");
        alSetError(AL_INVALID_OPERATION);
	}
    
	return ((ALCdevice*) returnValue);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALCvoid	  ALCAPIENTRY alcSuspendContext(ALCcontext *context)
{
#if LOG_API_USAGE
	DebugMessage("alcSuspendContext called");
#endif

    try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
        
        OALContext		*oalContext = gOALContextMap->Get((UInt32) context);
        if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);
            
        oalContext->SuspendContext();
    }
    catch (OSStatus     result) {
		DebugMessageN1("alcSuspendContext Failed - err = %ld\n", result);
        alSetError(result);
    }
    catch (...) {
		DebugMessage("alcSuspendContext Failed");
        alSetError(AL_INVALID_OPERATION);
	}
    
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALCvoid	ALCAPIENTRY alcDestroyContext (ALCcontext *context)
{
#if LOG_API_USAGE
	DebugMessage("alcDestroyContext called");
#endif

	try {
		if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
        
        // make sure it is a valid device token
		OALContext		*oalContext = gOALContextMap->Get((UInt32) context);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);
            
        // if this was the current context, set current context to NULL
        if (gCurrentContext == (UInt32) context)
            alcMakeContextCurrent(NULL);

        gOALContextMap->Remove((UInt32) context);	// remove from the map
	}
	catch (OSStatus     result) {
		DebugMessageN1("alcDestroyContext Failed - err = %ld\n", result);
        alSetError (result);
	}
    catch (...) {
		DebugMessage("alcDestroyContext Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Other APIs
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Other *****

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALCubyte*   ALCAPIENTRY alcGetString(ALCdevice *device, ALCenum param)
{
#if LOG_API_USAGE
	DebugMessage("alcGetString called");
#endif

	switch (param)
	{
		default:
			break;
	}
	return NULL;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALCvoid     ALCAPIENTRY alcGetIntegerv(ALCdevice *device,ALCenum param,ALCsizei size,ALCint *data)
{	
#if LOG_API_USAGE
	DebugMessage("alcGetIntegerv called");
#endif

	switch (param)
	{
		default:
			break;
	}

	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALboolean  ALCAPIENTRY alcIsExtensionPresent(ALCdevice *device, ALubyte *extName)
{
#if LOG_API_USAGE
	DebugMessage("alcIsExtensionPresent called");
#endif

	return AL_FALSE;    // no extensions present in this implementation
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALvoid  *  ALCAPIENTRY alcGetProcAddress(ALCdevice *device, ALubyte *funcName)
{
#if LOG_API_USAGE
	DebugMessage("alcGetProcAddress called");
#endif

	return 0;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALenum  ALCAPIENTRY alcGetEnumValue(ALCdevice *device, ALubyte *enumName)
{
#if LOG_API_USAGE
	DebugMessage("alcGetEnumValue called");
#endif

	// since the device name is ignored by always using the default output AU,
    // alGetEnumValue() should return the same thing
    return (alGetEnumValue (enumName));
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGetIntegerv (ALenum pname, ALint *data)
{
#if LOG_API_USAGE
	DebugMessage("alGetIntegerv called");
#endif

	try {
		if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

		OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        switch (pname)
        {
            case AL_DISTANCE_MODEL:
                *data = oalContext->GetDistanceModel();
                break;
                
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus     result) {
		DebugMessageN1("alGetIntegerv Failed - err = %ld\n", result);
        alSetError(result);
    }
    catch (...) {
		DebugMessage("alGetIntegerv Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// AL Methods
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** AL - METHODS *****

UInt32		gCurrentError = 0;				// globally stored error code

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALenum ALAPIENTRY alGetError ()
{
    ALenum	error = AL_NO_ERROR;

#if LOG_API_USAGE
	DebugMessage("alGetError called");
#endif

	if (gCurrentError != AL_NO_ERROR)
    {
#if LOG_ERRORS
		DebugMessageN1("alGetError: error = %ld\n", gCurrentError);
#endif
		error = gCurrentError;
		gCurrentError = AL_NO_ERROR;
	}
	
	return (error);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSetError (ALenum errorCode)
{
#if LOG_API_USAGE
	DebugMessage("alSetError called");
#endif

#if LOG_ERRORS
	DebugMessageN1("alSetError called: error #  %ld\n", errorCode);
#endif

    // only set an error if we are in a no error state
    if (gCurrentError == AL_NO_ERROR)
		gCurrentError = errorCode;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Buffer APIs
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Buffers *****

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGenBuffers(ALsizei n, ALuint *buffers)
{
#if LOG_API_USAGE
	DebugMessage("alGenBuffers called");
#endif

	UInt32		newBufferToken = 0;
	
	try {
        InitializeBufferMap();
        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
		if ((n + gOALBufferMap->Size() > AL_MAXBUFFERS) || (n > AL_MAXBUFFERS))
            throw ((OSStatus) AL_INVALID_VALUE);
		
        for (UInt32	i = 0; i < n; i++)
        {
            OALBuffer	newBuffer;
            
            // initialize some of the fields, others will be explicitly set later
            newBuffer.mData = NULL;
            newBuffer.mDataSize = 0;
            newBuffer.mAttachedCount = 0;
            newBuffer.mDataHasBeenConverted = false;
            
            newBufferToken = GetNewToken(); 						// get a unique token
                                    
            gOALBufferMap->Add(newBufferToken, &newBuffer);			// add the new buffer to the buffer map
            buffers[i] = newBufferToken;
        }
	}
	catch (OSStatus     result) {
		DebugMessageN1("alGenBuffers Failed - err = %ld\n", result);
        alSetError (result);
	}
    catch (...) {
		DebugMessage("alGenBuffers Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGetBufferf (ALuint buffer, ALenum pname, ALfloat *value)
{
#if LOG_API_USAGE
	DebugMessage("alGetBufferf called");
#endif

    try {
        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALBuffer	*oalBuffer = gOALBufferMap->Get((UInt32) buffer);
        if (oalBuffer == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);
        
        switch (pname)
        {
            case AL_FREQUENCY:
                *value = oalBuffer->mDataFormat.mSampleRate;
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
    }
    catch (OSStatus result) {
		DebugMessageN1("alGetBufferf Failed - err = %ld\n", result);
        alSetError(result);
    }
    catch (...) {
		DebugMessage("alGetBufferf Failed");
        alSetError(AL_INVALID_OPERATION);
	}
    
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGetBufferi(ALuint buffer, ALenum pname, ALint *value)
{
#if LOG_API_USAGE
	DebugMessage("alGetBufferi called");
#endif

    try {
        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALBuffer	*oalBuffer = gOALBufferMap->Get((UInt32) buffer);
        if (oalBuffer == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);

        switch (pname)
        {
            case AL_FREQUENCY:
                *value = (UInt32) oalBuffer->mDataFormat.mSampleRate;
                break;
            case AL_BITS:
                *value = oalBuffer->mPreConvertedDataFormat.mBitsPerChannel;
                break;
            case AL_CHANNELS:
                *value = oalBuffer->mDataFormat.NumberChannels();
                break;
            case AL_SIZE:
                *value = oalBuffer->mPreConvertedDataSize;
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }        
    }
    catch (OSStatus result) {
		DebugMessageN1("alGetBufferi Failed - err = %ld\n", result);
		alSetError(result);
    }
    catch (...) {
		DebugMessage("alGetBufferi Failed");
        alSetError(AL_INVALID_OPERATION);
	}
    
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alDeleteBuffers(ALsizei n, ALuint *buffers)
{
#if LOG_API_USAGE
	DebugMessage("alDeleteBuffers called");
#endif

	try {
        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);   

        if (gOALBufferMap->Empty())
            return; // nothing to do
            
        if (n > gOALBufferMap->Size() - 1)
        {
            throw ((OSStatus) AL_INVALID_VALUE);   
        }
        else
        {
            UInt32	i;
            // see if any of the buffers are attached to a source or are invalid names
            for (i = 0; i < n; i++)
            {
                // don't bother verifying the NONE buffer, it won't be deleted anyway
                if (buffers[i] != AL_NONE)
                {
                    OALBuffer	*oalBuffer = gOALBufferMap->Get(buffers[i]);
                    if (oalBuffer == NULL)
                    {
                        throw ((OSStatus) AL_INVALID_VALUE);    // the buffer is invalid
                    }
                    else if (oalBuffer->mAttachedCount > 0)
                    {
                        throw ((OSStatus) AL_INVALID_VALUE);    // the buffer is attached to a source so set an error and bail
                    }
                }
            }
        
            // All the buffers are OK'd for deletion, so delete them now
            for (i = 0; i < n; i++)
            {
                // do not delete the NONE buffer at the beginning of the map
                if (buffers[i] != AL_NONE)
                {
                    OALBuffer	*buffer = gOALBufferMap->Get((UInt32) buffers[i]);
                    if (buffer != NULL)
                    {
   	                	if (buffer->mData != NULL)
                        	free(buffer->mData);
                    }
	            	gOALBufferMap->Remove((UInt32) buffers[i]);
	            }
            }
        }
    }
    catch (OSStatus     result) {
		DebugMessageN1("alDeleteBuffers Failed - err = %ld\n", result);
        alSetError(result);
    }
    catch (...) {
		DebugMessage("alDeleteBuffers Failed");
        alSetError(AL_INVALID_OPERATION);
	}
    
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alBufferData(ALuint buffer, ALenum format, ALvoid *data, ALsizei size, ALsizei freq)
{
#if LOG_API_USAGE
	DebugMessage("alBufferData called");
#endif

	try {
        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);   

        OALBuffer	*oalBuffer = gOALBufferMap->Get((UInt32) buffer);
        if ((oalBuffer == NULL) || !IsFormatSupported(format))
            throw ((OSStatus) AL_INVALID_VALUE);   // this is not a valid buffer token or is an invalid format
	
        // don't allow if the buffer is in a queue
        if (oalBuffer->mAttachedCount > 0)
        {
            DebugMessage("alBufferData ATTACHMENT > 0");
            throw ((OSStatus) AL_INVALID_OPERATION);   
        }
		
        oalBuffer->mPreConvertedDataSize = (UInt32) size;
        // do not pre-convert stereo sounds, let the AC do the deinterleaving	
        OSStatus    result = noErr;
        if (!gConvertBufferNow || ((format == AL_FORMAT_STEREO16) || (format == AL_FORMAT_STEREO8)))
        {
            if (oalBuffer->mData != NULL)
            {
                if (oalBuffer->mDataSize != (UInt32) size)
                {
                    oalBuffer->mDataSize = (UInt32) size;
                    void *newDataPtr = realloc(oalBuffer->mData, oalBuffer->mDataSize);
                    oalBuffer->mData = (char *) newDataPtr;
                }		
            }
            else
            {
                oalBuffer->mDataSize = (UInt32) size;
                oalBuffer->mData = (char *) malloc (oalBuffer->mDataSize);
            }
            
            if (oalBuffer->mData)
            {
                result = FillInASBD(oalBuffer->mDataFormat, format, freq);
                    THROW_RESULT
                
                oalBuffer->mPreConvertedDataFormat.SetFrom(oalBuffer->mDataFormat); //  make sure they are the same so original format info can be returned to caller
                memcpy (oalBuffer->mData, data, oalBuffer->mDataSize);		
            }
        }
        else
        {
    #if LOG_EXTRAS		
        DebugMessage("alBufferData called: Converting Data Now");
    #endif
            result = ConvertDataForBuffer(oalBuffer, data, size, format, freq);	// convert the data to the mixer's format and copy to the buffer
                THROW_RESULT
        }
    }
    catch (OSStatus     result) {
		DebugMessageN1("alBufferData Failed - err = %ld\n", result);
        alSetError(result);
    }
    catch (...) {
		DebugMessage("alBufferData Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALboolean ALAPIENTRY alIsBuffer(ALuint buffer)
{
#if LOG_API_USAGE
	DebugMessage("alIsBuffer called");
#endif

    try {
        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);   

        OALBuffer	*oalBuffer = gOALBufferMap->Get((UInt32) buffer);

        if (oalBuffer != NULL)
            return AL_TRUE;
        else
            return AL_FALSE;
    }
    catch (OSStatus     result) {
		DebugMessageN1("alIsBuffer Failed - err = %ld\n", result);
        alSetError(result);
    }
    catch (...) {
		DebugMessage("alIsBuffer Failed");
        alSetError(AL_INVALID_OPERATION);
	}

    return AL_FALSE;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Source APIs
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** Sources *****

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALvoid  ALAPIENTRY alGenSources(ALsizei n, ALuint *sources)
{
#if LOG_API_USAGE
	DebugMessage("alGenSources called");
#endif

	if (n == 0)
		return; // NOP

	UInt32      i = 0,
                count = 0;
	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        if ((n > AL_MAXSOURCES) || (sources == NULL))
            throw ((OSStatus) AL_INVALID_VALUE);
        
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        for (i = 0; i < n; i++)
        {
            UInt32	newToken = GetNewToken();		// get a unique token
            
            oalContext->AddSource(newToken);		// add this source to the context
            sources[i] = newToken; 					// return the source token
            count++;
        }
	}
	catch (OSStatus     result){
		DebugMessageN1("alGenSources Failed - err = %ld\n", result);
		// some of the sources could not be created, so delete the ones that were and return none
		alSetError(result);
		alDeleteSources(i, sources);
		for (i = 0; i < count; i++)
			sources[i] = 0;
	}
    catch (...) {
		DebugMessage("alGenSources Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALvoid ALAPIENTRY alDeleteSources (ALsizei n, ALuint *sources)
{
#if LOG_API_USAGE
	DebugMessage("alDeleteSources called");
#endif

	if (n == 0)
		return; // NOP

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

		OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        if (n > oalContext->GetSourceCount())
            throw ((OSStatus) AL_INVALID_VALUE);
        
        for (UInt32 i = 0; i < n; i++)
        {
            oalContext->RemoveSource(sources[i]);
        }
	}
	catch (OSStatus     result) {
		DebugMessageN1("alDeleteSources Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alDeleteSources Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALCAPI ALboolean  ALAPIENTRY alIsSource(ALuint source)
{
#if LOG_API_USAGE
	DebugMessage("alIsSource called");
#endif

	ALboolean	returnValue = AL_FALSE;
	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

		OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
        if (oalSource != NULL)
            return AL_TRUE;
        else
            return AL_FALSE;
	}
	catch (OSStatus     result) {
		DebugMessageN1("alIsSource Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alIsSource Failed");
        alSetError(AL_INVALID_OPERATION);
	}

	return (returnValue);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSourcef (ALuint source, ALenum pname, ALfloat value)
{
#if LOG_API_USAGE
	DebugMessage("alSourcef called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

		OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
		if (oalSource == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);

        switch (pname) 
        {
            case AL_CONE_INNER_ANGLE:
                oalSource->SetConeInnerAngle(value);
                break;
            case AL_CONE_OUTER_ANGLE:
                oalSource->SetConeOuterAngle(value);
                break;
            case AL_CONE_OUTER_GAIN:
                if (value >= 0.0 && value <= 1.0)
                    oalSource->SetConeOuterGain(value);
                break;
            case AL_PITCH:
                if (value < 0.0f)
                    throw ((OSStatus) AL_INVALID_VALUE);
                oalSource->SetPitch(value);
                break;
            case AL_GAIN:
                oalSource->SetGain(value);
                break;
            case AL_MAX_DISTANCE:
                oalSource->SetMaxDistance(value);
                break;
            case AL_MIN_GAIN:
                if ((value < 0.0f) && (value > 1.0f))
                    throw ((OSStatus) AL_INVALID_VALUE);
                oalSource->SetMinGain(value);
                break;
            case AL_MAX_GAIN:
                if ((value < 0.0f) && (value > 1.0f))
                    throw ((OSStatus) AL_INVALID_VALUE);
                oalSource->SetMaxGain(value);
                break;
            case AL_ROLLOFF_FACTOR:
                if (value < 0.0f) 
                    throw ((OSStatus) AL_INVALID_VALUE);
                oalSource->SetRollOffFactor(value);
                break;
            case AL_REFERENCE_DISTANCE:
                if (value <= 0.0f)
                    throw ((OSStatus) AL_INVALID_VALUE);
                oalSource->SetReferenceDistance(value);
                break;
            default:
                alSetError(AL_INVALID_OPERATION);
                break;
        }
	}
	catch (OSStatus     result) {
		DebugMessageN1("alSourcef Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alSourcef Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSourcefv (ALuint source, ALenum pname, ALfloat *values)
{
#if LOG_API_USAGE
	DebugMessage("alSourcefv called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
		if (oalSource == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);

        switch(pname) 
        {
            case AL_POSITION:
                if (isnan(values[0]) || isnan(values[1]) || isnan(values[2]))
                    throw ((OSStatus) AL_INVALID_VALUE);
                oalSource->SetPosition(values[0], values[1], values[2]);
                break;
            case AL_DIRECTION:
                oalSource->SetDirection(values[0], values[1], values[2]);
                break;
            case AL_VELOCITY:
                oalSource->SetVelocity(values[0], values[1], values[2]);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch(OSStatus      result) {
		DebugMessageN1("alSourcefv Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alSourcefv Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSource3f (ALuint source, ALenum pname, ALfloat v1, ALfloat v2, ALfloat v3)
{
#if LOG_API_USAGE
	DebugMessage("alSource3f called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
		if (oalSource == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);

        switch (pname) 
        {
            case AL_POSITION:
                if (isnan(v1) || isnan(v2) || isnan(v3))
                    throw ((OSStatus) AL_INVALID_VALUE);
                oalSource->SetPosition(v1, v2, v3);
                break;
            case AL_VELOCITY:
                oalSource->SetVelocity(v1, v2, v3);
                break;
            case AL_DIRECTION:
                oalSource->SetDirection(v1, v2, v3);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN1("alSource3f Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alSource3f Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSourcei (ALuint source, ALenum pname, ALint value)
{
#if LOG_API_USAGE
	DebugMessage("alSourcei called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
		if (oalSource == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);

        switch (pname) 
        {
            case AL_CONE_INNER_ANGLE:
                oalSource->SetConeInnerAngle(value);
                break;
            case AL_CONE_OUTER_ANGLE:
                oalSource->SetConeOuterAngle(value);
                break;
            case AL_CONE_OUTER_GAIN:
                if (value >= 0 && value <= 1)
                    oalSource->SetConeOuterGain(value);
                break;
            case AL_LOOPING:
                oalSource->SetLooping(value);
                break;
            case AL_BUFFER:
                if ((oalSource->GetState() == AL_STOPPED) || (oalSource->GetState() == AL_INITIAL))
                {
                    if (gOALBufferMap == NULL)
                        throw ((OSStatus) AL_INVALID_OPERATION);   

                    if ((alIsBuffer(value)) || (value == NULL))
                        oalSource->SetBuffer(value, gOALBufferMap->Get((UInt32) value));
                    else
                        throw ((OSStatus) AL_INVALID_OPERATION);
                } 
                else
                    throw ((OSStatus) AL_INVALID_OPERATION);
    
                break;
            case AL_SOURCE_RELATIVE:
                if ((value == AL_FALSE) || (value == AL_TRUE))
                    oalSource->SetSourceRelative(value);
                else
                    throw ((OSStatus) AL_INVALID_VALUE);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN1("alSourcei Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alSourcei Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGetSourcef (ALuint source, ALenum pname, ALfloat *value)
{
#if LOG_API_USAGE
	DebugMessage("alGetSourcef called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
		if (oalSource == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);

        switch (pname) 
        {
            case AL_CONE_INNER_ANGLE:
                *value = oalSource->GetConeInnerAngle();
                break;
            case AL_CONE_OUTER_ANGLE:
                *value = oalSource->GetConeOuterAngle();
                break;
            case AL_CONE_OUTER_GAIN:
                *value = oalSource->GetConeOuterGain();
                break;
            case AL_PITCH:
                *value = oalSource->GetPitch();
                break;
            case AL_GAIN:
                *value = oalSource->GetGain();
                break;
            case AL_MAX_DISTANCE:
                *value = oalSource->GetMaxDistance();
                break;
            case AL_MIN_GAIN:
                *value = oalSource->GetMinGain();
                break;
            case AL_MAX_GAIN:
                *value = oalSource->GetMaxGain();
                break;
            case AL_ROLLOFF_FACTOR:
                *value = oalSource->GetRollOffFactor();
                break;
            case AL_REFERENCE_DISTANCE:
                *value = oalSource->GetReferenceDistance();
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN1("alGetSourcef Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alGetSourcef Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGetSource3f (ALuint source, ALenum pname, ALfloat *v1, ALfloat *v2, ALfloat *v3)
{
#if LOG_API_USAGE
	DebugMessage("alGetSource3f called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
		if (oalSource == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);

        switch (pname) 
        {
            case AL_POSITION:
                oalSource->GetPosition(*v1, *v2, *v3);
                break;
            case AL_DIRECTION:
                oalSource->GetDirection(*v1, *v2, *v3);
                break;
            case AL_VELOCITY:
                oalSource->GetVelocity(*v1, *v2, *v3);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN1("alGetSource3f Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alGetSource3f Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGetSourcefv (ALuint source, ALenum pname, ALfloat *values)
{
#if LOG_API_USAGE
	DebugMessage("alGetSourcefv called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
		if (oalSource == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);

        switch(pname) 
        {
            case AL_POSITION:
                oalSource->GetPosition(values[0], values[1], values[2]);
                break;
            case AL_VELOCITY:
                oalSource->GetVelocity(values[0], values[1], values[2]);
                break;
            case AL_DIRECTION:
                oalSource->GetDirection(values[0], values[1], values[2]);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN1("alGetSourcefv Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alGetSourcefv Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGetSourcei (ALuint source, ALenum pname, ALint *value)
{
#if LOG_API_USAGE
	DebugMessage("alGetSourcei called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
		if (oalSource == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);

        switch (pname) 
        {
            case AL_CONE_INNER_ANGLE:
                *value = (UInt32) oalSource->GetConeInnerAngle();
                break;
            case AL_CONE_OUTER_ANGLE:
                *value = (UInt32) oalSource->GetConeOuterAngle();
                break;
            case AL_CONE_OUTER_GAIN:
                *value = (UInt32) oalSource->GetConeOuterGain();
                break;
            case AL_LOOPING:
                *value = oalSource->GetLooping();
                break;
            case AL_BUFFER:
                *value = oalSource->GetBuffer();
                break;
            case AL_SOURCE_RELATIVE:
                *value = oalSource->GetSourceRelative();
                break;
            case AL_SOURCE_STATE:
                *value = oalSource->GetState();
                break;
            case AL_BUFFERS_QUEUED:
                *value = oalSource->GetQLength();
                break;
            case AL_BUFFERS_PROCESSED:
                *value = oalSource->BuffersProcessed();
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus      result) {
		DebugMessageN1("alGetSourcei Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alGetSourcei Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSourcePlay(ALuint source)
{	
#if LOG_API_USAGE
	DebugMessage("alSourcePlay called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
		if (oalSource == NULL)
            throw ((OSStatus) AL_INVALID_NAME);

        if (oalSource->GetQLength() != 0)
        {
            UInt32		state = oalSource->GetState();
            switch (state)
            {
                case AL_INITIAL:
                    oalSource->Play();					// start playing the queue
                    break;
                case AL_PLAYING:
                    oalSource->RewindToStart();			// rewind the buffer queue and continue playing form the beginning of the 1st buffer
                    break;
                case AL_PAUSED:
                    oalSource->Resume();				// continue playing again from somewhere in the buffer queue
                    break;
                case AL_STOPPED:
                    oalSource->Reset();					// reset the buffer queue and mark state as AL_INITIAL
                    oalSource->Play();					// start playing the queue
                    break;
                default:
                    break;
            }
        }		
	}
	catch (OSStatus      result) {
		DebugMessageN1("alSourcePlay Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alSourcePlay Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSourcePause (ALuint source)
{
#if LOG_API_USAGE
	DebugMessage("alSourcePause called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
		if (oalSource == NULL)
            throw ((OSStatus) AL_INVALID_NAME);

        oalSource->Pause();
	}
	catch (OSStatus      result) {
		DebugMessageN1("alSourcePause Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alSourcePause Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSourceStop (ALuint source)
{
#if LOG_API_USAGE
	DebugMessage("alSourceStop called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
		if (oalSource == NULL)
            throw ((OSStatus) AL_INVALID_NAME);

        oalSource->Stop();
	}
	catch (OSStatus      result) {
		DebugMessageN1("alSourceStop Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alSourceStop Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSourceRewind (ALuint source)
{
#if LOG_API_USAGE
	DebugMessage("alSourceRewind called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
		if (oalSource == NULL)
            throw ((OSStatus) AL_INVALID_NAME);

        oalSource->RewindToStart();
	}
	catch (OSStatus      result) {
		DebugMessageN1("alSourceRewind Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alSourceRewind Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSourcePlayv(ALsizei n, ALuint *ID)
{
#if LOG_API_USAGE
	DebugMessage("alSourcePlayv called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        for (UInt32	i = 0; i < n; i++)
            alSourcePlay(ID[i]);
    }
	catch (OSStatus      result) {
		DebugMessageN1("alSourcePlayv Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alSourcePlayv Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSourcePausev(ALsizei n, ALuint *ID)
{
#if LOG_API_USAGE
	DebugMessage("alSourcePausev called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        for (UInt32	i = 0; i < n; i++)
            alSourcePause(ID[i]);
    }
	catch (OSStatus      result) {
		DebugMessageN1("alSourcePausev Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alSourcePausev Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSourceStopv(ALsizei n, ALuint *ID)
{
#if LOG_API_USAGE
	DebugMessage("alSourceStopv called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        for (UInt32	i = 0; i < n; i++)
            alSourceStop(ID[i]);
    }
	catch (OSStatus      result) {
		DebugMessageN1("alSourceStopv Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alSourceStopv Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSourceRewindv (ALsizei n, ALuint *ID)
{
#if LOG_API_USAGE
	DebugMessage("alSourceRewindv called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        for (UInt32	i = 0; i < n; i++)
            alSourceRewind(ID[i]);
    }
	catch (OSStatus      result) {
		DebugMessageN1("alSourceRewindv Failed - err = %ld\n", result);
		alSetError(result);
	}
    catch (...) {
		DebugMessage("alSourceRewindv Failed");
        alSetError(AL_INVALID_OPERATION);
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSourceQueueBuffers (ALuint source, ALsizei n, ALuint *buffers)
{
#if LOG_API_USAGE
	DebugMessage("alSourceQueueBuffers called");
#endif

	if (n == 0)
		return;	// no buffers were actually requested for queueing
		
	try {
        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);   

        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
		if (oalSource == NULL)
            throw ((OSStatus) AL_INVALID_NAME);
                        
        // verify that buffers provided are valid before queueing them.
        for (UInt32	i = 0; i < n; i++)
        {
            if (buffers[i] != AL_NONE)
            {
                // verify that this is a valid buffer
                OALBuffer *oalBuffer = gOALBufferMap->Get(buffers[i]);
                if (oalBuffer == NULL)
                    throw ((OSStatus) AL_INVALID_VALUE);				// an invalid buffer id has been provided
            }
            else
            {
                // NONE is a valid buffer parameter - do nothing it's valid
            }
        }

        // all valid buffers, so append them to the queue
        for (UInt32	i = 0; i < n; i++)
        {
            oalSource->AppendBufferToQueue(buffers[i], gOALBufferMap->Get(buffers[i]));
        }
	}
	catch (OSStatus		result) {
		DebugMessageN1("alSourceQueueBuffers Failed - err = %ld\n", result);
		alSetError(result);
	}
	catch (...) {
		DebugMessage("alSourceQueueBuffers Failed");
        alSetError(AL_INVALID_OPERATION);
	}

	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSourceUnqueueBuffers (ALuint source, ALsizei n, ALuint *buffers)
{
#if LOG_API_USAGE
	DebugMessage("alSourceUnqueueBuffers called");
#endif

	if (n == 0)
		return;

	try {
        if (buffers == NULL)
            throw ((OSStatus) AL_INVALID_VALUE);   

        if (gOALBufferMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);   

        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
		
        OALContext		*oalContext = gOALContextMap->Get((UInt32) gCurrentContext);
		if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALSource	*oalSource = oalContext->GetSource((UInt32) source);
		if (oalSource == NULL)
            throw ((OSStatus) AL_INVALID_NAME);

        if (oalSource->GetQLength() < n)
            throw (OSStatus) AL_INVALID_VALUE;				// n is greater than the source's Q length
        
        oalSource->RemoveBuffersFromQueue(n, (UInt32 *) buffers);

	}
	catch (OSStatus		result) {
		DebugMessageN1("alSourceUnqueueBuffers Failed - err = %ld\n", result);
		// reinitialize the elements in the buffers array
		if (buffers)
		{
			for (UInt32	i = 0; i < n; i++)
				buffers[i] = 0;
		}
		// this would be real bad, as now we have a buffer queue in an unknown state
		alSetError(result);
	}
	catch (...){
		DebugMessage("alSourceUnqueueBuffers Failed");
        alSetError(AL_INVALID_OPERATION);
	}

	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY	alDistanceModel (ALenum value)
{
#if LOG_API_USAGE
	DebugMessage("alDistanceModel called");
#endif

	try {
        switch (value)
        {
            case AL_NONE:			
            case AL_INVERSE_DISTANCE:
            case AL_INVERSE_DISTANCE_CLAMPED:
            {
                if (gOALContextMap == NULL)
                    throw ((OSStatus) AL_INVALID_OPERATION);
                    
                OALContext		*oalContext = gOALContextMap->Get(gCurrentContext);
                if (oalContext == NULL)
                    throw ((OSStatus) AL_INVALID_OPERATION);

                oalContext->SetDistanceModel(value);
            } 
            break;
            
            default:
                alSetError(AL_INVALID_VALUE);
                break;
        }
    }
	catch (OSStatus		result) {
		DebugMessageN1("alDistanceModel Failed - err = %ld\n", result);
        alSetError(result);
    }
    catch (...) {
		DebugMessage("alDistanceModel Failed");
        alSetError(AL_INVALID_OPERATION);   // by default
    }
    return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alDopplerFactor (ALfloat value)
{	
#if LOG_API_USAGE
	DebugMessage("alDopplerFactor called");
#endif

	try {
        if (value < 0.0f)
            throw ((OSStatus) AL_INVALID_VALUE);

        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
            
        OALContext		*oalContext = gOALContextMap->Get(gCurrentContext);
        if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        oalContext->SetDopplerFactor(value);
	}
	catch (OSStatus		result) {
		DebugMessageN1("alDopplerFactor Failed - err = %ld\n", result);
        alSetError(result);
    }
    catch (...) {
		DebugMessage("alDopplerFactor Failed");
        alSetError(AL_INVALID_OPERATION);   // by default
    }
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alDopplerVelocity (ALfloat value)
{
#if LOG_API_USAGE
	DebugMessage("alDopplerVelocity called");
#endif

	try {
        if (value < 0.0f)
            throw ((OSStatus) AL_INVALID_VALUE);

        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
            
        OALContext		*oalContext = gOALContextMap->Get(gCurrentContext);
        if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        oalContext->SetDopplerVelocity(value);
	}
	catch (OSStatus		result) {
		DebugMessageN1("alDopplerVelocity Failed - err = %ld\n", result);
        alSetError(result);
    }
    catch (...) {
		DebugMessage("alDopplerVelocity Failed");
        alSetError(AL_INVALID_OPERATION);   // by default
    }
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGetListenerf( ALenum pname, ALfloat* value )
{
#if LOG_API_USAGE
	DebugMessage("alGetListenerf called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
            
        OALContext		*oalContext = gOALContextMap->Get(gCurrentContext);
        if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        switch(pname) 
        {
            case AL_GAIN:
                *value = oalContext->GetListenerGain();
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
		DebugMessageN1("alGetListenerf Failed - err = %ld\n", result);
        alSetError(result);
    }
	catch (...) {
		DebugMessage("alGetListenerf Failed");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGetListener3f( ALenum pname, ALfloat* v1, ALfloat* v2, ALfloat* v3 )
{
#if LOG_API_USAGE
	DebugMessage("alGetListener3f called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
            
        OALContext		*oalContext = gOALContextMap->Get(gCurrentContext);
        if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        switch(pname) 
        {
            case AL_POSITION:
                oalContext->GetListenerPosition(v1, v2, v3);
                break;
            case AL_VELOCITY:
                oalContext->GetListenerVelocity(v1, v2, v3);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
		DebugMessageN1("alGetListener3f Failed - err = %ld\n", result);
        alSetError(result);
    }
	catch (...) {
		DebugMessage("alGetListener3f Failed");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGetListenerfv( ALenum pname, ALfloat* values )
{
#if LOG_API_USAGE
	DebugMessage("alGetListenerfv called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
            
        OALContext		*oalContext = gOALContextMap->Get(gCurrentContext);
        if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        switch (pname) 
        {
            case AL_POSITION:
                oalContext->GetListenerPosition(&values[0], &values[1], &values[2]);
                break;
            case AL_VELOCITY:
                oalContext->GetListenerVelocity(&values[0], &values[1], &values[2]);
                break;
            case AL_ORIENTATION:
                oalContext->GetListenerOrientation( &values[0], &values[1], &values[2],
                                                    &values[3], &values[4], &values[5]);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
		DebugMessageN1("alGetListenerfv Failed - err = %ld\n", result);
        alSetError(result);
    }
	catch (...) {
		DebugMessage("alGetListenerfv Failed");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alListener3f (ALenum pname, ALfloat v1, ALfloat v2, ALfloat v3)
{
#if LOG_API_USAGE
	DebugMessage("alListener3f called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
            
        OALContext		*oalContext = gOALContextMap->Get(gCurrentContext);
        if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        switch (pname) 
        {
            case AL_POSITION:
                if (isnan(v1) || isnan(v2) || isnan(v3))
                    throw ((OSStatus) AL_INVALID_VALUE);
                oalContext->SetListenerPosition(v1, v2, v3);
                break;
            case AL_VELOCITY:
                oalContext->SetListenerVelocity(v1, v2, v3);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
		DebugMessageN1("alListener3f Failed - err = %ld\n", result);
        alSetError(result);
    }
	catch (...) {
		DebugMessage("alListener3f Failed");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alListenerfv (ALenum pname, ALfloat *values)
{
#if LOG_API_USAGE
	DebugMessage("alListenerfv called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
            
        OALContext		*oalContext = gOALContextMap->Get(gCurrentContext);
        if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        switch(pname) 
        {
            case AL_POSITION:
                if (isnan(values[0]) || isnan(values[1]) || isnan(values[2]))
                    throw ((OSStatus) AL_INVALID_VALUE);                        
                oalContext->SetListenerPosition(values[0], values[1], values[2]);
                break;
            case AL_VELOCITY:
                oalContext->SetListenerVelocity(values[0], values[1], values[2]);
                break;
            case AL_ORIENTATION:
                oalContext->SetListenerOrientation(	values[0], values[1], values[2],
                                                    values[3], values[4], values[5]);
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
 		DebugMessageN1("alListenerfv Failed - err = %ld\n", result);
       alSetError(result);
    }
	catch (...) {
 		DebugMessage("alListenerfv Failed");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alListenerf (ALenum pname, ALfloat value)
{
#if LOG_API_USAGE
	DebugMessage("alListenerf called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
            
        OALContext		*oalContext = gOALContextMap->Get(gCurrentContext);
        if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        switch (pname) 
        {
            case AL_GAIN:
                oalContext->SetListenerGain((Float32) value);     //gListener.Gain=value;
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
 		DebugMessageN1("alListenerf Failed - err = %ld\n", result);
        alSetError(result);
    }
	catch (...) {
 		DebugMessage("alListenerf Failed");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGetFloatv (ALenum pname, ALfloat *data)
{
#if LOG_API_USAGE
	DebugMessage("alGetFloatv called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
            
        OALContext		*oalContext = gOALContextMap->Get(gCurrentContext);
        if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        switch(pname)
        {
            case AL_DOPPLER_FACTOR:
                *data = oalContext->GetDopplerFactor();
                break;
            case AL_DOPPLER_VELOCITY:
                *data = oalContext->GetDopplerVelocity();
                break;
            default:
                alSetError(AL_INVALID_ENUM);
                break;
        }
	}
	catch (OSStatus		result) {
 		DebugMessageN1("alGetFloatv Failed - err = %ld\n", result);
        alSetError(result);
    }
	catch (...) {
 		DebugMessage("alGetFloatv Failed");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
    return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

ALAPI ALenum ALAPIENTRY alGetEnumValue (ALubyte *ename)
{
#if LOG_API_USAGE
	DebugMessageN1("alGetEnumValue called", *ename);
#endif

	if (strcmp("AL_INVALID", (const char *)ename) == 0) { return AL_INVALID; }
	if (strcmp("ALC_INVALID", (const char *)ename) == 0) { return ALC_INVALID; }
	if (strcmp("AL_NONE", (const char *)ename) == 0) { return AL_NONE; }
	if (strcmp("AL_FALSE", (const char *)ename) == 0) { return AL_FALSE; }
	if (strcmp("ALC_FALSE", (const char *)ename) == 0) { return ALC_FALSE; }
	if (strcmp("AL_TRUE", (const char *)ename) == 0) { return AL_TRUE; }
	if (strcmp("ALC_TRUE", (const char *)ename) == 0) { return ALC_TRUE; }
	if (strcmp("AL_SOURCE_RELATIVE", (const char *)ename) == 0) { return AL_SOURCE_RELATIVE; }
	if (strcmp("AL_CONE_INNER_ANGLE", (const char *)ename) == 0) { return AL_CONE_INNER_ANGLE; }
	if (strcmp("AL_CONE_OUTER_ANGLE", (const char *)ename) == 0) { return AL_CONE_OUTER_ANGLE; }
	if (strcmp("AL_PITCH", (const char *)ename) == 0) { return AL_PITCH; }
	if (strcmp("AL_POSITION", (const char *)ename) == 0) { return AL_POSITION; }
	if (strcmp("AL_DIRECTION", (const char *)ename) == 0) { return AL_DIRECTION; }
	if (strcmp("AL_VELOCITY", (const char *)ename) == 0) { return AL_VELOCITY; }
	if (strcmp("AL_LOOPING", (const char *)ename) == 0) { return AL_LOOPING; }
	if (strcmp("AL_BUFFER", (const char *)ename) == 0) { return AL_BUFFER; }
	if (strcmp("AL_GAIN", (const char *)ename) == 0) { return AL_GAIN; }
	if (strcmp("AL_MIN_GAIN", (const char *)ename) == 0) { return AL_MIN_GAIN; }
	if (strcmp("AL_MAX_GAIN", (const char *)ename) == 0) { return AL_MAX_GAIN; }
	if (strcmp("AL_ORIENTATION", (const char *)ename) == 0) { return AL_ORIENTATION; }
	if (strcmp("AL_REFERENCE_DISTANCE", (const char *)ename) == 0) { return AL_REFERENCE_DISTANCE; }
	if (strcmp("AL_ROLLOFF_FACTOR", (const char *)ename) == 0) { return AL_ROLLOFF_FACTOR; }
	if (strcmp("AL_CONE_OUTER_GAIN", (const char *)ename) == 0) { return AL_CONE_OUTER_GAIN; }
	if (strcmp("AL_MAX_DISTANCE", (const char *)ename) == 0) { return AL_MAX_DISTANCE; }
	if (strcmp("AL_SOURCE_STATE", (const char *)ename) == 0) { return AL_SOURCE_STATE; }
	if (strcmp("AL_INITIAL", (const char *)ename) == 0) { return AL_INITIAL; }
	if (strcmp("AL_PLAYING", (const char *)ename) == 0) { return AL_PLAYING; }
	if (strcmp("AL_PAUSED", (const char *)ename) == 0) { return AL_PAUSED; }
	if (strcmp("AL_STOPPED", (const char *)ename) == 0) { return AL_STOPPED; }
	if (strcmp("AL_BUFFERS_QUEUED", (const char *)ename) == 0) { return AL_BUFFERS_QUEUED; }
	if (strcmp("AL_BUFFERS_PROCESSED", (const char *)ename) == 0) { return AL_BUFFERS_PROCESSED; }
	if (strcmp("AL_FORMAT_MONO8", (const char *)ename) == 0) { return AL_FORMAT_MONO8; }
	if (strcmp("AL_FORMAT_MONO16", (const char *)ename) == 0) { return AL_FORMAT_MONO16; }
	if (strcmp("AL_FORMAT_STEREO8", (const char *)ename) == 0) { return AL_FORMAT_STEREO8; }
	if (strcmp("AL_FORMAT_STEREO16", (const char *)ename) == 0) { return AL_FORMAT_STEREO16; }
	if (strcmp("AL_FREQUENCY", (const char *)ename) == 0) { return AL_FREQUENCY; }
	if (strcmp("AL_SIZE", (const char *)ename) == 0) { return AL_SIZE; }
	if (strcmp("AL_UNUSED", (const char *)ename) == 0) { return AL_UNUSED; }
	if (strcmp("AL_PENDING", (const char *)ename) == 0) { return AL_PENDING; }
	if (strcmp("AL_PROCESSED", (const char *)ename) == 0) { return AL_PROCESSED; }
	if (strcmp("ALC_MAJOR_VERSION", (const char *)ename) == 0) { return ALC_MAJOR_VERSION; }
	if (strcmp("ALC_MINOR_VERSION", (const char *)ename) == 0) { return ALC_MINOR_VERSION; }
	if (strcmp("ALC_ATTRIBUTES_SIZE", (const char *)ename) == 0) { return ALC_ATTRIBUTES_SIZE; }
	if (strcmp("ALC_ALL_ATTRIBUTES", (const char *)ename) == 0) { return ALC_ALL_ATTRIBUTES; }
	if (strcmp("ALC_DEFAULT_DEVICE_SPECIFIER", (const char *)ename) == 0) { return ALC_DEFAULT_DEVICE_SPECIFIER; }
	if (strcmp("ALC_DEVICE_SPECIFIER", (const char *)ename) == 0) { return ALC_DEVICE_SPECIFIER; }
	if (strcmp("ALC_EXTENSIONS", (const char *)ename) == 0) { return ALC_EXTENSIONS; }
	if (strcmp("ALC_FREQUENCY", (const char *)ename) == 0) { return ALC_FREQUENCY; }
	if (strcmp("ALC_REFRESH", (const char *)ename) == 0) { return ALC_REFRESH; }
	if (strcmp("ALC_SYNC", (const char *)ename) == 0) { return ALC_SYNC; }
	if (strcmp("AL_NO_ERROR", (const char *)ename) == 0) { return AL_NO_ERROR; }
	if (strcmp("AL_INVALID_NAME", (const char *)ename) == 0) { return AL_INVALID_NAME; }
	if (strcmp("AL_INVALID_ENUM", (const char *)ename) == 0) { return AL_INVALID_ENUM; }
	if (strcmp("AL_INVALID_VALUE", (const char *)ename) == 0) { return AL_INVALID_VALUE; }
	if (strcmp("AL_INVALID_OPERATION", (const char *)ename) == 0) { return AL_INVALID_OPERATION; }
	if (strcmp("AL_OUT_OF_MEMORY", (const char *)ename) == 0) { return AL_OUT_OF_MEMORY; }
	if (strcmp("ALC_NO_ERROR", (const char *)ename) == 0) { return ALC_NO_ERROR; }
	if (strcmp("ALC_INVALID_DEVICE", (const char *)ename) == 0) { return ALC_INVALID_DEVICE; }
	if (strcmp("ALC_INVALID_CONTEXT", (const char *)ename) == 0) { return ALC_INVALID_CONTEXT; }
	if (strcmp("ALC_INVALID_ENUM", (const char *)ename) == 0) { return ALC_INVALID_ENUM; }
	if (strcmp("ALC_INVALID_VALUE", (const char *)ename) == 0) { return ALC_INVALID_VALUE; }
	if (strcmp("ALC_OUT_OF_MEMORY", (const char *)ename) == 0) { return ALC_OUT_OF_MEMORY; }
	if (strcmp("AL_VENDOR", (const char *)ename) == 0) { return AL_VENDOR; }
	if (strcmp("AL_VERSION", (const char *)ename) == 0) { return AL_VERSION; }
	if (strcmp("AL_RENDERER", (const char *)ename) == 0) { return AL_RENDERER; }
	if (strcmp("AL_EXTENSIONS", (const char *)ename) == 0) { return AL_EXTENSIONS; }
	if (strcmp("AL_DOPPLER_FACTOR", (const char *)ename) == 0) { return AL_DOPPLER_FACTOR; }
	if (strcmp("AL_DOPPLER_VELOCITY", (const char *)ename) == 0) { return AL_DOPPLER_VELOCITY; }
	if (strcmp("AL_DISTANCE_MODEL", (const char *)ename) == 0) { return AL_DISTANCE_MODEL; }
	if (strcmp("AL_INVERSE_DISTANCE", (const char *)ename) == 0) { return AL_INVERSE_DISTANCE; }
	if (strcmp("AL_INVERSE_DISTANCE_CLAMPED", (const char *)ename) == 0) { return AL_INVERSE_DISTANCE_CLAMPED; }
	
	return -1;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALubyte * ALAPIENTRY alGetString (ALenum pname)
{
#if LOG_API_USAGE
	DebugMessage("alGetString called");
#endif

	switch (pname)
	{
		case AL_VENDOR:
			return (ALubyte *)alVendor;
		case AL_VERSION:
			return (ALubyte *)alVersion;
		case AL_RENDERER:
			return (ALubyte *)alRenderer;
		case AL_EXTENSIONS:
			return (ALubyte *)alExtensions;
		case AL_NO_ERROR:
			return (ALubyte *)alNoError;
		case AL_INVALID_NAME:
			return (ALubyte *)alErrInvalidName;
		case AL_INVALID_ENUM:
			return (ALubyte *)alErrInvalidEnum;
		case AL_INVALID_VALUE:
			return (ALubyte *)alErrInvalidValue;
		case AL_INVALID_OPERATION:
			return (ALubyte *)alErrInvalidOp;
		case AL_OUT_OF_MEMORY:
			return (ALubyte *)alErrOutOfMemory;
		default:
			alSetError(AL_INVALID_ENUM);
			break;
	}
	return NULL;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALfloat ALAPIENTRY alGetFloat (ALenum pname)
{
	Float32			returnValue = 0.0f;

#if LOG_API_USAGE
	DebugMessage("alGetFloat called");
#endif

	try {
        if (gOALContextMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);
            
        OALContext		*oalContext = gOALContextMap->Get(gCurrentContext);
        if (oalContext == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        switch (pname)
        {
            case AL_DOPPLER_FACTOR:
                returnValue = oalContext->GetDopplerFactor();
                break;
            case AL_DOPPLER_VELOCITY:
                returnValue = oalContext->GetDopplerVelocity();
                break;
            default:
                break;
        }
	}
	catch (OSStatus		result) {
 		DebugMessageN1("alGetFloat Failed - err = %ld\n", result);
        alSetError(result);
    }
	catch (...) {
 		DebugMessage("alGetFloat Failed");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	return (returnValue);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALboolean ALAPIENTRY alIsExtensionPresent(ALubyte *extName)
{
#if LOG_API_USAGE
	DebugMessage("alIsExtensionPresent called");
#endif

    return AL_FALSE;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

ALAPI ALvoid ALAPIENTRY alDeleteEnvironmentIASIG (ALsizei n, ALuint *environments)
{
#if LOG_API_USAGE
	DebugMessage("***** alDeleteEnvironmentIASIG called");
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alDisable (ALenum capability)
{
#if LOG_API_USAGE
	DebugMessage("***** alDisable called");
#endif
	switch (capability)
	{
		case ALC_CONVERT_DATA_UPON_LOADING:
			gConvertBufferNow = false;
			break;
		default:
			alSetError(AL_INVALID_VALUE);
			break;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alDistanceScale (ALfloat value)
{
#if LOG_API_USAGE
	DebugMessage("***** alDistanceScale called");
#endif

	if (value > 0.0f)
	{
		// gDistanceScale = value;
	} 
    else
	{
		alSetError(AL_INVALID_VALUE);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alEnable (ALenum capability)
{
#if LOG_API_USAGE
	DebugMessage("***** alEnable called");
#endif
	switch(capability)
	{
		case ALC_CONVERT_DATA_UPON_LOADING:
			gConvertBufferNow = true;
			break;
		default:
			alSetError(AL_INVALID_VALUE);
			break;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alEnviromentiIASIG (ALuint environment, ALenum pname, ALint value)
{
#if LOG_API_USAGE
	DebugMessage("***** alEnviromentiIASIG called");
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alEnvironmentfIASIG (ALuint environment, ALenum pname, ALfloat value)
{
#if LOG_API_USAGE
	DebugMessage("***** alEnvironmentfIASIG called");
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALsizei ALAPIENTRY alGenEnvironmentIASIG (ALsizei n, ALuint *environments)
{
#if LOG_API_USAGE
	DebugMessage("***** alGenEnvironmentIASIG called");
#endif
	return 0;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALboolean ALAPIENTRY alGetBoolean (ALenum pname)
{
#if LOG_API_USAGE
	DebugMessage("***** alGetBoolean called");
#endif
	return AL_FALSE;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGetBooleanv (ALenum pname, ALboolean *data)
{
#if LOG_API_USAGE
	DebugMessage("***** alGetBooleanv called");
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALdouble ALAPIENTRY alGetDouble (ALenum pname)
{
#if LOG_API_USAGE
	DebugMessage("***** alGetDouble called");
#endif

    double      returnValue = 0.0;

	try {
        if (gOALDeviceMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALDevice	*oalDevice = gOALDeviceMap->Get(gCurrentDevice);
        if (oalDevice == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

		switch (pname)
		{			
			case ALC_MIXER_OUTPUT_RATE:
				returnValue = (ALdouble) oalDevice->GetMixerRate();
				break;
			
			default:
				alSetError(AL_INVALID_VALUE);
				break;
		}
	}
	catch (OSStatus		result) {
 		DebugMessageN1("alGetDouble Failed - err = %ld\n", result);
        alSetError(result);
    }
	catch (...) {
 		DebugMessage("alGetDouble Failed");
        alSetError(AL_INVALID_OPERATION);   // by default
	}
	
	return (returnValue);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGetDoublev (ALenum pname, ALdouble *data)
{
#if LOG_API_USAGE
	DebugMessage("***** alGetDoublev called");
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSetInteger (ALenum pname, ALint value)
{	
#if LOG_API_USAGE
	DebugMessage("***** alSetIntegeri called");
#endif
	try {
        if (gOALDeviceMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALDevice	*oalDevice = gOALDeviceMap->Get(gCurrentDevice);
        if (oalDevice == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

		switch(pname)
		{			
			case ALC_SPATIAL_RENDERING_QUALITY:
				oalDevice->SetRenderQuality ((UInt32) value);
				break;

			case ALC_RENDER_CHANNEL_COUNT:
				oalDevice->SetRenderChannelCount ((UInt32) value);
				break;

            case ALC_MIXER_MAXIMUM_BUSSES:
                gMaximumMixerBusCount = value;
                break;
			
			default:
				alSetError(AL_INVALID_VALUE);
				break;
		}
	}
	catch (OSStatus		result) {
 		DebugMessageN1("alSetInteger Failed - err = %ld\n", result);
        alSetError(result);
    }
	catch (...) {
 		DebugMessage("alSetInteger Failed");
		alSetError(AL_INVALID_OPERATION); 
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alSetDouble (ALenum pname, ALdouble value)
{	
#if LOG_API_USAGE
	DebugMessage("***** alSetDouble called");
#endif
	try {
        if (gOALDeviceMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALDevice	*oalDevice = gOALDeviceMap->Get(gCurrentDevice);
        if (oalDevice == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

		switch (pname)
		{			
			case ALC_MIXER_OUTPUT_RATE:
				oalDevice->SetMixerRate ((Float64) value);
				break;
			
			default:
				alSetError(AL_INVALID_VALUE);
				break;
		}
	}
	catch (OSStatus		result) {
 		DebugMessageN1("alSetDouble Failed - err = %ld\n", result);
        alSetError(result);
    }
	catch (...) {
 		DebugMessage("alSetDouble Failed");
		alSetError(AL_INVALID_OPERATION); 
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALint ALAPIENTRY alGetInteger (ALenum pname)
{
#if LOG_API_USAGE
	DebugMessage("***** alGetInteger called");
#endif

	UInt32		returnValue	= 0;

	try {
        if (gOALDeviceMap == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

        OALDevice	*oalDevice = gOALDeviceMap->Get(gCurrentDevice);
        if (oalDevice == NULL)
            throw ((OSStatus) AL_INVALID_OPERATION);

		switch (pname)
		{
			case ALC_SPATIAL_RENDERING_QUALITY:
                returnValue = oalDevice->GetRenderQuality();
				break;

			case ALC_RENDER_CHANNEL_COUNT:
                returnValue = oalDevice->GetRenderChannelCount();
				break;
                
            case ALC_MIXER_MAXIMUM_BUSSES:
                returnValue = oalDevice->GetBusCount();
                break;
			
			default:
				alSetError(AL_INVALID_VALUE);
				break;
		}
	}
	catch (OSStatus		result) {
 		DebugMessageN1("alGetInteger Failed - err = %ld\n", result);
        // special case this property for now, it can return a valid value if there is no device open
        if (pname == ALC_MIXER_MAXIMUM_BUSSES)
            returnValue = gMaximumMixerBusCount;
        else
			alSetError(AL_INVALID_OPERATION); // not available yet as the device is not setup
    }
	catch (...) {
 		DebugMessage("alGetInteger Failed");
        // special case this property for now, it can return a valid value if there is no device open
        if (pname == ALC_MIXER_MAXIMUM_BUSSES)
            returnValue = gMaximumMixerBusCount;
        else
			alSetError(AL_INVALID_OPERATION); // not available yet as the device is not setup
	}
	
	return (returnValue);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alGetListeneri( ALenum pname, ALint* value )
{
#if LOG_API_USAGE
	DebugMessage("***** alGetListeneri called");
#endif

	*value = 0;

	try {
		switch (pname)
		{
			default:
				alSetError(AL_INVALID_VALUE);
				break;
		}
	}
	catch (...) {
 		DebugMessage("alGetListeneri Failed");
        alSetError(AL_INVALID_OPERATION); // not available yet as the device is not setup
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid * ALAPIENTRY alGetProcAddress(ALubyte *fname)
{
#if LOG_API_USAGE
	DebugMessage("***** alGetProcAddress called");
#endif
	return NULL;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALboolean ALAPIENTRY alIsEnabled(ALenum capability)
{
#if LOG_API_USAGE
	DebugMessage("***** alIsEnabled called");
#endif
	switch(capability)
	{
		case ALC_CONVERT_DATA_UPON_LOADING:
			return (gConvertBufferNow);
			break;
		default:
			break;
	}
	return (false);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALboolean ALAPIENTRY alIsEnvironmentIASIG (ALuint environment)
{
#if LOG_API_USAGE
	DebugMessage("***** alIsEnvironmentIASIG called");
#endif
	return AL_FALSE;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alListeneri (ALenum pname, ALint value)
{		
#if LOG_API_USAGE
	DebugMessage("***** alListeneri called");
#endif
	alSetError(AL_INVALID_ENUM);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alPropagationSpeed (ALfloat value)
{
#if LOG_API_USAGE
	DebugMessage("***** alPropagationSpeed called");
#endif
	if (value > 0.0f)
	{
		// gPropagationSpeed = value;
	} 
    else
	{
        alSetError(AL_INVALID_VALUE);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alQueuef (ALuint source, ALenum param, ALfloat value)
{
#if LOG_API_USAGE
	DebugMessage("***** alQueuef called");
#endif
	if (alIsSource(source))
	{
		switch(param) 
		{
			case AL_PITCH:
				if ((value>=0.5f)&&(value<=2.0f))
				{	
					// ***** gSource[source].pitch = value;
				}
				else
					alSetError(AL_INVALID_VALUE);
				break;
			case AL_GAIN:
				if (value <= 1.0f)
				{
					// ***** smSetSourceVolume (source, (value * kFullVolume), (value * kFullVolume));
				}	
				break;
			default:
				alSetError(AL_INVALID_OPERATION);
				break;
		}
	} 
    else
	{
		alSetError(AL_INVALID_NAME);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alQueuei (ALuint source, ALenum param, ALint value)
{
#if LOG_API_USAGE
	DebugMessage("***** alQueuei called");
#endif
}