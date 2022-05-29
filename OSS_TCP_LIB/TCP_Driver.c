/*-----------------------------------------------------------
**
**
** 文件名:  TCP_Driver.c
**
** 描述:    定义了通过TCP进行通信所需的所有函数和变量
**
** 定义的函数:
**    TCP_Init()
**    TCP_Send()
**    TCP_Recv()
**
** 设计注记:
**    默认采用TCP非阻塞的方式接收
**    只在嵌入式系统vxworks和windows下多设备的多路复用的简单使用，不适用于高并发环境。linux下select默认1024个fd，
**    0模式(Mode)仅支持点对点。可配合改版PPP协议（支持最大16MB一包数据）使用避免粘包(大包很常见)。1模式支持服务端可对应多个客户端。服务端数据需要根据应用层解析。PPP协议不再适用。
**	  0模式(Mode)点对点的方式服务端有数据收发就accept，客户端有数据收发就connect。支持客户端服务端的任意一方关机重启还能重新连接。
**    所以用户只需要send或者recv，(长度为0也可以，但一定要调用send或者recv,无需手动accept或者connect。)
**	  1模式(Mode)Select方式暂不完善，因为需要开线程接收数据，目前只也只支持点对点。需要服务端主动周期调用Recv，否则无法select以执行accept。
**    1模式(Mode)Select方式需要客户端主动周期调用Send，否则无法select以执行connect。
**
** 更改历史:
**    2022年4月3日  创建本文件
**    $Revision$
**    $Date$
**    $Author$
**    $Log$
**
**-----------------------------------------------------------
*/
/*TCP运行环境*/
#ifdef _WIN32
#define TCP_UNDER_VXWORKS   0
#else
#define TCP_UNDER_VXWORKS   2 /*Windows C++运行环境下定义为0, vxWorks5.x环境下定义为1, vxWorks6.x环境下定义为2*/
#endif

#define MY_MAX(x, y)  ((x) > (y) ? (x) : (y))
#define CLIENT_MAX_NUM  64
#define SERVER_MAX_NUM  8

#if TCP_UNDER_VXWORKS > 0
	#include <vxWorks.h>
	#include <ioLib.h>
#ifdef SYLIXOS
	#include <socket.h>
#else
	#include <sockLib.h>
#endif
	#include <inetLib.h>
	#include <stdio.h>
	#include <errno.h>
	#include <errnoLib.h>
	#include <inetLib.h>
	#include <selectLib.h>
	#include <fcntl.h>
	#include <taskLib.h>
	#include <sysLib.h>
	#include <string.h>
	
	#include<sys/types.h>
	#include<netinet/in.h>
	#include<sys/time.h>
	#include<arpa/inet.h>
	#include<netinet/tcp.h>

	#include "TCP_Driver.h"

#if TCP_UNDER_VXWORKS == 2
	#ifndef SOCKADDR
        //typedef struct sockaddr SOCKADDR;
	#endif
	#ifndef SYLIXOS
		typedef unsigned int SOCKET;
	#endif
#endif

#else /*Windows C VERSION*/
	#include <WinSock2.h>  /*<WinSock2.h>必须在window.h之前*/
    #include <stdio.h>
    #include <mstcpip.h>
	#pragma comment(lib, "WS2_32.lib")

	#include "TCP_Driver.h"
	typedef char* caddr_t;
	static WSADATA wsaData;
#endif


typedef struct sockaddr_in SockAddr_TYPE;

typedef struct
{
	int(*Send)(void * const Handler, void * const Payload, int const LenBytes);
	int(*Recv)(void * const Handler, void * const Buffer, int const MaxBytes);

	SOCKET		  Recv_Socket;  /*接收和发送的socket*/
	SOCKET		  Listen_Socket;/*服务端专用的监听的socket*/
	//SOCKET		  Server_Socket[CLIENT_MAX_NUM];	/*一个服务器暂定不超过64个客户端*/
	SockAddr_TYPE Self_Recv_Address;/*配置接收IP与端口号，用于创建Socket*/
	SockAddr_TYPE Peer_Recv_Address;/*配置目的IP与端口号，用于指定发送地址*/
	int Max_Fd;
	int Time_Out;
	int Type;				/*0服务端 1客户端*/
	int Mode;				/*0-点对点模式 1-C/S select模式*/
	int Accepted;			/*0:未accept  1:已accept   Type为0,Mode为0有效*/
	int Connected;			/*0:未connect 1:已connect  Type为1,Mode为0有效*/
    int CurCacheSize;
	int FirstFlag;
	fd_set* prfds;
} TCP_INNER_TYPE;

