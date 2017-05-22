#include "DsoundPlayer.h"

/*
功能：释放资源
*/
int unprepare(Player* ds){
	printf("unprepare in.\n");
	if (!ds){
		return -1;
	}
	/*主缓冲*/
	if (ds->primaryBuffer){
		IDirectSoundBuffer_Release(ds->primaryBuffer);
		ds->primaryBuffer = NULL;
	}
	/*辅助缓冲*/
	if (ds->secondaryBuffer){
		IDirectSoundBuffer_Release(ds->secondaryBuffer);
		ds->secondaryBuffer = NULL;
	}
	/*声卡设备对象*/
	if (ds->device){
		IDirectSound_Release(ds->device);
		ds->device = NULL;
	}
	/*通知事件*/
	for (size_t i = 0; i < sizeof(ds->notifEvents) / sizeof(ds->notifEvents[0]); i++){
		if (ds->notifEvents[i]){
			CloseHandle(ds->notifEvents[i]);
			ds->notifEvents[i] = NULL;
		}
	}
	return 0;
}

/*
功能：录音准备
*/
int prepare(Player* ds){
	printf("prepare in.\n");
	HRESULT hr;
	HWND hWnd;
	WAVEFORMATEX wfx = { 0 };
	DSBUFFERDESC dsbd = { 0 };

	if (ds->device || ds->primaryBuffer || ds->secondaryBuffer){
		return -1;
	}

	/* 创建播放设备 */
	if ((hr = DirectSoundCreate(NULL, &ds->device, NULL) != DS_OK)){
		return -3;
	}

	/* 设置协调级别 */
	if ((hWnd = GetForegroundWindow()) || (hWnd = GetDesktopWindow()) || (hWnd = GetConsoleWindow())){
		if ((hr = IDirectSound_SetCooperativeLevel(ds->device, hWnd, DSSCL_PRIORITY)) != DS_OK){
			return -4;
		}
	}

	/* 创建主缓冲区、格式设置 */
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = MEDIA_CHANNELS_DEFAULT;
	wfx.nSamplesPerSec = MEDIA_RATE_DEFAULT;
	wfx.wBitsPerSample = MEDIA_BITS_PER_SAMPLE_DEFAULT;
	wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample / 8);
	wfx.nAvgBytesPerSec = (wfx.nSamplesPerSec * wfx.nBlockAlign);

	/* 计算通知数据包大小，开辟缓冲区 */
	ds->bytes_per_notif_size = ((wfx.nAvgBytesPerSec * MEDIA_PTIME_DEFAULT) / 1000);
	if (!(ds->bytes_per_notif_ptr = (uint8_t*)realloc(ds->bytes_per_notif_ptr, ds->bytes_per_notif_size))){
		//DEBUG_ERROR("Failed to allocate buffer with size = %u", _bytes_per_notif_size);
		return -5;
	}

	dsbd.dwSize = sizeof(DSBUFFERDESC);
	dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbd.dwBufferBytes = 0;
	dsbd.lpwfxFormat = NULL;

	if ((hr = IDirectSound_CreateSoundBuffer(ds->device, &dsbd, &ds->primaryBuffer, NULL)) != DS_OK){
		return -6;
	}

	if ((hr = IDirectSoundBuffer_SetFormat(ds->primaryBuffer, &wfx)) != DS_OK){
		return -7;
	}

	/* 创建次缓冲区、格式设置 */
	dsbd.dwFlags = (DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME);
	dsbd.dwBufferBytes = (DWORD)(PLAYER_NOTIF_POS_COUNT * ds->bytes_per_notif_size);
	dsbd.lpwfxFormat = &wfx;

	if ((hr = IDirectSound_CreateSoundBuffer(ds->device, &dsbd, &ds->secondaryBuffer, NULL)) != DS_OK){
		return -8;
	}

#if 0
	/* 设置音量 [-10000,0]*/
	if (IDirectSoundBuffer_SetVolume(_secondaryBuffer, 0/*_convert_volume(0)*/) != DS_OK){
		printf("setVolume error\n");
	}
#endif
	
	return 0;
}

