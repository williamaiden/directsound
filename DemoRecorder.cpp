#include "DsoundRecorder.h"

#if 0
int main(void){
	Recorder recorder;
	/*开始录制*/
	if (startRecorder(&recorder)){
		printf("start recorder error.\n");
		return -1;
	}
	/*录制10s*/
	Sleep(10000);

	/*停止录制*/
	if (stopRecorder(&recorder)){
		printf("stop recorder error.\n");
		return -2;
	}

	return 0;
}
#endif