/*.BH-------------------------------------------------------------
**
** 函数名: TCP_Close
**
** 描述: 关闭TCP
**
** 输入参数:
**          tcp_id  : TCP的SOCKET号
**
** 输出参数:
**          返回值:         0, 成功
**                         -1, TCP_ID非法
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
int TCP_Close(TCP_STRUCT_TYPE * const Handler)
{
    TCP_INNER_TYPE *pTCP = (TCP_INNER_TYPE*)Handler;

    if (pTCP == NULL)
        return -1;
	if (pTCP->Type == 1){
	#if TCP_UNDER_VXWORKS > 0
		shutdown(pTCP->Recv_Socket, SHUT_RDWR);
		close(pTCP->Recv_Socket);
	#else
		shutdown(pTCP->Recv_Socket, SD_BOTH);
		closesocket(pTCP->Recv_Socket);
	#endif

	}
    return 0;
}
/* END of TCP_Close */

/*.BH-------------------------------------------------------------
**
** 函数名: TCP_Connect
**
** 描述: TCP连接函数，客户端主动发起三次握手
**
** 输入参数:
**          Handler : void * const, TCP句柄
**
** 输出参数:
**          返回值: int, >  0,   Connect成功
**                         -1, 句柄无效
**                         -2, 非阻塞返回的正常值
**                         -3, 该SOCKET已连接
**						   -4，  Connect失败
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
/* END of TCP_Connect */
int TCP_Connect(TCP_STRUCT_TYPE * const Handler)
{
	TCP_INNER_TYPE * pTCP = (TCP_INNER_TYPE*)Handler;
	int ret_val = 0;
	int connect_ret = 0;
	int err;

	if (pTCP == NULL)
		ret_val = -1;

	if (pTCP->Connected == 0){	/*如果是客户端*/
		connect_ret = connect(pTCP->Recv_Socket, (SOCKADDR*)&pTCP->Peer_Recv_Address, sizeof(SockAddr_TYPE));
#if TCP_UNDER_VXWORKS > 0
		if (ERROR == connect_ret)
		{
			err = errnoGet();
			/*非阻塞模式下的操作 -2是正常现象 -4是非正常情况，需要清socket*/
			if (err == EWOULDBLOCK || err == EINVAL){
				taskDelay(sysClkRateGet() / 500);
				pTCP->Connected = 0;
				ret_val = -2;
			}else if (err == EISCONN){
				printf("TCP SOCK %d already connected when trying to connect server:%s, port = %d!\n\n", pTCP->Recv_Socket,
					     inet_ntoa(pTCP->Peer_Recv_Address.sin_addr), ntohs(pTCP->Peer_Recv_Address.sin_port));
				pTCP->Connected = 1;
				ret_val = -3;
			}else{
				//printf("TCP SOCK %d connected err, ret = %d, errno = %d\n", pTCP->Recv_Socket, ret_val, WSAGetLastError());
				//closesocket(pTCP->Recv_Socket);
				//WSACleanup();
				pTCP->Connected = 0;
				ret_val = -4;
			}

			return ret_val;
		}
#else
		if (SOCKET_ERROR == connect_ret)
		{
			err = WSAGetLastError();
            /*非阻塞模式下的操作 -2是正常现象 -4是非正常情况，需要清socket*/
			if (err == WSAEWOULDBLOCK || err == WSAEINVAL){
				Sleep(2);	/*值调整得太大容易导致任务周期不稳定*/
                pTCP->Connected = 0;
				ret_val = -2;
			}else if (err == WSAEISCONN){
				printf("\nTCP SOCK %d already connected when trying to connect server:%s, port = %d!\n\n", pTCP->Recv_Socket,
						inet_ntoa(pTCP->Peer_Recv_Address.sin_addr), ntohs(pTCP->Peer_Recv_Address.sin_port));
                pTCP->Connected = 1;
				ret_val = -3;
			}else{
				//printf("TCP SOCK %d connected err, ret = %d, errno = %d\n", pTCP->Recv_Socket, ret_val, WSAGetLastError());
				//closesocket(pTCP->Recv_Socket);
				//WSACleanup();
                pTCP->Connected = 0;
				ret_val = -4;
			}

			return ret_val;
		}
#endif
		else{
			pTCP->Connected = 1;
            printf("\nTCP SOCK %d connected to server:%s, port = %d!\n\n", pTCP->Recv_Socket,
					inet_ntoa(pTCP->Peer_Recv_Address.sin_addr), ntohs(pTCP->Peer_Recv_Address.sin_port));
			ret_val = 0;
			return ret_val;
		}
	}
	else{
		ret_val = 0;
		return ret_val;
	}
}

