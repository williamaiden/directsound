#ifndef WIN32_AUDIO_CONTROL_DSRECORDER_H
#define WIN32_AUDIO_CONTROL_DSRECORDER_H

#include <dsound.h>
#include <stdint.h>
#include <stdio.h>
#include "Config.h"

#pragma comment (lib,"dsound.lib")
#pragma comment (lib,"dxguid.lib")

#if !defined(RECORDER_NOTIF_POS_COUNT)
#	define RECORDER_NOTIF_POS_COUNT		10
#endif /* RECODER_NOTIF_POS_COUNT */

/*开关，是否将数据写入文件*/
#define OPEN_SAVE_CAPTUREBUFFER_TO_FILE 1

typedef struct RECODER{
	RECODER(){
		device = NULL;
		captureBuffer = NULL;
		started = false;
		bytes_per_notif_ptr = NULL;
#if OPEN_SAVE_CAPTUREBUFFER_TO_FILE
		fp = NULL;
#endif
	}
	LPDIRECTSOUNDCAPTURE device;
	LPDIRECTSOUNDCAPTUREBUFFER captureBuffer;
	HANDLE notifEvents[RECORDER_NOTIF_POS_COUNT];
	bool started;
	size_t bytes_per_notif_size;
	uint8_t* bytes_per_notif_ptr;
	HANDLE tid[2];
#if OPEN_SAVE_CAPTUREBUFFER_TO_FILE
	FILE* fp;
#endif
} Recorder;

/*准备录音*/
int prepare(Recorder* ds);
/*开始录音*/
int startRecorder(Recorder* ds);
/*挂起录音*/
int suspendRecorder(Recorder* ds);
int resumeRecorder(Recorder* ds);
int stopRecorder(Recorder* ds);
/*释放录音资源*/
int unprepare(Recorder* ds);
DWORD WINAPI recorderThreadImpl(LPVOID params);

#if OPEN_SAVE_CAPTUREBUFFER_TO_FILE
int openFile(Recorder* ds);
int closeFile(Recorder* ds);
#endif

#endif
