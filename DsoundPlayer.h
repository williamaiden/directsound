#ifndef WIN32_AUDIO_CONTROL_DSPLAYER_H
#define WIN32_AUDIO_CONTROL_DSPLAYER_H

#include <dsound.h>
#include <stdint.h>
#include <stdio.h>
#include "Config.h"

#pragma comment (lib,"dsound.lib")
#pragma comment (lib,"dxguid.lib")

#if !defined(PLAYER_NOTIF_POS_COUNT)
#	define PLAYER_NOTIF_POS_COUNT	20
#endif /* PLAYER_NOTIF_POS_COUNT */

/*开关，是否从文件读取数据*/
#define OPEN_READ_PCM_FROM_FILE 1

typedef struct PLAYER{
	PLAYER(){
		device = NULL;
		primaryBuffer = NULL;
		secondaryBuffer = NULL;
		started = false;
		bytes_per_notif_ptr = NULL;
#if OPEN_READ_PCM_FROM_FILE
		fp = NULL;
#endif
	}
	LPDIRECTSOUND device;
	LPDIRECTSOUNDBUFFER primaryBuffer;
	LPDIRECTSOUNDBUFFER secondaryBuffer;
	HANDLE notifEvents[PLAYER_NOTIF_POS_COUNT];
	bool started;
	size_t bytes_per_notif_size;
	uint8_t* bytes_per_notif_ptr;
	HANDLE tid[2];
#if OPEN_READ_PCM_FROM_FILE
	FILE* fp;
#endif
} Player;

/*播放准备*/
int prepare(Player* ds);
/*开始播放*/
int startPlayer(Player* ds);
/*挂起播放*/
int suspendPlayer(Player* ds);
/*唤醒播放*/
int resumePlayer(Player* ds);
/*停止播放*/
int stopPlayer(Player* ds);
/*释放内存资源*/
int unprepare(Player* ds);

DWORD WINAPI playerThreadImpl(LPVOID params);
#if OPEN_READ_PCM_FROM_FILE
int openFile(Player* ds);
int closeFile(Player* ds);
#endif

#endif
