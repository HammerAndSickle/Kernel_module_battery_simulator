#include <linux/sched.h>
#include <linux/rcupdate.h>
#include <linux/module.h>   /* Needed by all modules */
#include <linux/kernel.h>   /* Needed for KERN_INFO */
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <asm/siginfo.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/moduleparam.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/rcupdate.h>
#include <linux/string.h>
#include <linux/pid.h>
#include <linux/debugfs.h>

MODULE_LICENSE("GPL");


#define PROCFS_MAX_SIZE         1024
#define PROCFS_TESTLEVEL        "battery_test"
#define PROCFS_NOTIFYPID        "battery_notify"
#define PROCFS_THRESHOLD        "battery_threshold"


//이 프로그램의 기능과 관련된 전역변수
static int level = 99;			//배터리 잔량
static int test_level = 0;		//테스트 배터리 잔량
static int notify_pid = -1;		//배터리 상태 알림을 받을 프로세스 ID
static int threshold = -1;		//이 값을 경계로 배터리 상태 알림이 바뀔 것이다..

static int status_changed = -1;		//전력 모드. 초기값 -1, 기본 0, 슬립 1

//이 프로그램에 사용될 버퍼 및 변수
static char lev_r_buffer[PROCFS_MAX_SIZE];	//level read 버퍼
static char thre_r_buffer[PROCFS_MAX_SIZE];	//threshold read 버퍼
static char noti_r_buffer[PROCFS_MAX_SIZE];	//notify read 버퍼

static char lev_w_buffer[PROCFS_MAX_SIZE];	//level write 버퍼
static char thre_w_buffer[PROCFS_MAX_SIZE];	//threshold write 버퍼
static char noti_w_buffer[PROCFS_MAX_SIZE];	//notify write 버퍼

static unsigned long lev_r_size = 0;		//level read 버퍼 size
static unsigned long thre_r_size = 0;		//threshold read 버퍼 size
static unsigned long noti_r_size = 0;		//notify read 버퍼 size

static unsigned long lev_w_size = 0;		//level write 버퍼 size
static unsigned long thre_w_size = 0;		//threshold write 버퍼 size
static unsigned long noti_w_size = 0;		//notify write 버퍼 size

//이 프로그램에 연결된 드라이버들
static struct proc_dir_entry *proc_battery_test_entry;		//battery_test 드라이버 엔트리
static struct proc_dir_entry *proc_battery_threshold_entry;	//battery_threshold 드라이버 엔트리
static struct proc_dir_entry *proc_battery_notify_entry;	//battery_notify 드라이버 엔트리

//유저 프로세스에 시그널을 보내는 함수.
static int send_sig_to_pid(int sig, pid_t pid)
{
	struct siginfo info;
	struct task_struct* task;

	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = sig;
	info.si_code = SI_USER; // sent by kill, sigsend, raise
	task = pid_task(find_vpid(pid), PIDTYPE_PID);

    //info.si_pid = get_current()->pid; // sender's pid
    //info.si_uid = (unsigned int)current_uid(); // sender's uid

    return send_sig_info(sig, &info, task);
}


