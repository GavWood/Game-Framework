//
//  SdSoundCaptureAL.cpp
//

#include <iostream>
#include <cstdlib>
#include "SdSoundCaptureAL.h"
#include "BtTypes.h"
#include "al.h"
#include "alc.h"
#include "ErrorLog.h"
#include "MtMath.h"
#include "SdSound.h"
#include "ApConfig.h"

//#include "kiss_fft.h"
#include "fft.h"

#define FFTBufferSize 1024
// Wikipedia: In terms of frequency, human voices are roughly in the range of 
// 80 Hz to 1100 Hz (that is, E2 to C6).
const float MIN_FREQ = 50.0f;
//const float MAX_FREQ = 1500.0f;
const float MAX_FREQ = 1800.0f;
BtFloat SdSoundCaptureAL::m_pitch = 0;
BtBool SdSoundCaptureAL::m_isRecordAvailable = BtFalse;

#pragma pack (1)

typedef struct tWAVEFORMATEX
{
    BtU16    wFormatTag;        /* format type */
    BtU16    nChannels;         /* number of channels (i.e. mono, stereo...) */
    BtU32	 nSamplesPerSec;    /* sample rate */
    BtU32    nAvgBytesPerSec;   /* for buffer estimation */
    BtU16    nBlockAlign;       /* block size of data */
    BtU16    wBitsPerSample;    /* Number of bits per sample of mono data */
    BtU16    cbSize;            /* The count in bytes of the size of extra information (after cbSize) */
} WAVEFORMATEX;
//#endif /* _WAVEFORMATEX_ */

//#ifndef WAVEHEADER
//#define WAVEHEADER
typedef struct
{
	char			szRIFF[4];
	BtU32			lRIFFSize;
	char			szWave[4];
	char			szFmt[4];
	BtU32			lFmtSize;
	WAVEFORMATEX	wfex;
	char			szData[4];
	BtU32			lDataSize;
} WAVEHEADER;
//#endif /* WAVHEADER */

#pragma pack (8)

WAVEHEADER sWaveHeader;
ALCdevice* pCaptureDevice = NULL;
ALchar inWAV[FFTBufferSize];
FILE *audioFile;
BtU32 iDataSize;
BtFloat volume;

// KISS FFT
BtBool SdSoundCaptureAL::m_isFFT = BtFalse;

// alcMakeContextCurrent(NULL) before alcDestroyContext(outputContext).

///////////////////////////////////////////////////////////////////////////////
// StartCapture

// static
BtBool SdSoundCaptureAL::StartCapture( BtBool isToFile, BtBool isFFT )
{
	// http://stackoverflow.com/questions/3056113/recording-audio-with-openal
	// http://stackoverflow.com/questions/9785447/simultaneous-record-playing-with-openal-using-threads

	pCaptureDevice = alcCaptureOpenDevice( NULL, 22050, AL_FORMAT_MONO16, FFTBufferSize);
    
	if( !pCaptureDevice  )
	{
		ErrorLog::Printf( "Could not find EXT capture\r\n" );
		return BtFalse;
	}
    
	ALCdevice		*pDevice;
	ALCcontext		*pContext;
	ALint			iDataSize = 0;
    
	// Check for Capture Extension support
	pContext = alcGetCurrentContext();
	pDevice = alcGetContextsDevice(pContext);
	if (alcIsExtensionPresent(pDevice, "ALC_EXT_CAPTURE") == AL_FALSE)
	{
		ErrorLog::Printf("Could not find EXT capture\r\n");
		return BtFalse;
	}
    
	// Create / open a file for the captured data
	BtChar filename[256];
	sprintf( filename, "%ssample.wav", ApConfig::GetDocuments() );
	audioFile = fopen( filename, "wb");
    
	if ( audioFile == NULL )
	{
		//ErrorLog::Fatal_Printf( "Could not find EXT capture\r\n" );
		//return;
	}
    
	iDataSize = 0;
    
	// Prepare a WAVE file header for the captured data
	// http://soundfile.sapp.org/doc/WaveFormat/
	sprintf(sWaveHeader.szRIFF, "RIFF");
	sWaveHeader.lRIFFSize = 0;
	sprintf(sWaveHeader.szWave, "WAVE");
	sprintf(sWaveHeader.szFmt, "fmt ");
	sWaveHeader.lFmtSize = sizeof(WAVEFORMATEX);
	sWaveHeader.wfex.nChannels = 1;
	sWaveHeader.wfex.wBitsPerSample = 16;
	sWaveHeader.wfex.wFormatTag = 1; //WAVE_FORMAT_PCM;
	sWaveHeader.wfex.nSamplesPerSec = 22050;
	sWaveHeader.wfex.nBlockAlign = sWaveHeader.wfex.nChannels * sWaveHeader.wfex.wBitsPerSample / 8;
	sWaveHeader.wfex.nAvgBytesPerSec = sWaveHeader.wfex.nSamplesPerSec * sWaveHeader.wfex.nBlockAlign;
	sWaveHeader.wfex.cbSize = 0;
	sprintf(sWaveHeader.szData, "data");
	sWaveHeader.lDataSize = 0;
    
	if( audioFile )
	{
		fwrite(&sWaveHeader, sizeof(WAVEHEADER), 1, audioFile);
	}
    
	// Start audio capture
	alcCaptureStart(pCaptureDevice);

	m_isFFT= isFFT;

	init_fft( FFTBufferSize, 22050);

	return BtTrue;
}