/*.BH-------------------------------------------------------------
**
** 函数名: TCP_Accept
**
** 描述: TCP Accept函数
**
** 输入参数:
**          Handler : void * const, TCP句柄
**
** 输出参数:
**          返回值: int, >    0, Accept成功
**                         -1, 句柄无效
**                         -2, 非阻塞返回的正常值
**                         -3, Accept失败
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
/* END of TCP_Accept */
int TCP_Accept(TCP_STRUCT_TYPE * const Handler)
{
	TCP_INNER_TYPE * pTCP = (TCP_INNER_TYPE*)Handler;
	int ret_val = 0;
	int err = 0;
	SOCKET accept_ret = 0;
	SockAddr_TYPE Source_Address;
	int Source_AddrSize = sizeof(SockAddr_TYPE);
	
	if (pTCP == NULL)
		ret_val = -1;

	if (pTCP->Accepted == 0){	/*如果是服务端且当前未accept*/
		accept_ret = accept(pTCP->Listen_Socket, (SOCKADDR*)&Source_Address, &Source_AddrSize);
#if TCP_UNDER_VXWORKS > 0
		if (ERROR == accept_ret)
		{
			/*非阻塞模式下的操作 -2是正常现象 -3是非正常情况，需要清socket*/
			err = errnoGet();
			if (err == EWOULDBLOCK)
			{
				taskDelay(sysClkRateGet()/500);
				ret_val = -2;
			}
			else
			{
				close(pTCP->Listen_Socket);
				ret_val = -3;
			}
			pTCP->Accepted = 0;
			return ret_val;
		}
#else
		if (INVALID_SOCKET == accept_ret)
		{
			/*非阻塞模式下的操作 -2是正常现象 -3是非正常情况，需要清socket*/
			err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK)
			{
				Sleep(2);
				ret_val = -2;
			}
			else
			{
				closesocket(pTCP->Listen_Socket);
				WSACleanup();
				ret_val = -3;
			}
			pTCP->Accepted = 0;
			return ret_val;
		}
#endif
		else{
			pTCP->Accepted = 1; 
			pTCP->Recv_Socket = accept_ret;
			printf("TCP SOCK %d got a connection from %s:%d\n\n", pTCP->Recv_Socket, inet_ntoa(Source_Address.sin_addr), 
					ntohs(Source_Address.sin_port));
			return ret_val;
		}
	}
	else{
		return ret_val;
	}
}

