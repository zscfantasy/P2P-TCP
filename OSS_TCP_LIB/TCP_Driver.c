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
**	  仅支持点对点的TCP,服务端一个socket对应客户端一个socket
**
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

#ifndef TCP_UNDER_VXWORKS > 0
	#include <vxWorks.h>
	#include <ioLib.h>
	#include <sockLib.h>
	#include <inetLib.h>
	#include <stdio.h>
	#include <errno.h>
	#include <inetLib.h>
	#include <selectLib.h>
	#include <fcntl.h>
	#include <taskLib.h>
	#include <sysLib.h>
	#include <string.h>
	#include "TCP_Driver.h"
typedef unsigned int SOCKET;
#else /*Windows C VERSION*/
	#include <WinSock2.h>  /*<WinSock2.h>必须在window.h之前*/
    #include <stdio.h>
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

	SOCKET		  Recv_Socket;/*接收和发送的socket*/
	SockAddr_TYPE Self_Recv_Address;/*配置接收IP与端口号，用于创建Recv_Socket*/
	SockAddr_TYPE Peer_Recv_Address;/*配置目的IP与端口号，用于指定发送地址*/
	int Time_Out;
	int Mode;/*0服务端 1客户端*/
	SOCKET Accept_Socket;	/*作为服务端，一旦accept成功，得换成SOCKET Accept_Socket收发数据*/
	int Accepted;
	int Connected;
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

	close(pTCP->Recv_Socket);

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
**          返回值: int, >  0, 连接状态
**                         -1, 句柄无效
**                         -2, 非阻塞返回的正常值
**                         -3, 该SOCKET已连接
**						   -4，连接失败
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
/* END of TCP_Connect */
int TCP_Connect(void * const Handler)
{
	TCP_INNER_TYPE * pTCP = (TCP_INNER_TYPE*)Handler;
	int ret_val = 0;
	int connect_ret = 0;
	int err;

	if (pTCP == NULL)
		ret_val = -1;

	if (pTCP->Connected == 0){	/*如果是客户端*/
		connect_ret = connect(pTCP->Recv_Socket, (SOCKADDR*)&pTCP->Peer_Recv_Address, sizeof(SockAddr_TYPE));
		if (SOCKET_ERROR == connect_ret)
		{
			err = WSAGetLastError();
            /*非阻塞模式下的操作 -2是正常现象 -4是非正常情况，需要清socket*/
			if (err == WSAEWOULDBLOCK || err == WSAEINVAL){
#if TCP_UNDER_VXWORKS > 0
				taskDelay(sysClkRateGet() / 500);
#else
				Sleep(2);	/*值调整得太大容易导致任务周期不稳定*/
#endif
                pTCP->Connected = 0;
				ret_val = -2;
			}else if (err == WSAEISCONN){
				printf("TCP SOCK %d already connected when trying to connect server!\n", pTCP->Recv_Socket);
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
		else{
			pTCP->Connected = 1;
            printf("TCP SOCK %d connect server OK\n", pTCP->Recv_Socket);
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
**          返回值: int, >  0, 接收成功
**                         -1, 句柄无效
**                         -2, 非阻塞返回的正常值
**                         -3, 接收失败
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
/* END of TCP_Accept */
int TCP_Accept(void * const Handler)
{
	TCP_INNER_TYPE * pTCP = (TCP_INNER_TYPE*)Handler;
	int ret_val = 0;
	int err = 0;
	SOCKET accept_ret = 0;
	SockAddr_TYPE Source_Address;
	int Source_AddrSize = sizeof(SockAddr_TYPE);
	
	if (pTCP == NULL)
		ret_val = -1;

	if (pTCP->Accepted == 0){	/*如果是服务端*/
		accept_ret = accept(pTCP->Recv_Socket, (SOCKADDR*)&Source_Address, &Source_AddrSize);
		if (INVALID_SOCKET == accept_ret)
		{
			/*非阻塞模式下的操作 -2是正常现象 -3是非正常情况，需要清socket*/
			err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK)
			{
#if TCP_UNDER_VXWORKS > 0
				taskDelay(sysClkRateGet()/500);
#else
				Sleep(2);
#endif
				ret_val = -2;
			}
			else
			{
#if TCP_UNDER_VXWORKS > 0
				close(pTCP->Recv_Socket);
#else
				closesocket(pTCP->Recv_Socket);
				WSACleanup();
#endif

				ret_val = -3;
			}
			pTCP->Accepted = 0;
			return ret_val;
		}
		else{
			pTCP->Accepted = 1; 
			pTCP->Accept_Socket = accept_ret;
			printf("TCP SOCK %d got a connection from %s:%d\n", pTCP->Recv_Socket, inet_ntoa(Source_Address.sin_addr), 
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
**                         -3, Socket故障
**						   -4，Connect错误
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
/* END of TCP_Send */
int TCP_Send(void * const Handler, void * const Payload, int const LenBytes)
{
	TCP_INNER_TYPE * pTCP = (TCP_INNER_TYPE*)Handler;
	int ret_val = 0;
	int connect_ret = 0;

	if (pTCP == NULL)
		ret_val = -1;
	else if ((Payload == NULL) || (LenBytes < 1))
		ret_val = -2;
	else
	{
		/*服务端直接回复，客户端需要connect连接的前提下回复*/
		if (pTCP->Mode == 0){
			ret_val = send(pTCP->Accept_Socket, (caddr_t)Payload, LenBytes, 0);
		}
		else if (pTCP->Mode == 1){
			ret_val = TCP_Connect(pTCP);
			if (ret_val < 0){
				//printf("connection failed! connect_ret = &d\n", connect_ret);
				return -4;
			}
			ret_val = send(pTCP->Recv_Socket,   (caddr_t)Payload, LenBytes, 0);
		}
		else{
			ret_val = -5;	/*mode invalid, do nothing*/
		}
		
#if TCP_UNDER_VXWORKS > 0
		if (ret_val == ERROR){
			printf("TCP_Send Error %s\n", strerror(errno));
			ret_val = -3;
		}
#else
		if (ret_val == SOCKET_ERROR)
		{
			//printf("TCP_Send errno = %d\n", WSAGetLastError());
			ret_val = -3;
		}
#endif

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
**                         -3, Socket故障
**						   -4, Accept接收错误
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
int TCP_Recv(void * const Handler, void * const Buffer, int const MaxBytes)
{
	TCP_INNER_TYPE * pTCP = (TCP_INNER_TYPE*)Handler;
	SockAddr_TYPE Source_Address;
	int Source_AddrSize = sizeof(SockAddr_TYPE);
	int ret_val = 0;
	int Accept_Socket = 0;

	if (pTCP == NULL)
		ret_val = -1;
	else if ((Buffer == NULL) || (MaxBytes < 1))
		ret_val = -2;
	else
	{
		if (pTCP->Mode == 0){
			ret_val = TCP_Accept(pTCP);
			if (ret_val < 0){
				//printf("ACCEPT failed! accept_ret = &d\n", accept_ret);
				return -4;
			}
			ret_val = recv(pTCP->Accept_Socket, (char*)Buffer, MaxBytes, 0);
		}
		else if (pTCP->Mode == 1){
			ret_val = recv(pTCP->Recv_Socket, (char*)Buffer, MaxBytes, 0);
		}
		else{
			ret_val = -5;	/*mode invalid, do nothing*/
		}

		if (ret_val <= 0){
			//printf("[TCP_Recv]Socket%d, err %d\n", pTCP->Accept_Socket, WSAGetLastError());
			ret_val = -3;
		}
	}

	return ret_val;
}
/* END of TCP_Recv */

/*.BH-------------------------------------------------------------
**
** 函数名: TCP_Init
**
** 描述: TCP客户端初始化接口函数
**
** 输入参数:
**          Handler        : TCP_STRUCT_TYPE * const, 需初始化的TCP句柄
**          Peer_IP        : unsigned int const, 对方设备IP地址
**          Peer_Send_Port : unsigned short const, 对方设备发送TCP端口号
**          Peer_Recv_Port : unsigned short const, 对方设备接收TCP端口号
**          Self_IP        : unsigned int const, 本方设备IP地址(单网卡环境可置零,多网卡环境必须指定通信网卡地址)
**          Self_Send_Port : unsigned short const, 本方设备发送TCP端口号
**          Self_Recv_Port : unsigned short const, 本方设备接收TCP端口号
**          Recv_Cache     : int const, 接收缓存大小(系统默认为8192, 大数据量时可增加)
**          Recv_Block_Mode: int const, 接收模式: 0-阻塞, 1-非阻塞
**			Mode		   : int const, TCP模式: 0-服务端， 1-客户端
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
	unsigned int const Self_IP, unsigned short const Self_Recv_Port, int const Recv_Cache, int const Recv_Block_Mode, int const Mode)
{
	TCP_INNER_TYPE *pTCP = (TCP_INNER_TYPE*)Handler;
	unsigned int Recv_Mode;
	int Sock_AddrSize = sizeof(SockAddr_TYPE);
    int ret;

#if TCP_UNDER_VXWORKS == 0
	static int First_Init = 1;
    if (First_Init){
        ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
	First_Init = 0;
#endif

	if (pTCP == NULL)
		return -1;

	pTCP->Self_Recv_Address.sin_family = AF_INET;
	pTCP->Self_Recv_Address.sin_addr.s_addr = htonl(Self_IP);
	pTCP->Self_Recv_Address.sin_port = htons(Self_Recv_Port);
	pTCP->Recv_Socket = socket(AF_INET, SOCK_STREAM, 0);
    pTCP->Mode = Mode;
	if (Mode == 0){	/*服务端*/
		if (bind(pTCP->Recv_Socket, (SOCKADDR*)&(pTCP->Self_Recv_Address), sizeof(SockAddr_TYPE)) < 0)
			return -1;
		/*(监听，最多监听MAX_LISTEN_NUM个客户端)*/
		if (listen(pTCP->Recv_Socket, 1) < 0)
		{
			printf("Sock %d listen failed!\n", pTCP->Recv_Socket);
			//TCP_Close(pTCP);
			return -1;
		}
	}

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

	pTCP->Peer_Recv_Address.sin_family = AF_INET;
	pTCP->Peer_Recv_Address.sin_addr.s_addr = htonl(Peer_IP);
	pTCP->Peer_Recv_Address.sin_port = htons(Peer_Recv_Port);

	pTCP->Send = TCP_Send;
	pTCP->Recv = TCP_Recv;

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