//==============WRITE================
//battery_test에 write를 수행 : 0~100 사이의 전력을 쉘 스크립트에서 임의로 넣어 시나리오를 만들 것이다.
//쉘 스크립트는 이것을 수시로 수행할 것이다.
static int test_level_write( struct file *filp, const char *user_space_buffer, unsigned long len, loff_t *off )
{

        int status = 0;
        int requested;

        lev_w_size = len;

        if (lev_w_size > PROCFS_MAX_SIZE ) {
                lev_w_size = PROCFS_MAX_SIZE;
        }

        /* 유저레벨->커널레벨을 통해 버퍼에 값이 쓰여진다. */
        if ( copy_from_user(lev_w_buffer, user_space_buffer, lev_w_size) ) {
                return -EFAULT;
        }

	//입력되어 들어온 문자열이 int로 변환이 가능해야 한다. "100" 같은거 처럼..
        status  = kstrtoint(lev_w_buffer, 10, &requested);
        if(status < 0)
        {
                printk(KERN_INFO "Error while called kstrtoint(...)\n");
                return -ENOMEM;
        }
        // 그 값은 또 0~100 사이여야만 한다.
        if(requested< 0 || requested > 100){
                printk(KERN_INFO "Invalid battery level.\n");
                return -ENOMEM;
        }
        // 옳은 값이라면 test_level 잔량에 입력값이 적용된다.
        test_level = requested;


	//배터리 잔량에 따라서 시그널을 보내거나 한다.
	if(threshold != -1 && notify_pid != -1)
	{
		//전력이 임계값보다 작을 때 : 이전엔 일반 모드였다면 시그널 전송 후 슬립 모드로
		if(test_level < threshold)
		{
			if(status_changed == 0)
				send_sig_to_pid(SIGUSR1, notify_pid);
			status_changed = 1;

		}
		//전력이 임계값보다 클 때 : 이전엔 슬립 모드였다면 시그널 전송 후 일반 모드로
		else
		{
			if(status_changed == 1)
				send_sig_to_pid(SIGUSR2, notify_pid);
			status_changed = 0;
		}

	}
        // *off += procfs_buffer_size; // not necessary here!

        return lev_w_size;

}

//battery_threshold에 write를 수행 : 0~100 사이의 경계값을 넣을 것이다.
static int threshold_write( struct file *filp, const char *user_space_buffer, unsigned long len, loff_t *off )
{

        int status = 0;
        int requested;

        thre_w_size = len;

        if (thre_w_size > PROCFS_MAX_SIZE ) {
                thre_w_size = PROCFS_MAX_SIZE;
        }

        /* 유저레벨->커널레벨을 통해 버퍼에 값이 쓰여진다. */
        if ( copy_from_user(thre_w_buffer, user_space_buffer, thre_w_size) ) {
                return -EFAULT;
        }

	//입력되어 들어온 문자열이 int로 변환이 가능해야 한다. "100" 같은거 처럼..
        status  = kstrtoint(thre_w_buffer, 10, &requested);
        if(status < 0)
        {
                printk(KERN_INFO "Error while called kstrtoint(...)\n");
                return -ENOMEM;
        }
        // 그 값은 또 0~100 사이여야만 한다.
        if(requested< 0 || requested > 100){
                printk(KERN_INFO "Invalid battery level.\n");
                return -ENOMEM;
        }
        // 옳은 값이라면 threshold에 입력값이 적용된다.
        threshold = requested;

        // *off += procfs_buffer_size; // not necessary here!

        return thre_w_size;

}

//battery_notify에 write를 수행 : 알림이 날아갈 PID를 설정한다.
static int notify_write( struct file *filp, const char *user_space_buffer, unsigned long len, loff_t *off )
{

        int status = 0;
        int requested;

        noti_w_size = len;

        if (noti_w_size > PROCFS_MAX_SIZE ) {
                noti_w_size = PROCFS_MAX_SIZE;
        }

        /* 유저레벨->커널레벨을 통해 버퍼에 값이 쓰여진다. */
        if ( copy_from_user(noti_w_buffer, user_space_buffer, noti_w_size) ) {
                return -EFAULT;
        }

	//입력되어 들어온 문자열이 int로 변환이 가능해야 한다. "100" 같은거 처럼..
        status  = kstrtoint(noti_w_buffer, 10, &requested);
        if(status < 0)
        {
                printk(KERN_INFO "Error while called kstrtoint(...)\n");
                return -ENOMEM;
        }
        // 그 값은 음수일 수 없다.
        if(requested< 0){
                printk(KERN_INFO "Invalid pid.\n");
                return -ENOMEM;
        }
        // 옳은 값이라면 notify_pid에 적용된다.
        notify_pid = requested;

        // *off += procfs_buffer_size; // not necessary here!

        return noti_w_size;

}



