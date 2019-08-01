#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include <pthread.h>


static void debug_print_buf(const char *info, unsigned char buf[], int len)
{
    int  i;

    if(info)
        printf("\n****%s***len = %d*****************",info,  len);

    for(i = 0; i < len; i++)
    {
        if(!(i%16))
            printf("\n");
        printf("%02x ", buf[i]);
    }

    printf("\n\n");
}

#ifdef DEBUG
#define DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__)
#define DEBUG_PRINT_BUF(...) debug_print_buf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#define DEBUG_PRINT_BUF(...)
#endif


/****************************************************
 * 根据uart_fd属性
 *****************************************************/
static int uart_set(int fd, int nSpeed, int nBits, char nEvent, int nStop)
{
    struct termios newttys1,oldttys1;

    /*保存原有串口配置*/
    if(tcgetattr(fd,&oldttys1)!=0)
    {
        perror("Setupserial 1");
        return -1;
    }
    bzero(&newttys1,sizeof(newttys1));
    newttys1.c_cflag|=(CLOCAL|CREAD ); /*CREAD 开启串行数据接收，CLOCAL并打开本地连接模式*/

    newttys1.c_cflag &=~CSIZE;/*设置数据位*/
    /*数据位选择*/
    switch(nBits)
    {
        case 7:
            newttys1.c_cflag |=CS7;
            break;
        case 8:
            newttys1.c_cflag |=CS8;
            break;
    }
    /*设置奇偶校验位*/
    switch( nEvent )
    {
        case 1:  /*无奇偶校验*/
            newttys1.c_cflag &= ~PARENB;
            break;
        case 2:/*奇校验*/
            newttys1.c_cflag |= PARENB;/*开启奇偶校验*/
            newttys1.c_iflag |= (INPCK | ISTRIP);/*INPCK打开输入奇偶校验；ISTRIP去除字符的第八个比特  */
            newttys1.c_cflag |= PARODD;/*启用奇校验(默认为偶校验)*/
            break;
        case 3: /*偶校验*/
            newttys1.c_cflag |= PARENB; /*开启奇偶校验  */
            newttys1.c_iflag |= ( INPCK | ISTRIP);/*打开输入奇偶校验并去除字符第八个比特*/
            newttys1.c_cflag &= ~PARODD;/*启用偶校验*/
            break;
    }
    /*设置波特率*/
    switch( nSpeed )
    {
        case 2400:
            cfsetispeed(&newttys1, B2400);
            cfsetospeed(&newttys1, B2400);
            break;
        case 4800:
            cfsetispeed(&newttys1, B4800);
            cfsetospeed(&newttys1, B4800);
            break;
        case 9600:
            cfsetispeed(&newttys1, B9600);
            cfsetospeed(&newttys1, B9600);
            break;
        case 115200:
            cfsetispeed(&newttys1, B115200);
            cfsetospeed(&newttys1, B115200);
            break;
        default:
            cfsetispeed(&newttys1, B9600);
            cfsetospeed(&newttys1, B9600);
            break;
    }
    /*设置停止位*/
    if( nStop == 1)/*设置停止位；若停止位为1，则清除CSTOPB，若停止位为2，则激活CSTOPB*/
    {
        newttys1.c_cflag &= ~CSTOPB;/*默认为一位停止位； */
    }
    else if( nStop == 2)
    {
        newttys1.c_cflag |= CSTOPB;/*CSTOPB表示送两位停止位*/
    }

    /*设置最少字符和等待时间，对于接收字符和等待时间没有特别的要求时*/
    newttys1.c_cc[VTIME] = 0;/*非规范模式读取时的超时时间；*/
    newttys1.c_cc[VMIN]  = 1; /*非规范模式读取时的最小字符数*/
    tcflush(fd ,TCIFLUSH);/*tcflush清空终端未完成的输入/输出请求及数据；TCIFLUSH表示清空正收到的数据，且不读取出来 */

    /*激活配置使其生效*/
    if((tcsetattr( fd, TCSANOW,&newttys1))!=0)
    {
        perror("com set error");
        return -1;
    }

    return 0;
}



/****************************************************
 * check
 *     1:无校验
 *     2：奇校验
 *     3：偶校验
 * return fd
 *      <0 error,
 *      =0 ok.
 *****************************************************/
int sys_uart_open(char *uartname, int speed, int check)
{
    int fd;

    fd = open(uartname, O_RDWR|O_NOCTTY);
    if(fd < 0)
    {
        fprintf(stderr,"uart_open %s error\n", uartname);
        perror("open:");
        return -3;
    }

    if(uart_set(fd, speed, 8, check, 1) < 0)
    {
        fprintf(stderr,"uart set failed!\n");
        return -4;
    }

    printf("%s[%d]: fd = %d\n", __func__, __LINE__, fd);

    return fd;
}