/*.BH-------------------------------------------------------------
**
** 函数名: TCP_Send
**
** 描述: TCP发送函数
**
** 输入参数:
**          Handler : void * const, TCP句柄
**          Payload : void * const, 发送数据指针
**          LenBytes: int const, 发送数据长度
**
** 输出参数:
**          返回值: int, >  0, 发送成功(返回发送数据长度, 单位:BYTE)
**                         -1, 句柄无效
**                         -2, Payload为空或LenBytes小于1
**                         -3, Connect错误
**                         -4, mode参数错误
**						   -5, 发送返回错误
**						   
**
** 设计注记:仅用于点对点通信
**
**.EH-------------------------------------------------------------
*/
/* END of TCP_Send */
int TCP_Send(void * const Handler, void * const Payload, int const LenBytes)
{
	TCP_INNER_TYPE * pTCP = (TCP_INNER_TYPE*)Handler;
	int ret_val = 0;
	int err;

	if (pTCP == NULL){
		ret_val = -1;
		return ret_val;
	}
		
	if (Payload == NULL){
		ret_val = -2;
		return ret_val;
	}
		
	if (pTCP->Type == 0){
		ret_val = TCP_Accept(pTCP);
		if (ret_val < 0){
			//printf("ACCEPT failed! accept_ret = &d\n", accept_ret);
			ret_val = -3;
			return ret_val;
		}
	}
	else if (pTCP->Type == 1){
		ret_val = TCP_Connect(pTCP);
		if (ret_val < 0){
			//printf("connection failed! connect_ret = &d\n", connect_ret);
			ret_val = -3;
			return ret_val;
		}
	}
	else{
		ret_val = -4;	/*mode invalid, do nothing*/
		return ret_val;
	}
	
	if (LenBytes < 1){
		ret_val = -2;
		return ret_val;
	}

	ret_val = send(pTCP->Recv_Socket,   (caddr_t)Payload, LenBytes, 0/*MSG_NOSIGNAL*/);
	if (ret_val <= 0){
#if TCP_UNDER_VXWORKS > 0
		//printf("[TCP_Send]err %d\n", errnoGet());
		err = errnoGet();
		if((err == EWOULDBLOCK) || (err == EINTR) || (err == EAGAIN)){
			ret_val = 0;
			return ret_val;
		}
#else
		//printf("[TCP_Send]err %d\n", WSAGetLastError());
		err = WSAGetLastError();
		if((err == WSAEWOULDBLOCK) || (err == WSAEINTR)){
			ret_val = 0;
			return ret_val;
		}
#endif
		/*对端意外断开连接，WSAGetLastError可能的值：10054L*/
		/*对端按照要求shutdown或者close的方式连接，WSAGetLastError可能的值：0*/
		if (pTCP->Type == 0){
			pTCP->Accepted = 0;    /*认为已经断开连接，重新accept即可无需关闭socket*/
			ret_val - -5;
		}
		if (pTCP->Type == 1){
			pTCP->Connected = 0;   /*对于客户端而言，如果已经断开连接乐，需要重新connect，且重新初始化socket，才能连接上服务端。服务端不需要*/
			#if TCP_UNDER_VXWORKS > 0
            	close(pTCP->Recv_Socket);
			#else
				closesocket(pTCP->Recv_Socket);
			#endif
			TCP_Init(pTCP, ntohl(pTCP->Peer_Recv_Address.sin_addr.s_addr), ntohs(pTCP->Peer_Recv_Address.sin_port),
							ntohl(pTCP->Self_Recv_Address.sin_addr.s_addr), ntohs(pTCP->Self_Recv_Address.sin_port),
							pTCP->CurCacheSize, TCP_RECV_NONBLK, 1, 0);
			ret_val - -5;
		}
	}

	return ret_val;
}

/*.BH-------------------------------------------------------------
**
** 函数名: TCP_Recv
**
** 描述: TCP接收函数
**
** 输入参数:
**          Handler : void * const, TCP句柄
**          Buffer  : void * const, 存放接收数据的缓存指针
**          MaxBytes: int const, Buffer的最大容量
**
** 输出参数:
**          返回值: int, >= 1, 实际获取到的数据长度(单位:BYTE)
**                          0, 无数据可接收
**                         -1, 句柄无效
**                         -2, Buffer为空或MaxBytes长度不足
**                         -3, Accept错误
**                         -4, mode参数错误
**						   -5, 接收返回错误
**
** 设计注记: 仅用于点对点通信
**
**.EH-------------------------------------------------------------
*/
int TCP_Recv(void * const Handler, void * const Buffer, int const MaxBytes)
{
	TCP_INNER_TYPE * pTCP = (TCP_INNER_TYPE*)Handler;
	int ret_val = 0;
	int err;

	if (pTCP == NULL){
		ret_val = -1;
		return ret_val;
	}
	if (Buffer == NULL){
		ret_val = -2;
		return ret_val;
	}

	if (pTCP->Type == 0){
		ret_val = TCP_Accept(pTCP);
		if (ret_val < 0){
			//printf("ACCEPT failed! accept_ret = &d\n", accept_ret);
			ret_val = -3;
			return ret_val;
		}
	}
	else if (pTCP->Type == 1){
		ret_val = TCP_Connect(pTCP);
		if (ret_val < 0){
			//printf("connection failed! connect_ret = &d\n", connect_ret);
			ret_val = -3;
			return ret_val;
		}
	}
	else{
		ret_val = -4;	/*mode invalid, do nothing*/
		return ret_val;
	}

	if (MaxBytes < 1){
		ret_val = -2;
		return ret_val;
	}
	ret_val = recv(pTCP->Recv_Socket, (char*)Buffer, MaxBytes, 0);	  
	if (ret_val <= 0){
#if TCP_UNDER_VXWORKS > 0
		//printf("[TCP_Recv]err %d\n", errnoGet());
		err = errnoGet();
		if((err == EWOULDBLOCK) || (err == EINTR) || (err == EAGAIN)){
			ret_val = 0;
			return ret_val;
		}
#else
		//printf("[TCP_Recv]err %d\n", WSAGetLastError());
		err = WSAGetLastError();
		if((err == WSAEWOULDBLOCK) || (err == WSAEINTR)){
			ret_val = 0;
			return ret_val;
		}
#endif
		/*对端意外断开连接，WSAGetLastError可能的值：10054L*/
		/*对端按照要求shutdown或者close的方式连接，WSAGetLastError可能的值：0*/
		if (pTCP->Type == 0){
			pTCP->Accepted = 0;    /*认为已经断开连接，重新accept即可无需关闭socket*/
			ret_val = -5;
		}
		if (pTCP->Type == 1){
			pTCP->Connected = 0;   /*对于客户端而言，如果已经断开连接乐，需要重新connect，且重新初始化socket，才能连接上服务端。服务端不需要*/
			#if TCP_UNDER_VXWORKS > 0
				close(pTCP->Recv_Socket);
			#else
				closesocket(pTCP->Recv_Socket);
			#endif
			TCP_Init(pTCP, ntohl(pTCP->Peer_Recv_Address.sin_addr.s_addr), ntohs(pTCP->Peer_Recv_Address.sin_port),
							ntohl(pTCP->Self_Recv_Address.sin_addr.s_addr), ntohs(pTCP->Self_Recv_Address.sin_port),
							pTCP->CurCacheSize, TCP_RECV_NONBLK, 1, 0);
			ret_val = -5;
		}	
	}

	return ret_val;
}
/* END of TCP_Recv */

