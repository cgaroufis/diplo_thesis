// SineCreate.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <Windows.h>
#include "../../../../../Downloads/pa_stable_v190600_20161030/portaudio/include/portaudio.h"

#define SAMPLE_RATE   (44100)
#define FRAMES_PER_BUFFER  (128)
#define PIPENAME "\\\\.\\pipe\\userdata"

#ifndef M_PI
#define M_PI  (3.14159264)
#endif

struct SineData
{
	double A;
	int f;
};

SineData s1, s2;

SineData ProcessDescriptor(char desc);

class Sine //class definition
{
public:
	Sine() : stream(0), phase1(0), phase2(0)
	{
		/* initialise sinusoidal wavetable */
		for (int i = 0; i<4000; i++)
		{
			sine[i] = (float)(sin(((double)i / (double)4000) * M_PI * 2.)); //mia periodos?
		}

		printf(message, "No Message");
	}

	bool open(PaDeviceIndex index) //class members
	{
		PaStreamParameters outputParameters;

		outputParameters.device = index;
		if (outputParameters.device == paNoDevice) {
			return false;
		}

		const PaDeviceInfo* pInfo = Pa_GetDeviceInfo(index);
		//if (pInfo != 0)
		//{
		//	printf("Output device name: '%s'\r", pInfo->name);
		//}

		outputParameters.channelCount = 2;       /* stereo output */
		outputParameters.sampleFormat = paFloat32; /* 32 bit floating point output */
		outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
		outputParameters.hostApiSpecificStreamInfo = NULL;

		PaError err = Pa_OpenStream(
			&stream,
			NULL, /* no input */
			&outputParameters,
			SAMPLE_RATE,
			paFramesPerBufferUnspecified,
			paClipOff,      /* we won't output out of range samples so don't bother clipping them */
			&Sine::paCallback,
			this            /* Using 'this' for userData so we can cast to Sine* in paCallback method */
			);

		if (err != paNoError)
		{
			/* Failed to open stream to device !!! */
			return false;
		}

		err = Pa_SetStreamFinishedCallback(stream, &Sine::paStreamFinished);

		if (err != paNoError)
		{
			Pa_CloseStream(stream);
			stream = 0;

			return false;
		}

		return true;
	}

	bool close()
	{
		if (stream == 0)
			return false;

		PaError err = Pa_CloseStream(stream);
		stream = 0;

		return (err == paNoError);
	}


	bool start()
	{
		if (stream == 0)
			return false;

		PaError err = Pa_StartStream(stream);

		return (err == paNoError);
	}

	bool stop()
	{
		if (stream == 0)
			return false;

		PaError err = Pa_StopStream(stream);

		return (err == paNoError);
	}

private:
	/* The instance callback, where we have access to every method/variable in object of class Sine */
	int paCallbackMethod(const void *inputBuffer, void *outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo* timeInfo,
		PaStreamCallbackFlags statusFlags)
	{
		float *out = (float*)outputBuffer;
		unsigned long i;

		(void)timeInfo; /* Prevent unused variable warnings. */
		(void)statusFlags;
		(void)inputBuffer;

		for (i = 0; i<framesPerBuffer; i++) /*left phase, right phase correspond to left-right earphones...*/
		{
			*out++ = 2*s1.A*sine[int(phase1)] + s2.A*sine[int(phase2)];  /* left */
			*out++ = 2*s1.A*sine[int(phase1)] + s2.A*sine[int(phase2)];  /* right */
			phase1 += double(4000)/double(s1.f);
			if (phase1 >= 4000) phase1 -= 4000; /*restart period*/
			phase2 += double(4000) / double(s2.f);
			if (phase2 >= 4000) phase2 -= 4000;
		}
		return paContinue;

	}

	/* This routine will be called by the PortAudio engine when audio is needed.
	** It may called at interrupt level on some machines so don't do anything
	** that could mess up the system like calling malloc() or free().
	*/
	static int paCallback(const void *inputBuffer, void *outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo* timeInfo,
		PaStreamCallbackFlags statusFlags,
		void *userData)
	{
		/* Here we cast userData to Sine* type so we can call the instance method paCallbackMethod, we can do that since
		we called Pa_OpenStream with 'this' for userData */
		return ((Sine*)userData)->paCallbackMethod(inputBuffer, outputBuffer,
			framesPerBuffer,
			timeInfo,
			statusFlags);
	}


	void paStreamFinishedMethod()
	{
		printf("Stream Completed: %s\n", message);
	}

	/*
	* This routine is called by portaudio when playback is done.
	*/
	static void paStreamFinished(void* userData)
	{
		return ((Sine*)userData)->paStreamFinishedMethod();
	}

	PaStream *stream;
	float sine[4000];
	double phase1;
	double phase2;
	char message[20];
};

class ScopedPaHandler
{
public:
	ScopedPaHandler()
		: _result(Pa_Initialize())
	{
	}
	~ScopedPaHandler()
	{
		if (_result == paNoError)
		{
			Pa_Terminate();
		}
	}

	PaError result() const { return _result; }

private:
	PaError _result;
};


/*******************************************************************/
int main(void);
int main(void)
{
	Sine sine;
	char data[2];

	Sleep(200); //for synchronization issues until a second pipe is created.
	HANDLE hPipe = CreateFileA(PIPENAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	printf("Audio process opened\n");
	ScopedPaHandler paInit;
	if (paInit.result() != paNoError) goto error;
	{
		//initialising with the first values
		do
		{
			while (ReadFile(hPipe, data, 2, NULL, NULL) == 0); //block until reading the first value//
			s1 = ProcessDescriptor(data[0]);
			s2 = ProcessDescriptor(data[1]); 
		}
		while (s1.f <= 0); //and f2 <= 0
		
		//working loop.
		while (1 > 0)
		{
			printf("Value read: %d\n", s1.f);
			while(sine.open(Pa_GetDefaultOutputDevice()) == 0);
			while(sine.start() ==0);
			while (ReadFile(hPipe, data, 2, NULL, NULL) == 0); //reblock
			s1 = ProcessDescriptor(data[0]);
			s2 = ProcessDescriptor(data[1]);
			while (sine.stop() == 0);
			while(sine.close()==0);
		}
	}

	printf("Test finished.\n");
	return paNoError;

error:
	fprintf(stderr, "An error occured while using the portaudio stream\n");
	fprintf(stderr, "Error number: %d\n", paInit.result());
	fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(paInit.result()));
	return 1;
}

SineData ProcessDescriptor(char descr)
{
	SineData SData;

	if (descr != 48)
	{
		SData.A = 0.33;
		SData.f = 40 + 20 * (descr - 48);
	}
	else //zero descriptor
	{
		SData.A = 0;
		SData.f = 100;
	}
	return SData;
}