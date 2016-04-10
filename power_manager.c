//기본적으로 전력 관리 상태를 화면에 알려주나..
//배터리의 경계값을 지정하거나, 알림 pid 번호를 지정하거나, 시그널을 받아들이는 코드이다.

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

#define BATT_NOTIFY "/proc/battery_notify"
#define BATT_THRESHOLD "/proc/battery_threshold"

static int status = 0;		//전력 관리 상태(0:기본, 1:슬립)

static void my_handler(int signum)
{
	switch(signum)
	{
	case SIGUSR1:
		write(2, "\n[Sleep Mode]\n", 14);
		status = 1;
		break;
	case SIGUSR2:
		write(2, "\n[Normal Mode]\n", 15);
		status = 0;
		break;
	}

	return;
}


int main()
{
	int device_notify;	//BATT_NOTIFY 모듈
	int device_threshold;	//BATT_THRESHOLD 모듈

	int notifyPid;		//알림을 보낼 프로세스
	int levelThreshold;	//남은 배터리 양을 문자로 나타냄(battLevel / 10)


	int i;
	const char s[2] = " ";
	char* token;
	char cmdbuf[64] = {0, };	//커맨드 입력 버퍼
	char buf[128] = {0, };	//모듈 버퍼



	//전력 모드 시그널 핸들러를 등록한다
	signal(SIGUSR1, my_handler);
	signal(SIGUSR2, my_handler);

	while(1)
	{
		printf("power manager pid : %d\n", getpid());
		printf("============== COMMAND =====================\n");
		printf("cpid [pid] : set Pid to receive signals from module\n");
		printf("cthr [0~100] : set threshold of level which will act as a triggering signal\n");

		printf(" > ");
		fgets(cmdbuf, 64, stdin);

		token = strtok(cmdbuf, s);

		if(token != NULL)
		{	
			//change pid
			if(strcmp("cpid", token) == 0)
			{

				token = strtok(NULL, s);
				strcpy(buf, token);
				
				printf("%d\n", atoi(buf));

				device_notify = open(BATT_NOTIFY, O_WRONLY | O_NDELAY);
				
				if (device_notify < 0)
					break;
				write(device_notify, buf, 128);
				close(device_notify);
			}
			//change threshold
			else if(strcmp("cthr", token) == 0)
			{
				token = strtok(NULL, s);
				strcpy(buf, token);

				printf("%d\n", atoi(buf));

				device_threshold = open(BATT_THRESHOLD, O_WRONLY | O_NDELAY);
				
				if (device_threshold < 0)
					break;

				write(device_threshold, buf, 128);
				close(device_threshold);
			}

		}
		
	}
}