/*.BH-------------------------------------------------------------
**
** 函数名: TCP_BroadCast_Send
**
** 描述: TCP服务端广播发送函数(给当前左右连接的客户端发送相同的数据)
**
** 输入参数:
**          Handler : void * const, TCP句柄
**          Payload : void * const, 发送数据指针
**          LenBytes: int const, 发送数据长度
**
** 输出参数:
**          返回值: int, >  0, 发送成功(返回发送数据长度, 单位:BYTE)
**                         -1, 句柄无效
**                         -2, Payload为空或LenBytes小于1
**                         -3, 类型错误
**                         -4, 发送失败
**						   
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
/* END of TCP_BroadCast_Send */
int TCP_BroadCast_Send(void * const Handler, void * const Payload, int const LenBytes)
{
	TCP_INNER_TYPE * pTCP = (TCP_INNER_TYPE*)Handler;
	int ret_val = 0;
	int err;
	int fd;
	int i;

	if (pTCP == NULL){
		ret_val = -1;
		return ret_val;
	}
		
	if ((Payload == NULL) || (LenBytes < 1)){
		ret_val = -2;
		return ret_val;
	}
		
	if (pTCP->Type != 0){
		ret_val = -3;
		return ret_val;
	}
	
	//遍历所有文件描述符，确定当前所有连接的客户端
    for (fd = 0; fd <= pTCP->Max_Fd; fd++) 
	{
		if (fd == pTCP->Listen_Socket || !FD_ISSET(fd, pTCP->prfds)) {
			continue;
		}
		/*send最后一个参数：
		windows下一般设置为0
		linux下最好设置为MSG_NOSIGNAL,如果不设置,在发送出错后有可能会导致程序退出*/
		ret_val = send(fd, (caddr_t)Payload, LenBytes, 0/*MSG_NOSIGNAL*/);
		if (ret_val < 0){
	#if TCP_UNDER_VXWORKS > 0
			//printf("[TCP_Send]err %d\n", errnoGet());
			err = errnoGet();
			if((err != EWOULDBLOCK) && (err != EINTR) && (err != EAGAIN)){
				continue;
			}
	#else
			//printf("[TCP_Send]err %d\n", WSAGetLastError());
			err = WSAGetLastError();
			if((err != WSAEWOULDBLOCK) && (err != WSAEINTR)){
				continue;
			}
	#endif
		}
		if (ret_val == 0){
			/*对端关闭了， 需要close本端socket。但是作为服务端，不需要重新初始化*/
			#if TCP_UNDER_VXWORKS > 0
				close(fd);
			#else
				closesocket(fd);
			#endif

			//pTCP->Server_Socket[i] = -1;
			FD_CLR(fd, pTCP->prfds);
			ret_val = -4;
		}

	}
	
	return ret_val;
}