/***************************************************
 * return
 *       <0 error
 *       =0 ok
 **************************************************/
int sys_uart_close(int uartid)
{
    close(uartid);

    return 0;
}


/*************************************************
 * return
 *       <0 error,
 *       =0 read nothing,
 *       >0 read ok.
 **************************************************/
int sys_uart_read(int uartid, unsigned char buf[], int len, int timeout_ms)
{
    int rcv_count = 0;
    fd_set rfds;
    int ret;
    struct timeval time;
    char *ptr = buf;


    while(1)
    {
        FD_ZERO(&rfds);
        FD_SET(uartid, &rfds);
        time.tv_sec = timeout_ms/1000;
        time.tv_usec = (timeout_ms%1000)*1000;

        ret = select(uartid+1, &rfds, NULL, NULL, &time);
        if(ret < 0){
            perror("sys_uart_read:select");
            return rcv_count;
        }else if(ret == 0){
            return rcv_count;
        }

        ret = read(uartid, ptr+rcv_count, len - rcv_count);
        if(ret < 0){
            if(errno == EINTR){
                ret = 0;
            }else{
                perror("sys_uart_write:read");
                return rcv_count;
            }
        }else if(ret == 0){
            //printf("%s[%d]: ret = %d\n", __func__, __LINE__, ret);
            return -3;
        }

        rcv_count += ret;

        if(rcv_count == len)
            return rcv_count;
    }
}

/*************************************************
 * return
 *       <0 error,
 *       =0 write nothing,
 *       >0 write ok.
 **************************************************/
int sys_uart_write(int uartid, unsigned char buf[], int len, int timeout_ms)
{
    size_t  nleft;
    int ret;
    const char *ptr;

    ptr = buf;
    nleft = len;

    while(nleft > 0){
        ret = write(uartid, ptr, nleft);
        if(ret < 0){
            if( errno == EINTR){
                ret = 0;
            }else {
                perror("sys_uart_write:write");
                return -3;
            }
        }

        nleft -= ret;
        ptr   += ret;
    }

    return(len - nleft);
}


/*************************************************
 * thread_rx_tx
 **************************************************/
void* thread_rx_tx(void *arg)
{
	int fd = 0;
	char *uart_name = (char *)arg;

	char wbuff[64] = "abcdefg";//{"PC: this is PC message"}; //{0};

	char rbuff[20] = {0};
	char rbuff_wanted[] = {0xFE ,0x05 ,0x90 ,0x21 ,0x00 ,0x00,0x01,0xFF};
	int ret = 0;

    fd = open(uart_name, O_RDWR|O_NOCTTY);
    if(fd < 0)
    {
        fprintf(stderr,"uart_open %s error\n", uart_name);
        perror("open:");
        return (void *)-3;
    }

    if(uart_set(fd, 115200, 8, 0, 1) < 0)
    {
        fprintf(stderr,"uart set failed!\n");
        return (void *)-4;
    }


    //sprintf(wbuff, "%s", (char*)arg);
    //wbuff[strlen(arg)] = '\0';

 //   DEBUG_PRINT("%s[%d]: fd = %d\n", __func__, __LINE__, fd);

    while (1) {
        ret = sys_uart_read(fd, rbuff, sizeof(rbuff_wanted), 10);
        if(ret > 0){
      	//DEBUG_PRINT("recive data\n");
            //DEBUG_PRINT_BUF(uart_name, rbuff, sizeof(rbuff));
	    rbuff[ret] = '\0';
	    printf("%s:receive data:%s\n", uart_name, rbuff);
            	printf("%ld\n", strlen(wbuff));
		ret = sys_uart_write(fd, wbuff, strlen(wbuff), 10000);
        }
	else
		ret = sys_uart_write(fd, wbuff, strlen(wbuff), 10000);
	//else
	//	printf("%s:not receive\n", uart_name);
    }
}




/*************************************************
 * main
 **************************************************/
int main(int argc, const char *argv[])
{
    int ret = 0;
    pthread_t tid1, tid2;
    int speed = 0;
    int check = 0;
    int var = -1;

    if(argc != 3){
        printf("Usage: %s /dev/ttyUSB0 /dev/ttyUSB1\n", argv[0]);
        return -1;
    }
    
    printf("I am pc\n");

    if(pthread_create(&tid1, NULL, thread_rx_tx, (void *)argv[1])){
        printf("thread1 create err\n");
        return -1;
    }
	
    usleep(50*1000);
    if(pthread_create(&tid2,NULL,thread_rx_tx, (void *)argv[2])){
        printf("thread2 create err\n");
        return -1;
    }

    while(1){
        sleep(1);
    }

    return 0;
}


