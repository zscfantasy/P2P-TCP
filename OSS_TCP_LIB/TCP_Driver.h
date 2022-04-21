/*-----------------------------------------------------------
**
** ��Ȩ:    �й��������ߵ�����о���, 2017��
**
** �ļ���:  TCP_Driver.h
**
** ����:    ������tcpͨ�������ŵ����нӿ�
**
** ���ע��:
**
** ����:
**     ��ʫ��, 2017��1�¿�ʼ��д���ļ�
**
** ������ʷ:
**    2017��1��3��  �������ļ�
**    $Revision$
**    $Date$
**    $Author$
**    $Log$
**
**-----------------------------------------------------------
*/
#ifndef __TCP_Driver__
#define __TCP_Driver__

/*---------------------------------
**         TCP�궨��
**---------------------------------
*/
/*UDP����ģʽ*/
#define TCP_RECV_BLOCK   0 /*����ʽ����*/
#define TCP_RECV_NONBLK  1 /*������ʽ����*/

#ifdef __cplusplus
extern "C" {
#endif
	/*---------------------------------
	**            ���Ͷ���
	**---------------------------------
	*/
	typedef struct
	{
		int(*Send)(void * const Handler, void * const Payload, int const LenBytes);
		int(*Recv)(void * const Handler, void * const Buffer, int const MaxBytes);

		char TCP_Data[72];
	} TCP_STRUCT_TYPE;

	int TCP_Init(TCP_STRUCT_TYPE * const Handler, unsigned int const Peer_IP, unsigned short const Peer_Recv_Port,
		unsigned int const Self_IP, unsigned short const Self_Recv_Port, int const Recv_Cache, int const Recv_Block_Mode, int const Mode);

	int TCP_Connect(void * const Handler);
	int TCP_Accept(void * const Handler);
#ifdef __cplusplus
}
#endif

#endif
