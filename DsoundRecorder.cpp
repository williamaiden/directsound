#include "DsoundRecorder.h"
#include <stdio.h>
/*
功能：资源回收
*/
int unprepare(Recorder* ds){
	printf("unprepare in.\n");
	if (!ds){
		return -1;
	}
	if (ds->captureBuffer) {
		IDirectSoundCaptureBuffer_Release(ds->captureBuffer);
		ds->captureBuffer = NULL;
	}
	if (ds->device) {
		IDirectSoundCapture_Release(ds->device);
		ds->device = NULL;
	}
	for (size_t i = 0; i < (sizeof(ds->notifEvents) / sizeof(ds->notifEvents[0])); i++){
		if (ds->notifEvents[i]) {
			CloseHandle(ds->notifEvents[i]);
			ds->notifEvents[i] = NULL;
		}
	}

	return 0;
}

/*
功能：配置录音信息，分配资源
*/
int prepare(Recorder* ds){
	printf("prepare in.\n");
	HRESULT hr;
	WAVEFORMATEX wfx = {0};
	DSCBUFFERDESC dsbd = {0};

	if (!ds){
		return -1;
	}

	/*已经准备完毕，直接使用旧的*/
	if (ds->device && ds->captureBuffer) {
		return 0;
	}
#if 0
	ds = (Recorder*)malloc(sizeof(Recorder));

	if (!ds){
		return -2;
	}
#endif

	/* 创建采集设备 */
	if ((hr = DirectSoundCaptureCreate(NULL, &ds->device, NULL) != DS_OK)) {
		return -3;
	}

	/* 配置采集缓冲区 */
	wfx.wFormatTag = WAVE_FORMAT_PCM;        //PCM采集
	wfx.nChannels = MEDIA_CHANNELS_DEFAULT;  //声道
	wfx.nSamplesPerSec = MEDIA_RATE_DEFAULT; //每秒样本次数
	wfx.wBitsPerSample = MEDIA_BITS_PER_SAMPLE_DEFAULT; //声道样本数据的位数
	wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample / 8);  //数据块大小
	wfx.nAvgBytesPerSec = (wfx.nSamplesPerSec * wfx.nBlockAlign);//缓冲区估计

	/* 数据包大小：采集量达到，马上通知 */
	ds->bytes_per_notif_size = ((wfx.nAvgBytesPerSec * MEDIA_PTIME_DEFAULT) / 1000);

	dsbd.dwSize = sizeof(DSCBUFFERDESC);
	dsbd.dwBufferBytes = (DWORD)(RECORDER_NOTIF_POS_COUNT * ds->bytes_per_notif_size);
	dsbd.lpwfxFormat = &wfx;
	/*创建采集缓冲区*/
	if ((hr = IDirectSoundCapture_CreateCaptureBuffer(ds->device, &dsbd, &ds->captureBuffer, NULL)) != DS_OK) {
		return -4;
	}

	return 0;
}