/*.BH-------------------------------------------------------------
**
** 函数名: TCP_Server_Recv
**
** 描述: TCP服务端多路复用接收函数（Select同时监控accept和recv的fd）
**
** 输入参数:
**          Handler : void * const, TCP句柄
**          Buffer  : void * const, 存放接收数据的缓存指针
**          MaxBytes: int const, Buffer的最大容量
**
** 输出参数:
**          返回值: int, >= 1, 实际获取到的数据长度(单位:BYTE)
**                          0, 无数据可接收
**                         -1, 句柄无效
**                         -2, Buffer为空或MaxBytes长度不足
**                         -3, select出错
**						   -4, select为1，recv为0 表示对端关闭链接
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
int TCP_Server_Recv(void * const Handler, void * const Buffer, int const MaxBytes)
{
	TCP_INNER_TYPE * pTCP = (TCP_INNER_TYPE*)Handler;
	SockAddr_TYPE Source_Address;
	int Source_AddrSize = sizeof(SockAddr_TYPE);
	int ret_sel, ret_val, fd, i;

	if (pTCP == NULL){
		ret_val = -1;
		return ret_val;
	}
	if ((Buffer == NULL) || (MaxBytes < 1)){
		ret_val = -2;
		return ret_val;
	}

	if((pTCP->FirstFlag == 1) && (pTCP != NULL)){
	    FD_ZERO(pTCP->prfds);
		FD_SET(pTCP->Listen_Socket, pTCP->prfds);
		pTCP->Max_Fd = MY_MAX(pTCP->Max_Fd, pTCP->Listen_Socket);
		pTCP->FirstFlag = 0;
	}

    //监听所有文件描述符(select的第一个参数取当前所有fd的最大值)
	ret_sel = select(pTCP->Max_Fd + 1, pTCP->prfds, NULL, NULL, NULL);
    if (ret_sel < 0) {
#if TCP_UNDER_VXWORKS > 0
		if (EINTR == errnoGet())
			return 0;
#else
        if (WSAEINTR == WSAGetLastError())
			return 0;
#endif	
        fprintf(stderr, "%s(): select() error: %d", __FUNCTION__, errno);
        return -3;	//报错且不能再执行
    }
	//这里是否要增加select返回值为0（超时）的情况？
    if (FD_ISSET(pTCP->Listen_Socket, pTCP->prfds)) { /* client connected */

        fd = accept(pTCP->Listen_Socket, (SOCKADDR *)&Source_Address, &Source_AddrSize);
        if (fd != -1) {
            //把建议链接的客户端文件描述符添加到监听集合
            FD_SET(fd, pTCP->prfds);
            pTCP->Max_Fd = MY_MAX(pTCP->Max_Fd, fd);
			printf("\nTCP SOCK %d got a connection from %s:%d\n\n", fd, inet_ntoa(Source_Address.sin_addr), 
									ntohs(Source_Address.sin_port));
			//设置pTCP句柄自维护集合
			// for(i = 0 ; i < CLIENT_MAX_NUM; i++){
			// 	if(pTCP->Server_Socket[i] == -1)
			// 		pTCP->Server_Socket[i] = fd;
			// }
        }
    }
    //遍历所有文件描述符，接收客户端的数据
    for(fd = 0; fd < pTCP->Max_Fd; ++fd){
		if (fd == pTCP->Listen_Socket || !FD_ISSET(fd, pTCP->prfds)) {
			continue;
		}
		//这里把每一个客户端的信息都存在一个结构里面，应该为每一个链接创建不同的链接来进行通信
		//比较好的办法是新建一个线程，专门接收该fd的数据，再单独处理。
		ret_val = recv(fd, (char*)Buffer, MaxBytes, 0);
		if (ret_val <= 0) {	  /*正常来说是返回0，不会返回小于0的数*/
			/*对端关闭了， 需要close本端socket。但是作为服务端，不需要重新初始化*/
	#if TCP_UNDER_VXWORKS > 0
			close(fd);
	#else
			closesocket(fd);
	#endif
			// for(i = 0 ; i < CLIENT_MAX_NUM; i++){
			// 	if(pTCP->Server_Socket[i] == fd)
			// 		pTCP->Server_Socket[i] = -1;
			// }
			FD_CLR(fd, pTCP->prfds);
			ret_val = -5;
			return ret_val;
		}else{
			/*可以直接接收数据，可以将发送到消息队列。这里只是给一个思路，简化处理，只接收单个socket的数据，所以直接break*/
			break;
		}
	}
	return ret_val;
}

