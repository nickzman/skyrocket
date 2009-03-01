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

#ifndef __OAL_BUFFER__
#define __OAL_BUFFER__

#include <Carbon/Carbon.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <map>

struct OALBuffer{
					char							*mData;					// ptr to the actual audio data
					UInt32							mDataSize;				// size in bytes of the audio data ptr
					CAStreamBasicDescription		mDataFormat;			// format of the data of the data as it sits in memory
					UInt32							mPreConvertedDataSize;  // if data gets converted on the way in, it is necessary to remember the original size
					CAStreamBasicDescription		mPreConvertedDataFormat;// if data gets converted on the way in, it is necessary to remember the original format
					UInt32							mAttachedCount;			// count of the buffer's use across all valid source queues
					bool							mDataHasBeenConverted;	// was the data converted to the mixer format when handed to the library?
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark _____OALDeviceMap_____
class OALBufferMap : std::multimap<UInt32, OALBuffer, std::less<UInt32> > {
public:
    
    void Add (const	UInt32	inBufferToken, OALBuffer *inBuffer)  
	{
		iterator it = upper_bound(inBufferToken);
		insert(it, value_type (inBufferToken, *inBuffer));
	}

    OALBuffer* Get(UInt32	inBufferToken) {
        iterator	it = find(inBufferToken);
        if (it != end())
            return (&(*it).second);
		return (NULL);
    }
    
    void Remove (const	UInt32	inBufferToken) {
        iterator 	it = find(inBufferToken);
        if (it != end())
            erase(it);
    }

    UInt32 Size () const { return size(); }
    bool Empty () const { return empty(); }
};

#endif