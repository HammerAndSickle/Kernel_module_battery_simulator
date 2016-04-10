#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DEVICE_FILE_NAME "/proc/battery_test" // 배터리 모듈 파일

int main()
{
	int device;
	int battLevel;		//남은 배터리 양(읽어들인 것)
	int battLevelChar;	//남은 배터리 양을 문자로 나타냄(battLevel / 10)

	int i;
	char buf[128] = {0, };	//모듈에서 읽어오는 버퍼

	if (device < 0)
		return 1;
 
	printf("20133265, KMU, CHA DONGMIN\n\n");

	while(1)
	{
		device = open(DEVICE_FILE_NAME, O_RDONLY | O_NDELAY);
		if(device < 0) break;

		//battery_test의 값을 읽어들인 후, 정수로 바꾸어 출력할 것이다.
		read(device, buf, 128);
		close(device);
		battLevel = atoi(buf);

		

		//남은 양만큼 * 표시
		battLevelChar = battLevel/10;
		write(2, "Left Level : [", strlen("Left Level : ["));		

		for(i = 0; i < battLevelChar; i++)
			write(2, "*", 1);

		for(i = 0; i < (10 - battLevelChar); i++)
			write(2, "-", 1);

		printf("] => %d%c\n", battLevel, '%');
		sleep(1);
	}

}