/*
功能：开始录音
*/
int startPlayer(Player* ds){
	printf("startPlayer in.\n");
	HRESULT hr;
	LPDIRECTSOUNDNOTIFY lpDSBNotify;
	DSBPOSITIONNOTIFY pPosNotify[PLAYER_NOTIF_POS_COUNT] = { 0 };

	static DWORD dwMajorVersion = -1;

	if (!ds){
		return -1;
	}
	if (ds->started){
		return 0;
	}

	// Get OS version
	if (dwMajorVersion == -1){
		OSVERSIONINFO osvi;
		ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
#pragma warning (disable: 4996) //屏蔽警告，GetVersionEx已弃用
		GetVersionEx(&osvi);
		dwMajorVersion = osvi.dwMajorVersion;
	}
	/*播放准备*/
	if (prepare(ds)){
		return -2;
	}

	if (!ds->device || !ds->primaryBuffer || !ds->secondaryBuffer){
		return -3;
	}

	if ((hr = IDirectSoundBuffer_QueryInterface(ds->secondaryBuffer, IID_IDirectSoundNotify, (LPVOID*)&lpDSBNotify)) != DS_OK){
		return -4;
	}

	/* 相关事件通知点 */
	for (size_t i = 0; i < PLAYER_NOTIF_POS_COUNT; i++){
		ds->notifEvents[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		/*在Windows Vista及更高版本的缓冲区开始处设置通知点偏移量，并在XP和之前的缓冲区的一半设置通知点偏移量
		win7 dwMajorVersion = 6*/
		pPosNotify[i].dwOffset = (DWORD)((ds->bytes_per_notif_size * i) + (dwMajorVersion > 5 ? (ds->bytes_per_notif_size >> 1) : 1));
		pPosNotify[i].hEventNotify = ds->notifEvents[i];
	}
	if ((hr = IDirectSoundNotify_SetNotificationPositions(lpDSBNotify, PLAYER_NOTIF_POS_COUNT, pPosNotify)) != DS_OK){
		IDirectSoundNotify_Release(lpDSBNotify);
		return -5;
	}

	if ((hr = IDirectSoundNotify_Release(lpDSBNotify))){

	}

	/* 开始播放缓冲区
	将次缓冲区的声音数据送到混声器中,与其他声音进行混合,最后输到主缓冲区自动播放.*/
	if ((hr = IDirectSoundBuffer_Play(ds->secondaryBuffer, 0, 0, DSBPLAY_LOOPING)) != DS_OK){
		return -6;
	}

#if OPEN_READ_PCM_FROM_FILE
	if (openFile(ds)){
		return -2;
	}
#endif

	ds->tid[0] = ::CreateThread(NULL, 0, playerThreadImpl, ds, 0, NULL);
	if (!ds->tid[0]){
		printf("thread create error.\n");
	}
	/* 启动线程 */
	ds->started = true;

	return 0;
}

/*
功能：暂停播放
*/
int suspendPlayer(Player* ds){
	return 0;
}

/*
功能：重新播放
*/
int resumePlayer(Player* ds){
	return 0;
}

/*
功能：停止播放
*/
int stopPlayer(Player* ds){
	printf("stopPlayer in.\n");
	HRESULT hr;

	if (!ds || !ds->started){
		return 0;
	}
	ds->started = false;
	if (ds->tid[0]){
		if (0 == (WaitForSingleObject(ds->tid[0], INFINITE) == WAIT_FAILED) ? -1 : 0){
			::CloseHandle(ds->tid[0]);
			ds->tid[0] = NULL;
		}
	}

	if ((hr = IDirectSoundBuffer_Stop(ds->secondaryBuffer)) != DS_OK){

	}

	if ((hr = IDirectSoundBuffer_SetCurrentPosition(ds->secondaryBuffer, 0)) != DS_OK){

	}
#if OPEN_READ_PCM_FROM_FILE
	if (closeFile(ds)){
		return -2;
	}
#endif
	// unprepare
	// will be prepared again before calling next start()
	unprepare(ds);

	return 0;
}

/*
功能：线程执行体
*/
DWORD WINAPI playerThreadImpl(LPVOID params){
	printf("playerThreadImpl in.\n");
	HRESULT hr;
	LPVOID lpvAudio1, lpvAudio2;
	DWORD dwBytesAudio1, dwBytesAudio2, dwEvent;
	static const DWORD dwWriteCursor = 0;
	size_t out_size = 0;

	Player* ds = (Player*)(params);
	if (!ds){
		return NULL;
	}
	SetThreadPriority(GetCurrentThread(), THREAD_BASE_PRIORITY_MAX);

	while (ds->started) {
		dwEvent = WaitForMultipleObjects(PLAYER_NOTIF_POS_COUNT, ds->notifEvents, FALSE, INFINITE);
		if (!ds->started) {
			break;
		}
		// lock
		if (hr = IDirectSoundBuffer_Lock(ds->secondaryBuffer,
			dwWriteCursor/* Ignored because of DSBLOCK_FROMWRITECURSOR */,
			(DWORD)ds->bytes_per_notif_size,
			&lpvAudio1, &dwBytesAudio1,
			&lpvAudio2, &dwBytesAudio2,
			DSBLOCK_FROMWRITECURSOR) != DS_OK){
			printf("IDirectSoundBuffer_Lock error\n");
			continue;
		}

#if OPEN_READ_PCM_FROM_FILE
		if ((out_size = fread(ds->bytes_per_notif_ptr, 1, ds->bytes_per_notif_size, ds->fp)) != ds->bytes_per_notif_size){
			//ds->started = false;//停止播放
			printf("player finish.\n");
			stopPlayer(ds);
		}
#endif
		if (out_size < ds->bytes_per_notif_size) {
			// fill with silence
			memset(&ds->bytes_per_notif_ptr[out_size], 0, (ds->bytes_per_notif_size - out_size));
		}
		if ((dwBytesAudio1 + dwBytesAudio2) == ds->bytes_per_notif_size) {
			memcpy(lpvAudio1, ds->bytes_per_notif_ptr, dwBytesAudio1);
			if (lpvAudio2 && dwBytesAudio2) {
				memcpy(lpvAudio2, &ds->bytes_per_notif_ptr[dwBytesAudio1], dwBytesAudio2);
			}
		}
		else {
			//DEBUG_ERROR("Not expected: %d+%d#%d", dwBytesAudio1, dwBytesAudio2, dsound->bytes_per_notif_size);
		}

		// unlock
		if ((hr = IDirectSoundBuffer_Unlock(ds->secondaryBuffer, lpvAudio1, dwBytesAudio1, lpvAudio2, dwBytesAudio2)) != DS_OK) {

		}
	}

	return NULL;
}

#if OPEN_READ_PCM_FROM_FILE
int openFile(Player* ds){
	printf("openFile in.\n");
	if (!ds){
		return -1;
	}
	if (!(ds->fp)){
		fopen_s(&(ds->fp), "../test.pcm", "rb");
		if (ds->fp == NULL){
			printf("cannot open PCM file.\n");
			return -2;
		}
	}
	return 0;
}

int closeFile(Player* ds){
	printf("closeFile in.\n");
	if (!ds){
		return -1;
	}
	if (ds->fp){
		fclose(ds->fp);
		ds->fp = NULL;
	}
	return 0;
}
#endif