////////////////////////////////////////////////////////////////////////////////
// Update

// static
void SdSoundCaptureAL::Update()
{
	if (m_isRecordAvailable)
	{
		ALint iSamplesAvailable;

		// Find out how many samples have been captured
		alcGetIntegerv(pCaptureDevice, ALC_CAPTURE_SAMPLES, 1, &iSamplesAvailable);

		// When we have enough data to fill our BUFFERSIZE byte buffer, grab the samples
		if (iSamplesAvailable > (ALint)(FFTBufferSize / sWaveHeader.wfex.nBlockAlign))
		{
			BtU32 numSamples = FFTBufferSize / sWaveHeader.wfex.nBlockAlign;

			// Consume Samples
			alcCaptureSamples(pCaptureDevice, inWAV, numSamples);

			ALCshort *buffer = (ALCshort*)inWAV;

			// Get the average volume
			//bool isSampling = BtFalse;
			//if( isSampling )
			{
				BtU32 averageVolume = 0;
				for (BtU32 i = 0; i < numSamples; i++)
				{
					BtS32 val = buffer[i];
					averageVolume += MtAbs(val);
				}

				volume = ((BtFloat)averageVolume) / numSamples;

				// Calculate the pitch
				m_pitch = find_pitch((BtU8*)buffer, MIN_FREQ, MAX_FREQ);
			}

			// Write this to a file
			if (audioFile)
			{
				// Write the audio data to a file
				fwrite(inWAV, FFTBufferSize, 1, audioFile);
			}

			// Record total amount of data recorded
			iDataSize += FFTBufferSize;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// StopCapture

void SdSoundCaptureAL::StopCapture()
{
	// Stop the FFT
	done_fft();

	// Stop capture
	alcCaptureStop(pCaptureDevice);

	// Fill in Size information in Wave Header
	if(audioFile)
	{
		fseek(audioFile, 4, SEEK_SET);
		BtU32 iSize = iDataSize + sizeof(WAVEHEADER) - 8;
		fwrite(&iSize, 4, 1, audioFile);
		fseek(audioFile, 42, SEEK_SET);
		fwrite(&iDataSize, 4, 1, audioFile);

		// Close the audio file
		fclose(audioFile);
	}
}

////////////////////////////////////////////////////////////////////////////////
// GetFFT

BtFloat	*SdSoundCaptureAL::GetFFT( BtU32 &samples )
{
//	samples = SamplesRead;
	return (BtFloat*)BtNull;//simpleOutFFT;
}

////////////////////////////////////////////////////////////////////////////////
// GetPitch

BtFloat SdSoundCaptureAL::GetPitch()
{
	return m_pitch;
}

////////////////////////////////////////////////////////////////////////////////
// GetVolume

//static
BtFloat SdSoundCaptureAL::GetVolume()
{
	return volume;
}

////////////////////////////////////////////////////////////////////////////////
// IsRecordAvailable

//static
BtBool SdSoundCaptureAL::IsRecordAvailable()
{
	return m_isRecordAvailable;
}

////////////////////////////////////////////////////////////////////////////////
// Create

void SdSoundCaptureAL::Create()
{
	m_isRecordAvailable = SdSoundCaptureAL::StartCapture( BtFalse );
}

////////////////////////////////////////////////////////////////////////////////
// Destroy

void SdSoundCaptureAL::Destroy()
{
	SdSoundCaptureAL::StopCapture();
}