#include "DsoundPlayer.h"

#if 1
int main(void){
	Player player;
	/*开始录制*/
	if (startPlayer(&player)){
		printf("start player error.\n");
		return -1;
	}
	
	
	/*播放100s*/
	Sleep(100000);

	/*停止录制*/
	/*if (stopPlayer(&player)){
		printf("stop player error.\n");
		return -2;
	}*/

	return 0;
}
#endif
