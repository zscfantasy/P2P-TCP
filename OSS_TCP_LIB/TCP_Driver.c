/*-----------------------------------------------------------
**
**
** �ļ���:  TCP_Driver.c
**
** ����:    ������ͨ��TCP����ͨ����������к����ͱ���
**
** ����ĺ���:
**    TCP_Init()
**    TCP_Send()
**    TCP_Recv()
**
** ���ע��:
**	  ��֧�ֵ�Ե��TCP,�����һ��socket��Ӧ�ͻ���һ��socket
**
**
** ������ʷ:
**    2022��4��3��  �������ļ�
**    $Revision$
**    $Date$
**    $Author$
**    $Log$
**
**-----------------------------------------------------------
*/
/*TCP���л���*/
#ifdef _WIN32
#define TCP_UNDER_VXWORKS   0
#else
#define TCP_UNDER_VXWORKS   2 /*Windows C++���л����¶���Ϊ0, vxWorks5.x�����¶���Ϊ1, vxWorks6.x�����¶���Ϊ2*/
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
	#include <WinSock2.h>  /*<WinSock2.h>������window.h֮ǰ*/
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

	SOCKET		  Recv_Socket;/*���պͷ��͵�socket*/
	SockAddr_TYPE Self_Recv_Address;/*���ý���IP��˿ںţ����ڴ���Recv_Socket*/
	SockAddr_TYPE Peer_Recv_Address;/*����Ŀ��IP��˿ںţ�����ָ�����͵�ַ*/
	int Time_Out;
	int Mode;/*0����� 1�ͻ���*/
	SOCKET Accept_Socket;	/*��Ϊ����ˣ�һ��accept�ɹ����û���SOCKET Accept_Socket�շ�����*/
	int Accepted;
	int Connected;
} TCP_INNER_TYPE;

/*.BH-------------------------------------------------------------
**
** ������: TCP_Close
**
** ����: �ر�TCP
**
** �������:
**          tcp_id  : TCP��SOCKET��
**
** �������:
**          ����ֵ:         0, �ɹ�
**                         -1, TCP_ID�Ƿ�
**
** ���ע��:
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
** ������: TCP_Connect
**
** ����: TCP���Ӻ������ͻ�������������������
**
** �������:
**          Handler : void * const, TCP���
**
** �������:
**          ����ֵ: int, >  0, ����״̬
**                         -1, �����Ч
**                         -2, ���������ص�����ֵ
**                         -3, ��SOCKET������
**						   -4������ʧ��
**
** ���ע��:
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

	if (pTCP->Connected == 0){	/*����ǿͻ���*/
		connect_ret = connect(pTCP->Recv_Socket, (SOCKADDR*)&pTCP->Peer_Recv_Address, sizeof(SockAddr_TYPE));
		if (SOCKET_ERROR == connect_ret)
		{
			err = WSAGetLastError();
            /*������ģʽ�µĲ��� -2���������� -4�Ƿ������������Ҫ��socket*/
			if (err == WSAEWOULDBLOCK || err == WSAEINVAL){
#if TCP_UNDER_VXWORKS > 0
				taskDelay(sysClkRateGet() / 500);
#else
				Sleep(2);	/*ֵ������̫�����׵����������ڲ��ȶ�*/
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
** ������: TCP_Accept
**
** ����: TCP Accept����
**
** �������:
**          Handler : void * const, TCP���
**
** �������:
**          ����ֵ: int, >  0, ���ճɹ�
**                         -1, �����Ч
**                         -2, ���������ص�����ֵ
**                         -3, ����ʧ��
**
** ���ע��:
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

	if (pTCP->Accepted == 0){	/*����Ƿ����*/
		accept_ret = accept(pTCP->Recv_Socket, (SOCKADDR*)&Source_Address, &Source_AddrSize);
		if (INVALID_SOCKET == accept_ret)
		{
			/*������ģʽ�µĲ��� -2���������� -3�Ƿ������������Ҫ��socket*/
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
** ������: TCP_Send
**
** ����: TCP���ͺ���
**
** �������:
**          Handler : void * const, TCP���
**          Payload : void * const, ��������ָ��
**          LenBytes: int const, �������ݳ���
**
** �������:
**          ����ֵ: int, >  0, ���ͳɹ�(���ط������ݳ���, ��λ:BYTE)
**                         -1, �����Ч
**                         -2, PayloadΪ�ջ�LenBytesС��1
**                         -3, Socket����
**						   -4��Connect����
**
** ���ע��:
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
		/*�����ֱ�ӻظ����ͻ�����Ҫconnect���ӵ�ǰ���»ظ�*/
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
** ������: TCP_Recv
**
** ����: TCP���պ���
**
** �������:
**          Handler : void * const, TCP���
**          Buffer  : void * const, ��Ž������ݵĻ���ָ��
**          MaxBytes: int const, Buffer���������
**
** �������:
**          ����ֵ: int, >= 1, ʵ�ʻ�ȡ�������ݳ���(��λ:BYTE)
**                          0, �����ݿɽ���
**                         -1, �����Ч
**                         -2, BufferΪ�ջ�MaxBytes���Ȳ���
**                         -3, Socket����
**						   -4, Accept���մ���
**
** ���ע��:
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
** ������: TCP_Init
**
** ����: TCP�ͻ��˳�ʼ���ӿں���
**
** �������:
**          Handler        : TCP_STRUCT_TYPE * const, ���ʼ����TCP���
**          Peer_IP        : unsigned int const, �Է��豸IP��ַ
**          Peer_Send_Port : unsigned short const, �Է��豸����TCP�˿ں�
**          Peer_Recv_Port : unsigned short const, �Է��豸����TCP�˿ں�
**          Self_IP        : unsigned int const, �����豸IP��ַ(����������������,��������������ָ��ͨ��������ַ)
**          Self_Send_Port : unsigned short const, �����豸����TCP�˿ں�
**          Self_Recv_Port : unsigned short const, �����豸����TCP�˿ں�
**          Recv_Cache     : int const, ���ջ����С(ϵͳĬ��Ϊ8192, ��������ʱ������)
**          Recv_Block_Mode: int const, ����ģʽ: 0-����, 1-������
**			Mode		   : int const, TCPģʽ: 0-����ˣ� 1-�ͻ���
**
** �������:
**          ����ֵ: int, >= 0, ��ʼ���ɹ�
**                         -1, ��ʼ��ʧ��(�����Ч)
**
** ���ע��:
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
	if (Mode == 0){	/*�����*/
		if (bind(pTCP->Recv_Socket, (SOCKADDR*)&(pTCP->Self_Recv_Address), sizeof(SockAddr_TYPE)) < 0)
			return -1;
		/*(������������MAX_LISTEN_NUM���ͻ���)*/
		if (listen(pTCP->Recv_Socket, 1) < 0)
		{
			printf("Sock %d listen failed!\n", pTCP->Recv_Socket);
			//TCP_Close(pTCP);
			return -1;
		}
	}

	if (Recv_Cache > 8192){
        ret = setsockopt(pTCP->Recv_Socket, SOL_SOCKET, SO_RCVBUF, (char*)&Recv_Cache, sizeof(int));
		/*���ͻ��壬���ܳ����Զ˽��ܻ���*/
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