/*.BH-------------------------------------------------------------
**
** 函数名: TCP_Client_Recv
**
** 描述: TCP客户端多路复用接收函数
**
** 输入参数:
**          Handler : void * const, TCP句柄
**          Buffer  : void * const, 存放接收数据的缓存指针
**          MaxBytes: int const, Buffer的最大容量
**
** 输出参数:
**          返回值: int, >= 1, 实际获取到的数据长度(单位:BYTE)
**                          0, 无数据可接收
**                         -1, 句柄无效
**                         -2, Buffer为空或MaxBytes长度不足
**                         -3, select出错
**						   -4, select为1，recv为0 表示对端关闭链接
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
int TCP_Client_Recv(void * const Handler, void * const Buffer, int const MaxBytes)
{
	TCP_INNER_TYPE * pTCP = (TCP_INNER_TYPE*)Handler;
	SockAddr_TYPE Source_Address;
	int Source_AddrSize = sizeof(SockAddr_TYPE);
	int ret_val;

	if (pTCP == NULL){
		ret_val = -1;
		return ret_val;
	}
	if ((Buffer == NULL) || (MaxBytes < 1)){
		ret_val = -2;
		return ret_val;
	}

	if((pTCP->FirstFlag == 1) && (pTCP != NULL)){
	    FD_ZERO(pTCP->prfds);
		FD_SET(pTCP->Recv_Socket, pTCP->prfds);
		pTCP->FirstFlag = 0;
	}

    //监听所有文件描述符(select的第一个参数取当前所有fd的最大值)
	//<0 错误  =0超时  =1获取到
    if (select(pTCP->Recv_Socket + 1, pTCP->prfds, NULL, NULL, NULL) < 0) {
#if TCP_UNDER_VXWORKS > 0
		if (EINTR == errnoGet())
			return 0;
#else
        if (WSAEINTR == WSAGetLastError())
			return 0;
#endif	
        fprintf(stderr, "%s(): select() error: %d", __FUNCTION__, errno);
        return -3;	//报错且不能再执行
    }

    if (FD_ISSET(pTCP->Recv_Socket, pTCP->prfds)) { /* client connected */
 		//XXX：这里把每一个客户端的信息都存在一个结构里面，应该为每一个链接创建不同的链接来进行通信
		ret_val = recv(pTCP->Recv_Socket, (char*)Buffer, MaxBytes, 0);
        if (ret_val <= 0) {
            /*对端关闭了， 需要close本端socket, 同时重新初始化客户端的socket*/
#if TCP_UNDER_VXWORKS > 0
            close(pTCP->Recv_Socket);
#else
			closesocket(pTCP->Recv_Socket);
#endif
			TCP_Init(pTCP, ntohl(pTCP->Peer_Recv_Address.sin_addr.s_addr), ntohs(pTCP->Peer_Recv_Address.sin_port),
							ntohl(pTCP->Self_Recv_Address.sin_addr.s_addr), ntohs(pTCP->Self_Recv_Address.sin_port),
							pTCP->CurCacheSize, TCP_RECV_NONBLK, 1, 0);

			FD_CLR(pTCP->Recv_Socket, pTCP->prfds);
			return -4;
        }
      
    }
 
}

