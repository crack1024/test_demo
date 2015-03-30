/**
 * $Id: main.c 2014-11
 *
 * @brief DMA main Module.
 *
 * @Author Eric
 *
 * This part of code is written in C programming language.
 * Please visit http://en.wikipedia.org/wiki/C_(programming_language)
 * for more details on the language used herein.
 */
#define _GNU_SOURCE 1

#include <signal.h>
#include <arpa/inet.h>
#include "../dma-hw/dma_hw.h"


#define  LENGTH_MIN           8
#define  LENGTH_MAX           4088

/**/
unsigned int cmd_buff[2];

//int fd, rx_length = 256 * 1024;

typedef struct sockaddr SA;
int listenfd = -1,      connfd = -1;
struct sockaddr_in myaddr, peeraddr;
socklen_t peerlen = sizeof(peeraddr);

pthread_t adc_thread = 0, dac_thread = 0;
pthread_mutex_t adc_lock;

unsigned char *adc_mem_addr;
unsigned char *dma_reg_addr_wr;
unsigned char *dac_mem_addr;
unsigned char *dma_reg_addr_rd;


/**
 * Function declaration
 */

int rx_func(void *arg);
int tx_func(void *arg);
static void sig_pipe(int signo);


int main (int argc, char *argv[])
{

	int ret;
	struct timeval tv;
	int nSendBufLen = 256*1024;

	if (signal(SIGPIPE, sig_pipe) == SIG_ERR)
	{
		fprintf(stderr, "can't catch SIGPIPE\n");
		exit(1);
	}

	pthread_mutex_init(&adc_lock, NULL);

	/**
	 * Memory Mapping
	 */
	adc_mem_addr = map_memory(S2MM_DST_ADDR, S2MM_BUF_SIZE);
	memset(adc_mem_addr, 0x0, S2MM_BUF_SIZE);
	dac_mem_addr = map_memory(MM2S_SRC_ADDR, MM2S_BUF_SIZE);
	memset(dac_mem_addr, 0x0,  MM2S_BUF_SIZE);

	/**
	 * Axi-dma reg Mapping
	 */
	dma_reg_addr_wr = map_memory(AXI_DMA_BASE_WR, REG_SIZE);
	dma_reg_addr_rd = map_memory(AXI_DMA_BASE_RD, REG_SIZE);

	/**
	 * Tcp server
	 */
	if((listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("fail to socket!");
		exit(-1);
	}

	bzero(&myaddr, sizeof(myaddr));
	myaddr.sin_family = PF_INET;
	myaddr.sin_port = htons(8000);
	myaddr.sin_addr.s_addr = htons(INADDR_ANY);

	if(bind(listenfd, (SA *)&myaddr, sizeof(myaddr)) < 0)
	{
		perror("fail to bind!");
		exit(-1);
	}

	listen(listenfd, 1);

	while(1)
	{
		if((connfd = accept(listenfd, (SA *)&peeraddr, &peerlen)) < 0)
		{
			perror("fail to accept!");
			exit(-1);
		}
#if 1
		tv.tv_sec = 10;
		tv.tv_usec = 0;
#define  RECV_TIMEOUT
#ifdef  RECV_TIMEOUT
		setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO|SO_SNDTIMEO, &tv, sizeof(tv));
#else
		setsockopt(connfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
		setsockopt(connfd, SOL_SOCKET, SO_SNDBUF, (const char *)&nSendBufLen, sizeof(int) );
#endif
		dma_dbg(MSG_DEBUG,"Connection from [%s:%d]\n", inet_ntoa(peeraddr.sin_addr), ntohs(peeraddr.sin_port));
		tx_func(NULL);
		dma_dbg(MSG_DEBUG,"Connection from [%s:%d] is closed\n", inet_ntoa(peeraddr.sin_addr), ntohs(peeraddr.sin_port));
		close(connfd);
	}

	return 0;
}

/**
 * @brief  recv data from axi dma ipcore function.
 *
 * @param[in]  arg                user parameters
 * @retval                        NULL
 *
 */
int rx_func(void* arg)
{
	//iowrite32(0, (unsigned long)(dma_reg_addr_wr+0x10));
	//iowrite32(1000, (unsigned long)(dma_reg_addr_wr+0x14));
	do
	{
		//dma_dbg(MSG_DEBUG,"rx start ........\n");
		axi_dma_init(S2MM_CHANNEL, 1000, 0, S2MM_DST_ADDR, (unsigned long)dma_reg_addr_wr);
		axi_dma_start(S2MM_CHANNEL, 0, (unsigned long)dma_reg_addr_wr);
		check_dma_done(S2MM_CHANNEL, (unsigned long)dma_reg_addr_wr);
		axi_dma_reset(S2MM_CHANNEL, (unsigned long)dma_reg_addr_wr, FIFO_IP_RESET);
		//dma_dbg(MSG_DEBUG,"rx complete ........\n");
		if(send(connfd, (void *)adc_mem_addr, 1000, 0)<=0){
			dma_dbg(MSG_DEBUG,"send:errno=%s\n", strerror(errno));
			return -1;
		}
	}while(1);

	return 0;
}

/**
 * @brief  send data to axi dma thread.
 *
 * @param[in]  temp               user parameters
 * @retval                        NULL
 *
 */
int tx_func(void *arg)
{
	do
	{
		if(recv(connfd, (void *)dac_mem_addr, 2048, MSG_WAITALL) <= 0)
			break;
		axi_dma_init(MM2S_CHANNEL, 2048, 0, MM2S_SRC_ADDR, (unsigned long)dma_reg_addr_rd);
		axi_dma_start(MM2S_CHANNEL, 1, (unsigned long)dma_reg_addr_rd);
		check_dma_done(MM2S_CHANNEL, (unsigned long)dma_reg_addr_rd);
		axi_dma_reset(MM2S_CHANNEL, (unsigned long)dma_reg_addr_rd, FIFO_IP_RESET);

	}while(1);

	return 0;
}
/**
 * @brief  SIGPIPE handle to avoid program quit from wrong network connection
 *
 * @param[in]  signo              signal id
 * @retval                        void
 *
 */
static void sig_pipe(int signo)
{
	/*nothing to do*/
}