//==============READ================
//battery_test에 read를 수행 : 0~100 사이의 전력을 읽어 들이도록 한다.
static int test_level_read( struct file *filp, char *user_space_buffer, size_t count, loff_t *off )
{
        int ret = 0;
        int flag = 0;

        if(*off < 0) *off = 0;

        snprintf(lev_r_buffer, 16, "%d", test_level);
        lev_r_size = strlen(lev_r_buffer);

        if(*off > lev_r_size){
                return -EFAULT;
        }else if(*off == lev_r_size){
                return 0;
        }

        if(lev_r_size - *off > count)
                ret = count;
        else
                ret = lev_r_size - *off;



	//유저 레벨의 버퍼에(유저가 read에 달아놓은) 드라이버의 버퍼 내용이 이동된다.
        flag = copy_to_user(user_space_buffer, lev_r_buffer + (*off), ret);

        if(flag < 0)
                return -EFAULT;

        *off += ret;

        return ret;

}

//battery_threshold에 read를 수행 : 0~100 사이의 경계값을 지정
static int threshold_read( struct file *filp, char *user_space_buffer, size_t count, loff_t *off )
{
        int ret = 0;
        int flag = 0;

        if(*off < 0) *off = 0;

        snprintf(thre_r_buffer, 16, "%d", threshold);
        thre_r_size = strlen(thre_r_buffer);

        if(*off > thre_r_size){
                return -EFAULT;
        }else if(*off == thre_r_size){
                return 0;
        }

        if(thre_r_size - *off > count)
                ret = count;
        else
                ret = thre_r_size - *off;



	//유저 레벨의 버퍼에(유저가 read에 달아놓은) 드라이버의 버퍼 내용이 이동된다.
        flag = copy_to_user(user_space_buffer, thre_r_buffer + (*off), ret);

        if(flag < 0)
                return -EFAULT;

        *off += ret;

        return ret;

}

//battery_notify에 read를 수행 : PID를 읽어 들이도록 한다.
static int notify_read( struct file *filp, char *user_space_buffer, size_t count, loff_t *off )
{
        int ret = 0;
        int flag = 0;

        if(*off < 0) *off = 0;

        snprintf(noti_r_buffer, 16, "%d", notify_pid);
        noti_r_size = strlen(noti_r_buffer);

        if(*off > noti_r_size){
                return -EFAULT;
        }else if(*off == noti_r_size){
                return 0;
        }

        if(noti_r_size - *off > count)
                ret = count;
        else
                ret = noti_r_size - *off;



	//유저 레벨의 버퍼에(유저가 read에 달아놓은) 드라이버의 버퍼 내용이 이동된다.
        flag = copy_to_user(user_space_buffer, noti_r_buffer + (*off), ret);

        if(flag < 0)
                return -EFAULT;

        *off += ret;

        return ret;

}




//================================= 시스템 콜 핸들러

//test_level을 다룰 때 호출될 함수 리스트. (/proc/battery_test 가 드라이버 역할)
static const struct file_operations proc_test = {
        .write = test_level_write,
        .read = test_level_read,
};

//threshold을 다룰 때 호출될 함수 리스트. (/proc/battery_threshold 가 드라이버 역할)
static const struct file_operations proc_threshold = {
        .write = threshold_write,
        .read = threshold_read,
};

//notify_pid을 다룰 때 호출될 함수 리스트. (/proc/battery_notify 가 드라이버 역할)
static const struct file_operations proc_notify = {
        .write = notify_write,
        .read = notify_read,
};




int init_module(void)
{

        int ret = 0;

        proc_battery_test_entry = proc_create(PROCFS_TESTLEVEL, 0666, NULL, &proc_test);
        proc_battery_threshold_entry = proc_create(PROCFS_THRESHOLD, 0666, NULL, &proc_threshold);
        proc_battery_notify_entry = proc_create(PROCFS_NOTIFYPID, 0666, NULL, &proc_notify);

        if(proc_battery_test_entry == NULL || proc_battery_threshold_entry == NULL || proc_battery_notify_entry == NULL)
        {
                return -ENOMEM;
        }
        return ret;

}

//모듈을 제거, 깨끗이 정리할 때
void cleanup_module(void)
{
        remove_proc_entry(PROCFS_TESTLEVEL, proc_battery_test_entry);
	remove_proc_entry(PROCFS_NOTIFYPID, proc_battery_threshold_entry);
	remove_proc_entry(PROCFS_THRESHOLD, proc_battery_notify_entry);
}