/*.BH-------------------------------------------------------------
**
** 函数名: TCP_Init
**
** 描述: TCP客户端初始化接口函数
**
** 输入参数:
**          Handler        : TCP_STRUCT_TYPE * const, 需初始化的TCP句柄
**          Peer_IP        : unsigned int const, 对方设备IP地址
**          Peer_Recv_Port : unsigned short const, 对方设备接收TCP端口号
**          Self_IP        : unsigned int const, 本方设备IP地址(单网卡环境可置零,多网卡环境必须指定通信网卡地址)
**          Self_Recv_Port : unsigned short const, 本方设备接收TCP端口号
**          Recv_Cache     : int const, 接收缓存大小(系统默认为8192, 大数据量时可增加)
**          Recv_Block_Mode: int const, 接收模式: 0-阻塞, 1-非阻塞
**			Type		   : int const, TCP句柄类型: 0-服务端，   1-客户端
**			Mode           : int const, TCP接受模式: 0-点对点模式 1-C/S select模式
**			
**
** 输出参数:
**          返回值: int, >= 0, 初始化成功
**                         -1, 初始化失败(句柄无效)
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
int TCP_Init(TCP_STRUCT_TYPE * const Handler, unsigned int const Peer_IP, unsigned short const Peer_Recv_Port,
	unsigned int const Self_IP, unsigned short const Self_Recv_Port, int const Recv_Cache, int const Recv_Block_Mode, int const Type, int const Mode)
{
	static int First_Init = 1;
	TCP_INNER_TYPE *pTCP = (TCP_INNER_TYPE*)Handler;
	unsigned int Recv_Mode;
    int ret;
	int i;

    if (First_Init){
		#if TCP_UNDER_VXWORKS == 0
			ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
		#endif  
		First_Init = 0;
    }
	
	if (pTCP == NULL)
		return -1;

    pTCP->Type = Type;
	pTCP->Mode = Mode;

	pTCP->Self_Recv_Address.sin_family = AF_INET;
	pTCP->Self_Recv_Address.sin_addr.s_addr = htonl(Self_IP);
	pTCP->Self_Recv_Address.sin_port = htons(Self_Recv_Port);

	pTCP->Peer_Recv_Address.sin_family = AF_INET;
	pTCP->Peer_Recv_Address.sin_addr.s_addr = htonl(Peer_IP);
	pTCP->Peer_Recv_Address.sin_port = htons(Peer_Recv_Port);
	if (Type == 0){	/*服务端*/

		pTCP->Listen_Socket = socket(AF_INET, SOCK_STREAM, 0);
		if (bind(pTCP->Listen_Socket, (SOCKADDR*)&(pTCP->Self_Recv_Address), sizeof(SockAddr_TYPE)) < 0)
			return -1;
		/*(监听，最多监听MAX_LISTEN_NUM个客户端)*/
		if (listen(pTCP->Listen_Socket, 1) < 0)
		{
			printf("Sock %d listen failed!\n", pTCP->Listen_Socket);
			//TCP_Close(pTCP);
			return -1;
		}
		pTCP->CurCacheSize = Recv_Cache;
		if (Recv_Cache > 8192){
			ret = setsockopt(pTCP->Listen_Socket, SOL_SOCKET, SO_RCVBUF, (char*)&Recv_Cache, sizeof(int));
			/*发送缓冲，不能超过对端接受缓冲*/
			ret = setsockopt(pTCP->Listen_Socket, SOL_SOCKET, SO_SNDBUF, (char*)&Recv_Cache, sizeof(int));
		}

		Recv_Mode = (Recv_Block_Mode > 0);
	#if TCP_UNDER_VXWORKS > 0
		ioctl(pTCP->Listen_Socket, FIONBIO, &Recv_Mode);
	#else
		ioctlsocket(pTCP->Listen_Socket, FIONBIO, (u_long*)&Recv_Mode);
	#endif

		if(Mode == 0){
			pTCP->Send = TCP_Send;
			pTCP->Recv = TCP_Recv;
			pTCP->Accepted = 0;
			pTCP->Connected = 0;
		}else if(Mode == 1){
			pTCP->Send = TCP_BroadCast_Send;
			pTCP->Recv = TCP_Server_Recv;
			// for(i = 0 ; i < CLIENT_MAX_NUM; i++)
			// 	pTCP->Server_Socket[i] = -1;
			pTCP->prfds = (fd_set*)malloc(sizeof(fd_set));
			pTCP->FirstFlag == 1;
		}

	}
	else if (Type == 1){	/*客户端*/

		pTCP->Recv_Socket = socket(AF_INET, SOCK_STREAM, 0);
		pTCP->CurCacheSize = Recv_Cache;
		if (Recv_Cache > 8192){
			ret = setsockopt(pTCP->Recv_Socket, SOL_SOCKET, SO_RCVBUF, (char*)&Recv_Cache, sizeof(int));
			/*发送缓冲，不能超过对端接受缓冲*/
			ret = setsockopt(pTCP->Recv_Socket, SOL_SOCKET, SO_SNDBUF, (char*)&Recv_Cache, sizeof(int));
		}

		Recv_Mode = (Recv_Block_Mode > 0);
	#if TCP_UNDER_VXWORKS > 0
		ioctl(pTCP->Recv_Socket, FIONBIO, &Recv_Mode);
	#else
		ioctlsocket(pTCP->Recv_Socket, FIONBIO, (u_long*)&Recv_Mode);
	#endif

		if(Mode == 0){
			pTCP->Send = TCP_Send;
			pTCP->Recv = TCP_Recv;
			pTCP->Accepted = 0;
			pTCP->Connected = 0;
		}else if(Mode == 1){
			pTCP->Send = TCP_Send;
			pTCP->Recv = TCP_Client_Recv;
			pTCP->FirstFlag == 1;
		}
		
	}

	return 0;
	
}
/* END of TCP_Init */


#undef SockAddr_TYPE
#if TCP_UNDER_VXWORKS > 0
#undef SOCKADDR
#undef SOCKET
#else
#undef caddr_t
#endif
#undef TCP_UNDER_VXWORKS
