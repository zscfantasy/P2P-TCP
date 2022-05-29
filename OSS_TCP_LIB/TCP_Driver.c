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
**    Ĭ�ϲ���TCP�������ķ�ʽ����
**    ֻ��Ƕ��ʽϵͳvxworks��windows�¶��豸�Ķ�·���õļ�ʹ�ã��������ڸ߲���������linux��selectĬ��1024��fd��
**    0ģʽ(Mode)��֧�ֵ�Ե㡣����ϸİ�PPPЭ�飨֧�����16MBһ�����ݣ�ʹ�ñ���ճ��(����ܳ���)��1ģʽ֧�ַ���˿ɶ�Ӧ����ͻ��ˡ������������Ҫ����Ӧ�ò������PPPЭ�鲻�����á�
**	  0ģʽ(Mode)��Ե�ķ�ʽ������������շ���accept���ͻ����������շ���connect��֧�ֿͻ��˷���˵�����һ���ػ����������������ӡ�
**    �����û�ֻ��Ҫsend����recv��(����Ϊ0Ҳ���ԣ���һ��Ҫ����send����recv,�����ֶ�accept����connect��)
**	  1ģʽ(Mode)Select��ʽ�ݲ����ƣ���Ϊ��Ҫ���߳̽������ݣ�ĿǰֻҲֻ֧�ֵ�Ե㡣��Ҫ������������ڵ���Recv�������޷�select��ִ��accept��
**    1ģʽ(Mode)Select��ʽ��Ҫ�ͻ����������ڵ���Send�������޷�select��ִ��connect��
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
	#include <WinSock2.h>  /*<WinSock2.h>������window.h֮ǰ*/
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

	SOCKET		  Recv_Socket;  /*���պͷ��͵�socket*/
	SOCKET		  Listen_Socket;/*�����ר�õļ�����socket*/
	//SOCKET		  Server_Socket[CLIENT_MAX_NUM];	/*һ���������ݶ�������64���ͻ���*/
	SockAddr_TYPE Self_Recv_Address;/*���ý���IP��˿ںţ����ڴ���Socket*/
	SockAddr_TYPE Peer_Recv_Address;/*����Ŀ��IP��˿ںţ�����ָ�����͵�ַ*/
	int Max_Fd;
	int Time_Out;
	int Type;				/*0����� 1�ͻ���*/
	int Mode;				/*0-��Ե�ģʽ 1-C/S selectģʽ*/
	int Accepted;			/*0:δaccept  1:��accept   TypeΪ0,ModeΪ0��Ч*/
	int Connected;			/*0:δconnect 1:��connect  TypeΪ1,ModeΪ0��Ч*/
    int CurCacheSize;
	int FirstFlag;
	fd_set* prfds;
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
** ������: TCP_Connect
**
** ����: TCP���Ӻ������ͻ�������������������
**
** �������:
**          Handler : void * const, TCP���
**
** �������:
**          ����ֵ: int, >  0,   Connect�ɹ�
**                         -1, �����Ч
**                         -2, ���������ص�����ֵ
**                         -3, ��SOCKET������
**						   -4��  Connectʧ��
**
** ���ע��:
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

	if (pTCP->Connected == 0){	/*����ǿͻ���*/
		connect_ret = connect(pTCP->Recv_Socket, (SOCKADDR*)&pTCP->Peer_Recv_Address, sizeof(SockAddr_TYPE));
#if TCP_UNDER_VXWORKS > 0
		if (ERROR == connect_ret)
		{
			err = errnoGet();
			/*������ģʽ�µĲ��� -2���������� -4�Ƿ������������Ҫ��socket*/
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
            /*������ģʽ�µĲ��� -2���������� -4�Ƿ������������Ҫ��socket*/
			if (err == WSAEWOULDBLOCK || err == WSAEINVAL){
				Sleep(2);	/*ֵ������̫�����׵����������ڲ��ȶ�*/
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
** ������: TCP_Accept
**
** ����: TCP Accept����
**
** �������:
**          Handler : void * const, TCP���
**
** �������:
**          ����ֵ: int, >    0, Accept�ɹ�
**                         -1, �����Ч
**                         -2, ���������ص�����ֵ
**                         -3, Acceptʧ��
**
** ���ע��:
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

	if (pTCP->Accepted == 0){	/*����Ƿ�����ҵ�ǰδaccept*/
		accept_ret = accept(pTCP->Listen_Socket, (SOCKADDR*)&Source_Address, &Source_AddrSize);
#if TCP_UNDER_VXWORKS > 0
		if (ERROR == accept_ret)
		{
			/*������ģʽ�µĲ��� -2���������� -3�Ƿ������������Ҫ��socket*/
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
			/*������ģʽ�µĲ��� -2���������� -3�Ƿ������������Ҫ��socket*/
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
**                         -3, Connect����
**                         -4, mode��������
**						   -5, ���ͷ��ش���
**						   
**
** ���ע��:�����ڵ�Ե�ͨ��
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
		/*�Զ�����Ͽ����ӣ�WSAGetLastError���ܵ�ֵ��10054L*/
		/*�Զ˰���Ҫ��shutdown����close�ķ�ʽ���ӣ�WSAGetLastError���ܵ�ֵ��0*/
		if (pTCP->Type == 0){
			pTCP->Accepted = 0;    /*��Ϊ�Ѿ��Ͽ����ӣ�����accept��������ر�socket*/
			ret_val - -5;
		}
		if (pTCP->Type == 1){
			pTCP->Connected = 0;   /*���ڿͻ��˶��ԣ�����Ѿ��Ͽ������֣���Ҫ����connect�������³�ʼ��socket�����������Ϸ���ˡ�����˲���Ҫ*/
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
**                         -3, Accept����
**                         -4, mode��������
**						   -5, ���շ��ش���
**
** ���ע��: �����ڵ�Ե�ͨ��
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
		/*�Զ�����Ͽ����ӣ�WSAGetLastError���ܵ�ֵ��10054L*/
		/*�Զ˰���Ҫ��shutdown����close�ķ�ʽ���ӣ�WSAGetLastError���ܵ�ֵ��0*/
		if (pTCP->Type == 0){
			pTCP->Accepted = 0;    /*��Ϊ�Ѿ��Ͽ����ӣ�����accept��������ر�socket*/
			ret_val = -5;
		}
		if (pTCP->Type == 1){
			pTCP->Connected = 0;   /*���ڿͻ��˶��ԣ�����Ѿ��Ͽ������֣���Ҫ����connect�������³�ʼ��socket�����������Ϸ���ˡ�����˲���Ҫ*/
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
** ������: TCP_BroadCast_Send
**
** ����: TCP����˹㲥���ͺ���(����ǰ�������ӵĿͻ��˷�����ͬ������)
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
**                         -3, ���ʹ���
**                         -4, ����ʧ��
**						   
**
** ���ע��:
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
	
	//���������ļ���������ȷ����ǰ�������ӵĿͻ���
    for (fd = 0; fd <= pTCP->Max_Fd; fd++) 
	{
		if (fd == pTCP->Listen_Socket || !FD_ISSET(fd, pTCP->prfds)) {
			continue;
		}
		/*send���һ��������
		windows��һ������Ϊ0
		linux���������ΪMSG_NOSIGNAL,���������,�ڷ��ͳ�����п��ܻᵼ�³����˳�*/
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
			/*�Զ˹ر��ˣ� ��Ҫclose����socket��������Ϊ����ˣ�����Ҫ���³�ʼ��*/
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
** ������: TCP_Server_Recv
**
** ����: TCP����˶�·���ý��պ�����Selectͬʱ���accept��recv��fd��
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
**                         -3, select����
**						   -4, selectΪ1��recvΪ0 ��ʾ�Զ˹ر�����
**
** ���ע��:
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

    //���������ļ�������(select�ĵ�һ������ȡ��ǰ����fd�����ֵ)
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
        return -3;	//�����Ҳ�����ִ��
    }
	//�����Ƿ�Ҫ����select����ֵΪ0����ʱ���������
    if (FD_ISSET(pTCP->Listen_Socket, pTCP->prfds)) { /* client connected */

        fd = accept(pTCP->Listen_Socket, (SOCKADDR *)&Source_Address, &Source_AddrSize);
        if (fd != -1) {
            //�ѽ������ӵĿͻ����ļ���������ӵ���������
            FD_SET(fd, pTCP->prfds);
            pTCP->Max_Fd = MY_MAX(pTCP->Max_Fd, fd);
			printf("\nTCP SOCK %d got a connection from %s:%d\n\n", fd, inet_ntoa(Source_Address.sin_addr), 
									ntohs(Source_Address.sin_port));
			//����pTCP�����ά������
			// for(i = 0 ; i < CLIENT_MAX_NUM; i++){
			// 	if(pTCP->Server_Socket[i] == -1)
			// 		pTCP->Server_Socket[i] = fd;
			// }
        }
    }
    //���������ļ������������տͻ��˵�����
    for(fd = 0; fd < pTCP->Max_Fd; ++fd){
		if (fd == pTCP->Listen_Socket || !FD_ISSET(fd, pTCP->prfds)) {
			continue;
		}
		//�����ÿһ���ͻ��˵���Ϣ������һ���ṹ���棬Ӧ��Ϊÿһ�����Ӵ�����ͬ������������ͨ��
		//�ȽϺõİ취���½�һ���̣߳�ר�Ž��ո�fd�����ݣ��ٵ�������
		ret_val = recv(fd, (char*)Buffer, MaxBytes, 0);
		if (ret_val <= 0) {	  /*������˵�Ƿ���0�����᷵��С��0����*/
			/*�Զ˹ر��ˣ� ��Ҫclose����socket��������Ϊ����ˣ�����Ҫ���³�ʼ��*/
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
			/*����ֱ�ӽ������ݣ����Խ����͵���Ϣ���С�����ֻ�Ǹ�һ��˼·���򻯴���ֻ���յ���socket�����ݣ�����ֱ��break*/
			break;
		}
	}
	return ret_val;
}

/*.BH-------------------------------------------------------------
**
** ������: TCP_Client_Recv
**
** ����: TCP�ͻ��˶�·���ý��պ���
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
**                         -3, select����
**						   -4, selectΪ1��recvΪ0 ��ʾ�Զ˹ر�����
**
** ���ע��:
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

    //���������ļ�������(select�ĵ�һ������ȡ��ǰ����fd�����ֵ)
	//<0 ����  =0��ʱ  =1��ȡ��
    if (select(pTCP->Recv_Socket + 1, pTCP->prfds, NULL, NULL, NULL) < 0) {
#if TCP_UNDER_VXWORKS > 0
		if (EINTR == errnoGet())
			return 0;
#else
        if (WSAEINTR == WSAGetLastError())
			return 0;
#endif	
        fprintf(stderr, "%s(): select() error: %d", __FUNCTION__, errno);
        return -3;	//�����Ҳ�����ִ��
    }

    if (FD_ISSET(pTCP->Recv_Socket, pTCP->prfds)) { /* client connected */
 		//XXX�������ÿһ���ͻ��˵���Ϣ������һ���ṹ���棬Ӧ��Ϊÿһ�����Ӵ�����ͬ������������ͨ��
		ret_val = recv(pTCP->Recv_Socket, (char*)Buffer, MaxBytes, 0);
        if (ret_val <= 0) {
            /*�Զ˹ر��ˣ� ��Ҫclose����socket, ͬʱ���³�ʼ���ͻ��˵�socket*/
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
** ������: TCP_Init
**
** ����: TCP�ͻ��˳�ʼ���ӿں���
**
** �������:
**          Handler        : TCP_STRUCT_TYPE * const, ���ʼ����TCP���
**          Peer_IP        : unsigned int const, �Է��豸IP��ַ
**          Peer_Recv_Port : unsigned short const, �Է��豸����TCP�˿ں�
**          Self_IP        : unsigned int const, �����豸IP��ַ(����������������,��������������ָ��ͨ��������ַ)
**          Self_Recv_Port : unsigned short const, �����豸����TCP�˿ں�
**          Recv_Cache     : int const, ���ջ����С(ϵͳĬ��Ϊ8192, ��������ʱ������)
**          Recv_Block_Mode: int const, ����ģʽ: 0-����, 1-������
**			Type		   : int const, TCP�������: 0-����ˣ�   1-�ͻ���
**			Mode           : int const, TCP����ģʽ: 0-��Ե�ģʽ 1-C/S selectģʽ
**			
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
	if (Type == 0){	/*�����*/

		pTCP->Listen_Socket = socket(AF_INET, SOCK_STREAM, 0);
		if (bind(pTCP->Listen_Socket, (SOCKADDR*)&(pTCP->Self_Recv_Address), sizeof(SockAddr_TYPE)) < 0)
			return -1;
		/*(������������MAX_LISTEN_NUM���ͻ���)*/
		if (listen(pTCP->Listen_Socket, 1) < 0)
		{
			printf("Sock %d listen failed!\n", pTCP->Listen_Socket);
			//TCP_Close(pTCP);
			return -1;
		}
		pTCP->CurCacheSize = Recv_Cache;
		if (Recv_Cache > 8192){
			ret = setsockopt(pTCP->Listen_Socket, SOL_SOCKET, SO_RCVBUF, (char*)&Recv_Cache, sizeof(int));
			/*���ͻ��壬���ܳ����Զ˽��ܻ���*/
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
	else if (Type == 1){	/*�ͻ���*/

		pTCP->Recv_Socket = socket(AF_INET, SOCK_STREAM, 0);
		pTCP->CurCacheSize = Recv_Cache;
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