/*
功能：开始录音
*/
int startRecorder(Recorder* ds){
	printf("startRecorder in.\n");
	DWORD dwOffset;
	HRESULT hr;
	LPDIRECTSOUNDNOTIFY lpDSBNotify;
	DSBPOSITIONNOTIFY pPosNotify[RECORDER_NOTIF_POS_COUNT] = { 0 };

	/*录音准备*/
	if (!ds || prepare(ds)){
		return -1;
	}
	/*准备有故障*/
	if (!ds->device || !ds->captureBuffer) {
		return -2;
	}
	/*原本正在录音，尚未停止*/
	if (ds->started) {
		return 0;
	}

	/*创建通知对象 - lpDSBNotify*/
	if ((hr = IDirectSoundCaptureBuffer_QueryInterface(ds->captureBuffer,
		IID_IDirectSoundNotify,
		(LPVOID*)&lpDSBNotify)) != DS_OK) {
		return -3;
	}

	/* 设置事件通知 */
	dwOffset = (DWORD)(ds->bytes_per_notif_size - 1);
	for (size_t i = 0; i < (sizeof(ds->notifEvents) / sizeof(ds->notifEvents[0])); i++){
		ds->notifEvents[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		pPosNotify[i].dwOffset = dwOffset; //通知事件触发的位置（距离缓冲开始位置的偏移量）
		pPosNotify[i].hEventNotify = ds->notifEvents[i]; //触发的事件的句柄
		dwOffset += (DWORD)ds->bytes_per_notif_size;
	}
	/*设置通知位置*/
	if ((hr = IDirectSoundNotify_SetNotificationPositions(lpDSBNotify,
		RECORDER_NOTIF_POS_COUNT, //包含几个通知的位置
		pPosNotify)) != DS_OK) {
		IDirectSoundNotify_Release(lpDSBNotify);
		return -3;
	}

	if ((hr = IDirectSoundNotify_Release(lpDSBNotify))) {

	}
	else {
		IDirectSoundCaptureBuffer_Start(ds->captureBuffer, DSBPLAY_LOOPING);
	}

#if OPEN_SAVE_CAPTUREBUFFER_TO_FILE
	if (openFile(ds)){
		return -2;
	}
#endif

	ds->tid[0] = ::CreateThread(NULL, 0, recorderThreadImpl, ds, 0, NULL);
	if (!ds->tid[0]){
		printf("thread create error.\n");
	}
	/* 启动线程 */
	ds->started = true;

	return 0;

}
/*
功能：停止录音
*/
int stopRecorder(Recorder* ds){
	printf("stopRecorder in.\n");
	HRESULT hr;
	int ret = 0;

	if (!ds || !ds->started) {
		return 0;
	}

	ds->started = false;

	/*发送已标记消息，让线程不再等待*/
	if (ds->notifEvents[0]) {
		// thread is paused -> raise event now that "started" is equal to false
		SetEvent(ds->notifEvents[0]);
	}

	/* 等待录音线程结束 */
	if (ds->tid[0]){
		if (0 == (ret = (WaitForSingleObject(ds->tid[0], INFINITE) == WAIT_FAILED) ? -1 : 0)){
			::CloseHandle(ds->tid[0]);
			ds->tid[0] = NULL;
		}
	}

	if ((hr = IDirectSoundCaptureBuffer_Stop(ds->captureBuffer)) != DS_OK) {

	}

#if OPEN_SAVE_CAPTUREBUFFER_TO_FILE
	if (closeFile(ds)){
		return -2;
	}
#endif
	// unprepare
	// will be prepared again before next start()
	unprepare(ds);

	return 0;
}
/*
功能：暂停录音
*/
int suspendRecorder(Recorder* ds){
	printf("suspendRecorder in.\n");
	if (!ds || !ds->started){
		return -1;
	}
	/*暂停录音*/
	IDirectSoundCaptureBuffer_Stop(ds->captureBuffer);
#if OPEN_SAVE_CAPTUREBUFFER_TO_FILE
	if (closeFile(ds)){
		return -2;
	}
#endif
	ds->started = false;

	return 0;
}
/*
功能：重新开始录音
*/
int resumeRecorder(Recorder* ds){
	printf("resumeRecorder in.\n");
	if (!ds){
		return -1;
	}
	/*继续录音*/
	IDirectSoundCaptureBuffer_Start(ds->captureBuffer, DSBPLAY_LOOPING);
#if OPEN_SAVE_CAPTUREBUFFER_TO_FILE
	if (openFile(ds)){
		return -2;
	}
#endif
	ds->started = true;
	return 0;
}

/*
功能：线程执行体
*/
DWORD WINAPI recorderThreadImpl(LPVOID params){
	printf("recorderThreadImpl in.\n");
	HRESULT hr;
	LPVOID lpvAudio1, lpvAudio2;
	DWORD dwBytesAudio1, dwBytesAudio2, dwEvent, dwIndex;

	Recorder* ds = (Recorder*)params;
	if (!ds){
		return NULL;
	}

	SetThreadPriority(GetCurrentThread(), THREAD_BASE_PRIORITY_LOWRT);

	while (ds->started) {
		dwEvent = WaitForMultipleObjects(RECORDER_NOTIF_POS_COUNT, ds->notifEvents, FALSE, INFINITE);
		if (!ds->started) {
			break;
		}
		if (dwEvent < WAIT_OBJECT_0 || dwEvent >(WAIT_OBJECT_0 + RECORDER_NOTIF_POS_COUNT)) {
			break;
		}
		dwIndex = (dwEvent - WAIT_OBJECT_0);

		// lock
		if ((hr = IDirectSoundCaptureBuffer_Lock(ds->captureBuffer,
			(DWORD)(dwIndex * ds->bytes_per_notif_size), //（*乘号）锁定的内存与缓冲区首地址之间的偏移量
			(DWORD)ds->bytes_per_notif_size,             //锁定的缓存的大小
			&lpvAudio1,     //获取到的指向缓存数据的指针
			&dwBytesAudio1, //获取到的缓存数据的大小
			&lpvAudio2, &dwBytesAudio2, 0)) != DS_OK) {
			continue;
		}

#if OPEN_SAVE_CAPTUREBUFFER_TO_FILE
		if (fwrite(lpvAudio1, 1, dwBytesAudio1, ds->fp) != dwBytesAudio1){
			printf("fwrite audio1 error.\n");
		}
		if (fwrite(lpvAudio2, 1, dwBytesAudio2, ds->fp) != dwBytesAudio2){
			printf("fwrite audio2 error.\n");
		}
#endif

		//printf("saveCaptureToFile\n");
		//saveCaptureToFile(lpvAudio1, dwBytesAudio1, lpvAudio2, dwBytesAudio2);

		// unlock
		if ((hr = IDirectSoundCaptureBuffer_Unlock(ds->captureBuffer,
			lpvAudio1, //获取到的指向缓存数据的指针
			dwBytesAudio1, //写入的数据量
			lpvAudio2, dwBytesAudio2)) != DS_OK) {
			continue;
		}
	}

	return NULL;
}

#if OPEN_SAVE_CAPTUREBUFFER_TO_FILE
int openFile(Recorder* ds){
	if (!ds){
		return -1;
	}
	if (!(ds->fp)){
		fopen_s(&(ds->fp), "../test.pcm", "ab");
		if (ds->fp == NULL){
			printf("cannot open PCM file.\n");
			return -2;
		}
	}
	return 0;
}

int closeFile(Recorder* ds){
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
