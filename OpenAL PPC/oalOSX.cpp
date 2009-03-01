/**********************************************************************************************************************************
* OpenAL cross platform audio library
* Copyright (C) 1999-2000 by authors.
* Portions Copyright (C) 2004 by Apple Computer Inc.
* This library is free software; you can redistribute it and/or
*  modify it under the terms of the GNU Library General Public
*  License as published by the Free Software Foundation; either
*  version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  Library General Public License for more details.
*
* You should have received a copy of the GNU Library General Public
*  License along with this library; if not, write to the
*  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
*  Boston, MA  02111-1307, USA.
* Or go to http://www.gnu.org/copyleft/lgpl.html
**********************************************************************************************************************************/

#include "oalOSX.h"
#include "alut.h"

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

const char		kPathDelimiter[] = "/";
const UInt32	kPathDelimiterLength = 1;

bool	IsRelativePath(const char* inPath)
{
	return inPath[0] != kPathDelimiter[0];
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	MakeAbsolutePath(const char* inRelativePath, char* outAbsolutePath, UInt32 inMaxAbsolutePathLength)
{
	//	get the path to the current working directory
	getcwd(outAbsolutePath, inMaxAbsolutePathLength);
	if ((strlen(outAbsolutePath) + kPathDelimiterLength) <= inMaxAbsolutePathLength - 1)
	{
		strcat(outAbsolutePath, kPathDelimiter);
		if ((strlen(outAbsolutePath) + strlen(inRelativePath)) <= inMaxAbsolutePathLength - 1)
		{
			strcat(outAbsolutePath, inRelativePath);
		}
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	GetOALFormatFromASBD(CAStreamBasicDescription	&inASBD)
{
	switch (inASBD.mFormatID)
	{
		case kAudioFormatLinearPCM:
			// NOTE: if float: return 0;
			if (inASBD.mFormatFlags & kAudioFormatFlagIsFloat)  
			{
				return (0);		// float currently unsupported
			}
			else
			{
				if (inASBD.NumberChannels() == 1 && inASBD.mBitsPerChannel == 16)
					return AL_FORMAT_MONO16;
				else if (inASBD.NumberChannels() == 2 && inASBD.mBitsPerChannel == 16)
					return AL_FORMAT_STEREO16;
				else if (inASBD.NumberChannels() == 1 && inASBD.mBitsPerChannel == 8)
					return AL_FORMAT_MONO8;
				else if (inASBD.NumberChannels() == 2 && inASBD.mBitsPerChannel == 8)
					return AL_FORMAT_STEREO8;
			}
			break;
		
		default:
			return (0);
			break;
	}
	return (0);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool	IsFormatSupported(UInt32	inFormatID)
{
	switch(inFormatID)
	{
		case AL_FORMAT_MONO16:
		case AL_FORMAT_STEREO16:
		case AL_FORMAT_MONO8:
		case AL_FORMAT_STEREO8:
			return true;
			break;
		default:
			return false;
			break;
	}
	return false;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALUTAPI ALvoid ALUTAPIENTRY alutInit(ALint *argc,ALbyte **argv) 
{
    ALCcontext *Context;
    ALCdevice *Device;
	
    Device=alcOpenDevice(NULL);  //Open device
 	
    if (Device != NULL) {
        Context=alcCreateContext(Device,0);  //Create context
		
	if (Context != NULL) {
	    alcMakeContextCurrent(Context);  //Set active context
	}
    }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALUTAPI ALvoid ALUTAPIENTRY alutExit(ALvoid) 
{
    ALCcontext *Context;
    ALCdevice *Device;
	
    //Get active context
    Context=alcGetCurrentContext();
    //Get device for active context
    Device=alcGetContextsDevice(Context);
    //Release context
    alcDestroyContext(Context);
    //Close device
    alcCloseDevice(Device);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALUTAPI ALvoid ALUTAPIENTRY alutLoadWAVFile(ALbyte *file,ALenum *format,ALvoid **data,ALsizei *size,ALsizei *freq)
{
	OSStatus		err = noErr;
	AudioFileID		audioFile = 0;
	FSRef			fsRef;

	*data = NULL; // in case of failure, do not return some unitialized value as a bogus address

	if (IsRelativePath(file))
	{
		char			absolutePath[256];
		// we need to make a full path here so FSPathMakeRef() works properly
		MakeAbsolutePath(file, absolutePath, 256);
		// create an fsref from the file parameter
		err = FSPathMakeRef ((const UInt8 *) absolutePath, &fsRef, NULL);
	}
	else
		err = FSPathMakeRef ((const UInt8 *) file, &fsRef, NULL);
	
	if (err == noErr)
	{
		err = AudioFileOpen(&fsRef, fsRdPerm, 0, &audioFile);
		if (err == noErr)
		{
			UInt32							dataSize;
			CAStreamBasicDescription		asbd;
			
			dataSize = sizeof(CAStreamBasicDescription);
			AudioFileGetProperty(audioFile, kAudioFilePropertyDataFormat, &dataSize, &asbd);
			
			*format = GetOALFormatFromASBD(asbd);
			if (IsFormatSupported(*format))
			{
				*freq = (UInt32) asbd.mSampleRate;
				
				SInt64	audioDataSize = 0;
				dataSize = sizeof(audioDataSize);
				err = AudioFileGetProperty(audioFile, kAudioFilePropertyAudioDataByteCount, &dataSize, &audioDataSize);
				if (err == noErr)
				{
					*size = audioDataSize;
					*data = NULL;
					*data = calloc(1, audioDataSize);
					if (*data)
					{
						dataSize = audioDataSize;
						err = AudioFileReadBytes(audioFile, false, 0, &dataSize, *data);
						
						if ((asbd.mFormatID == kAudioFormatLinearPCM) && (asbd.mBitsPerChannel > 8))
						{
							// we just got 16 bit pcm data out of a WAVE file on a big endian platform, so endian swap the data
							AudioConverterRef				converter;
							CAStreamBasicDescription		outFormat = asbd;
							void *							tempData = NULL;
							
							// ste format to big endian
							outFormat.mFormatFlags = kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
							// make some place for converted data
							tempData = calloc(1 , audioDataSize);
							
							err = AudioConverterNew(&asbd, &outFormat, &converter);
							if ((err == noErr) && (tempData != NULL))
							{
								UInt32		bufferSize = audioDataSize;
								err = AudioConverterConvertBuffer(converter, audioDataSize, *data, &bufferSize, tempData);
								if (err == noErr)
									memcpy(*data, tempData, audioDataSize);
								AudioConverterDispose(converter);
							}
							if (tempData) free (tempData);
						}
					}
				}
			}
			err = AudioFileClose(audioFile);
		}
	}
    else
        alSetError(err);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALUTAPI ALvoid ALUTAPIENTRY alutUnloadWAV(ALenum format,ALvoid *data,ALsizei size,ALsizei freq)
{
	if (data)
		free(data);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// AL_MAIN functions
ALAPI ALvoid ALAPIENTRY alInit(ALint *argc, ALubyte **argv)
{
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALAPI ALvoid ALAPIENTRY alExit(ALvoid)
{
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// alutLoadWAVMemory()
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

/*
    alutLoadWAVMemory() existed in previous OAL implementations, and is provided for legacy purposes.
    This is the same implementation already existing in the Open Source repository.
*/

typedef struct                                  /* WAV File-header */
{
  ALubyte  Id[4];
  ALsizei  Size;
  ALubyte  Type[4];
} WAVFileHdr_Struct;

typedef struct                                  /* WAV Fmt-header */
{
  ALushort Format;                              
  ALushort Channels;
  ALuint   SamplesPerSec;
  ALuint   BytesPerSec;
  ALushort BlockAlign;
  ALushort BitsPerSample;
} WAVFmtHdr_Struct;

typedef struct									/* WAV FmtEx-header */
{
  ALushort Size;
  ALushort SamplesPerBlock;
} WAVFmtExHdr_Struct;

typedef struct                                  /* WAV Smpl-header */
{
  ALuint   Manufacturer;
  ALuint   Product;
  ALuint   SamplePeriod;                          
  ALuint   Note;                                  
  ALuint   FineTune;                              
  ALuint   SMPTEFormat;
  ALuint   SMPTEOffest;
  ALuint   Loops;
  ALuint   SamplerData;
  struct
  {
    ALuint Identifier;
    ALuint Type;
    ALuint Start;
    ALuint End;
    ALuint Fraction;
    ALuint Count;
  }      Loop[1];
} WAVSmplHdr_Struct;

typedef struct                                  /* WAV Chunk-header */
{
  ALubyte  Id[4];
  ALuint   Size;
} WAVChunkHdr_Struct;

void SwapWords(unsigned int *puint);
void SwapBytes(unsigned short *pshort);

void SwapWords(unsigned int *puint)
{
    unsigned int tempint;
	char *pChar1, *pChar2;
	
	tempint = *puint;
	pChar2 = (char *)&tempint;
	pChar1 = (char *)puint;
	
	pChar1[0]=pChar2[3];
	pChar1[1]=pChar2[2];
	pChar1[2]=pChar2[1];
	pChar1[3]=pChar2[0];
}

void SwapBytes(unsigned short *pshort)
{
    unsigned short tempshort;
    char *pChar1, *pChar2;
    
    tempshort = *pshort;
    pChar2 = (char *)&tempshort;
    pChar1 = (char *)pshort;
    
    pChar1[0]=pChar2[1];
    pChar1[1]=pChar2[0];
}

ALUTAPI ALvoid ALUTAPIENTRY alutLoadWAVMemory(ALbyte *memory,ALenum *format,ALvoid **data,ALsizei *size,ALsizei *freq)
{
	WAVChunkHdr_Struct ChunkHdr;
	WAVFmtExHdr_Struct FmtExHdr;
	WAVFileHdr_Struct FileHdr;
	WAVSmplHdr_Struct SmplHdr;
	WAVFmtHdr_Struct FmtHdr;
	int i;
	ALbyte *Stream;
	
	*format=AL_FORMAT_MONO16;
	*data=NULL;
	*size=0;
	*freq=22050;
	if (memory)
	{		
		Stream=memory;
		if (Stream)
		{
		    memcpy(&FileHdr,Stream,sizeof(WAVFileHdr_Struct));
		    Stream+=sizeof(WAVFileHdr_Struct);
			SwapWords(&FileHdr.Size);
			FileHdr.Size=((FileHdr.Size+1)&~1)-4;
			while ((FileHdr.Size!=0)&&(memcpy(&ChunkHdr,Stream,sizeof(WAVChunkHdr_Struct))))
			{
				Stream+=sizeof(WAVChunkHdr_Struct);
			    SwapWords(&ChunkHdr.Size);
			    
				if ((ChunkHdr.Id[0] == 'f') && (ChunkHdr.Id[1] == 'm') && (ChunkHdr.Id[2] == 't') && (ChunkHdr.Id[3] == ' '))
				{
					memcpy(&FmtHdr,Stream,sizeof(WAVFmtHdr_Struct));
				    SwapBytes(&FmtHdr.Format);
					if (FmtHdr.Format==0x0001)
					{
					    SwapBytes(&FmtHdr.Channels);
					    SwapBytes(&FmtHdr.BitsPerSample);
					    SwapWords(&FmtHdr.SamplesPerSec);
					    SwapBytes(&FmtHdr.BlockAlign);
					    
						*format=(FmtHdr.Channels==1?
								(FmtHdr.BitsPerSample==8?AL_FORMAT_MONO8:AL_FORMAT_MONO16):
								(FmtHdr.BitsPerSample==8?AL_FORMAT_STEREO8:AL_FORMAT_STEREO16));
						*freq=FmtHdr.SamplesPerSec;
						Stream+=ChunkHdr.Size;
					} 
					else
					{
						memcpy(&FmtExHdr,Stream,sizeof(WAVFmtExHdr_Struct));
						Stream+=ChunkHdr.Size;
					}
				}
				else if ((ChunkHdr.Id[0] == 'd') && (ChunkHdr.Id[1] == 'a') && (ChunkHdr.Id[2] == 't') && (ChunkHdr.Id[3] == 'a'))
				{
					if (FmtHdr.Format==0x0001)
					{
						*size=ChunkHdr.Size;
                            if(*data == NULL){
							*data=malloc(ChunkHdr.Size + 31);
							memset(*data,0,ChunkHdr.Size+31);
						}
						else{
							realloc(*data,ChunkHdr.Size + 31);
							memset(*data,0,ChunkHdr.Size+31);
						}
						if (*data) 
						{
							memcpy(*data,Stream,ChunkHdr.Size);
						    memset(((char *)*data)+ChunkHdr.Size,0,31);
							Stream+=ChunkHdr.Size;
						    if (FmtHdr.BitsPerSample == 16) 
						    {
						        for (i = 0; i < (ChunkHdr.Size / 2); i++)
						        {
						        	SwapBytes(&(*(unsigned short **)data)[i]);
						        }
						    }
						}
					}
					else if (FmtHdr.Format==0x0011)
					{
						//IMA ADPCM
					}
					else if (FmtHdr.Format==0x0055)
					{
						//MP3 WAVE
					}
				}
				else if ((ChunkHdr.Id[0] == 's') && (ChunkHdr.Id[1] == 'm') && (ChunkHdr.Id[2] == 'p') && (ChunkHdr.Id[3] == 'l'))
				{
				   	memcpy(&SmplHdr,Stream,sizeof(WAVSmplHdr_Struct));
					Stream+=ChunkHdr.Size;
				}
				else Stream+=ChunkHdr.Size;
				Stream+=ChunkHdr.Size&1;
				FileHdr.Size-=(((ChunkHdr.Size+1)&~1)+8);
			}
		}
	}
}
