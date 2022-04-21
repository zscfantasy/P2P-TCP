/*-----------------------------------------------------------
**
**
** �ļ���:  PPP_Protocol.c
**
** ����:    �����Ե�ͨ��Э��PPP(Point to Point Protocol)�����к�����ȫ�ֱ���
**
** ����ĺ���:
**    PPP_Protocol_Init
**    PPP_Send
**    PPP_Recv
**
** ���ע��:
**			һ��send����ܳ���16777216�ֽڣ�16MB��
**
**
** ������ʷ:
**    2018��9��17��  �������ļ�
**	  2020��9��17��  ���PCģ��FC��ģ�⣨TCP+PPP����֧�����һ֡16MB������PPP֡��ʽ�����ȸĳ�4�ֽڣ�ȥ��У�����Ч�ʣ�by zsc��
**    $Revision$
**    $Date$
**    $Author$
**    $Log$
**
**    PPP֡��ʽ:  0xA5, CheckSum, ���ȵ�8λ, �����м�8λ, �����м�8λ, ���ȸ�8λ, Datagram[...Len...], 0x5A
**                0     1         2          3            4            5          Len+5				   Len+6
**                Frame[...                     Len+7                         ...]
**              ���� = Len - 1, CheckSum�Գ��Ⱥ����ݱ�Datagram����У��(2~Len+3)
**				4���ֽڱ�ʾ�ĳ���ֻ������λ�ĳ��ȣ�������֡ͷ֡β
**-----------------------------------------------------------
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PPP_Protocol.h"

typedef struct
{
	/*.BH-------------------------------------------------------------
	**
	** ������: Send
	**
	** ����: TSS���ͺ���
	**
	** �������:
	**          Handler : void * const, TSS���
	**          Payload : void * const, ��������ָ��
	**          LenBytes: int const, �������ݳ���
	**
	** �������:
	**          ����ֵ: int, >  0, ���ͳɹ�(���ط������ݳ���, ��λ:BYTE)
	**                         -1, �����Ч
	**                         -2, PayloadΪ�ջ�LenBytesС��1
	**                      <= -3, �����쳣
	**
	** ���ע��:
	**
	**.EH-------------------------------------------------------------
	*/
	int(*Send)(void * const Handler, void * const Payload, int const LenBytes);

	/*.BH-------------------------------------------------------------
	**
	** ������: Recv
	**
	** ����: TSS���պ���
	**
	** �������:
	**          Handler : void * const, TSS���
	**          Buffer  : void * const, ��Ž������ݵĻ���ָ��
	**          MaxBytes: int const, Buffer���������
	**
	** �������:
	**          ����ֵ: int, >= 1, ʵ�ʻ�ȡ�������ݳ���(��λ:BYTE)
	**                          0, �����ݿɽ���
	**                         -1, �����Ч
	**                         -2, BufferΪ�ջ�MaxBytes���Ȳ���
	**                       <=-3, �����쳣
	**
	** ���ע��:
	**
	**.EH-------------------------------------------------------------
	*/
	int(*Recv)(void * const Handler, void * const Buffer, int const MaxBytes);
} TSS_HANDLER_TYPE;

typedef struct
{
	int (*Send)(void * const Handler, void * const Payload, int const LenBytes);
	int (*Recv)(void * const Handler, void * const Buffer,  int const MaxBytes);

	int ARP_Call_Flag; /*ARPЭ����ñ�־, 0-��ARP���� ����-ARP����*/
	/*ͨ���ֽ������ջ���Recv_Buffer[RecvBuf_LenBytes], pR_RecvBuf��pW_RecvBuf�ֱ�Ϊ��дָ��*/
	unsigned char *Recv_Buffer;
	int RecvBuf_LenBytes, pR_RecvBuf, pW_RecvBuf, Refresh_RecvBuf;
	/*ͨ������֡����Frame[Frame_MaxBytes]*/
	unsigned char *Frame;
	int Datagram_MaxBytes; /*������ݱ�����*/

	TSS_HANDLER_TYPE *SubIO;
} PPP_INNER_TYPE;


/*.BH-------------------------------------------------------------
**
** ������: PPP_Send
**
** ����: PPPЭ�鷢�ͺ���
**
** �������:
**          Handler : void * const, PPP���
**          Datagram: void * const, ���������ݱ�ָ��
**          LenBytes: int const, ���ݱ�����
**
** �������:
**          ����ֵ: int, >  0, ���ͳɹ�(���ط������ݱ�����, ��λ:BYTE)
**                         -1, �����Ч
**                         -2, DatagramΪ�ջ�LenBytesС��1
**
** ���ע��:
**
**.EH-------------------------------------------------------------
*/
int PPP_Send(void * const Handler, void * const Datagram, int const LenBytes)
{
	PPP_INNER_TYPE * pPPP = (PPP_INNER_TYPE*)Handler;
	unsigned char CheckSum, *Frame;
	int i;

	if (pPPP == NULL)
		return -1;
	if ((Datagram == NULL) || (LenBytes < 1) || (LenBytes > pPPP->Datagram_MaxBytes))
		return -2;

	if (pPPP->ARP_Call_Flag)
		Frame = (unsigned char*)Datagram - 4;
	else
	{
		Frame = pPPP->Frame;
		memcpy(&Frame[6], Datagram, LenBytes);
	}
	Frame[0] = 0xA5;
	Frame[2] = (unsigned char)(LenBytes - 1);
	Frame[3] = (unsigned char)((LenBytes - 1) >> 8);
	Frame[4] = (unsigned char)((LenBytes - 1) >> 16);
	Frame[5] = (unsigned char)((LenBytes - 1) >> 24);
	/*
	CheckSum = 0xFF ^ Frame[2] ^ Frame[3];
	for (i = 4; i < (LenBytes + 4); i++)
		CheckSum ^= Frame[i];
	Frame[1] = CheckSum;
	*/
	Frame[LenBytes + 6] = 0x5A;
	return pPPP->SubIO->Send(pPPP->SubIO, Frame, LenBytes + 7) - 7;
}
/* END of PPP_Send */


/*.BH-------------------------------------------------------------
**
** ������: PPP_Recv
**
** ����: PPPЭ����պ���
**
** �������:
**          Handler : void * const, PPP���
**          Buffer  : void * const, ������ݱ��Ļ���ָ��
**          MaxBytes: int const, Buffer�ĳ���
**
** �������:
**          ����ֵ: int, >= 1, ʵ�ʻ�ȡ�������ݱ�����(��λ:BYTE)
**                          0, �����ݱ��ɽ���
**                         -1, �����Ч
**                         -2, BufferΪ�ջ�MaxBytes���Ȳ���
**
** ���ע��:
**
**.EH-------------------------------------------------------------
*/
int PPP_Recv(void * const Handler, void * const Buffer, int const MaxBytes)
{
	PPP_INNER_TYPE *pPPP = (PPP_INNER_TYPE*)Handler;
	unsigned char *StreamBuf, *pDatagram, CheckSum;
	int pW_RecvBuf, pR_RecvBuf, ret_val, Available, Length, i;

	if (pPPP == NULL)
		return -1;
	if ((Buffer == NULL) || (MaxBytes < 1))
		return -2;

	StreamBuf = pPPP->Recv_Buffer;
	pW_RecvBuf = pPPP->pW_RecvBuf, pR_RecvBuf = pPPP->pR_RecvBuf;
	if (pPPP->Refresh_RecvBuf)
	{
		while ((Length = pPPP->SubIO->Recv(pPPP->SubIO, &StreamBuf[pW_RecvBuf], pPPP->RecvBuf_LenBytes - pW_RecvBuf)) > 0)
			pW_RecvBuf += Length;
		if (pW_RecvBuf != pPPP->pW_RecvBuf)
		{
			pPPP->pW_RecvBuf = pW_RecvBuf;
			pPPP->Refresh_RecvBuf = 0;
		}
	}

	ret_val = 0;
	while (pR_RecvBuf != pW_RecvBuf)
	{
		if (StreamBuf[pR_RecvBuf] == 0xA5)
		{
			Available = pW_RecvBuf - pR_RecvBuf;
			if (Available > 7)
			{
				Length = StreamBuf[pR_RecvBuf + 2] + (StreamBuf[pR_RecvBuf + 3] << 8) 
					  + (StreamBuf[pR_RecvBuf + 4] << 16) + (StreamBuf[pR_RecvBuf + 5] << 24) + 1 + 7; /*+1��ԭ���ݱ����� +7����֡����*/
				if (   (Length > (MaxBytes + 7)) /*MaxBytes�����ϲ�ӦС��������ݱ�����(֡����-֡ͷ-У���-4�ֽڳ���-֡β)*/
					|| (Length > (pPPP->RecvBuf_LenBytes >> 1)) ) /*Ϊ����Available��ԶС��Length, Ҫ�󻺴泤�����������֡���ȵ�����*/
					ret_val = -2; /*�����쳣(����֡���ȴ����г��ֲ���Ŵ�), ����ƥ��֡��־�ֶ�*/
				else if (Available >= Length)
				{
					if (StreamBuf[pR_RecvBuf + Length - 1] == 0x5A)
					{
						/*
						CheckSum = 0xFF ^ StreamBuf[pR_RecvBuf + 1] ^ StreamBuf[pR_RecvBuf + 2] ^ StreamBuf[pR_RecvBuf + 3];
						Length -= 7;
						pDatagram = &StreamBuf[pR_RecvBuf + 6];
						for (i = 0; i < Length; i++)
							CheckSum ^= pDatagram[i];
						if (CheckSum == 0)
						{
							memcpy(Buffer, pDatagram, Length);
							ret_val = Length;
							pR_RecvBuf += Length + 7;
						}
						else
							ret_val = -2; /*У��ʧ��, ����ƥ��֡��־�ֶ�*/

						Length -= 7;
						pDatagram = &StreamBuf[pR_RecvBuf + 6];
						memcpy(Buffer, pDatagram, Length);
						ret_val = Length;
						pR_RecvBuf += Length + 7;
					}
					else
						ret_val = -2; /*֡βƥ��ʧ��, ����ƥ��֡��־�ֶ�*/
				}
			}
			if (ret_val >= 0)
				break;
		}
		ret_val = 0;
		pR_RecvBuf++;
	}
	pPPP->pR_RecvBuf = pR_RecvBuf;

	if (ret_val <= 0)
	{
		if (pR_RecvBuf > 0)
		{
			pW_RecvBuf -= pR_RecvBuf;
			for (i = 0; i < pW_RecvBuf; i++) /*�����ڽ��յ�����ȫ֡,���Ƶ��������ײ����������ڴ���*/
				StreamBuf[i] = StreamBuf[pR_RecvBuf + i];
			pPPP->pW_RecvBuf = pW_RecvBuf;
			pPPP->pR_RecvBuf = 0;
		}
		pPPP->Refresh_RecvBuf = 1;
	}
	return ret_val;
}
/* END of PPP_Recv */


/*.BH-------------------------------------------------------------
**
** ������: PPP_Protocol_Init
**
** ����: PPP(Point to Point Protocol)Э���ʼ������
**
** �������:
**          Handler          : PPP_STRUCT_TYPE * const, ���ʼ����PPP���
**          ARP_Call_Flag    : int const, ARPЭ����ñ�־, 0-��ARP���� ����-ARP����
**          Datagram_MaxBytes: int const, ������ݱ�����(<=65536)
**          RecvBuf_LenBytes : int const, ���������泤��, �ɴ�������Э�鴦�������ۺϾ���(����С��������ݱ����ȵ�����)
**          SubIO            : void * const, �²�IO���
**
** �������:
**          ����ֵ: int, >= 0, ��ʼ���ɹ�
**                         -1, ��ʼ��ʧ��(�����Ч)
**                         -2, ��ʼ��ʧ��(��Ч����)
**                         -3, ���뻺��ռ�ʧ��
**
** ���ע��:
**
**.EH-------------------------------------------------------------
*/
int PPP_Protocol_Init(PPP_STRUCT_TYPE * const Handler, int const ARP_Call_Flag, int const Datagram_MaxBytes, int const RecvBuf_LenBytes, void * const SubIO)
{
	PPP_INNER_TYPE *pPPP = (PPP_INNER_TYPE*)Handler;

	if (pPPP == NULL)
		return -1;
	if ((Datagram_MaxBytes < 1) || (Datagram_MaxBytes > 16777216/*65536*/) || (RecvBuf_LenBytes < (Datagram_MaxBytes * 2)) || (SubIO == NULL))
		return -2;

	if (ARP_Call_Flag)
		pPPP->Frame = NULL;
	else
	{
		pPPP->Frame = (unsigned char*)malloc((Datagram_MaxBytes + 7 + 3) & 0xFFFFFFFC); /*+7:����֡; +3&0xFFFFFFFC:ȡ4��������*/
		if (pPPP->Frame == NULL)
			return -3;
	}
	pPPP->RecvBuf_LenBytes = (RecvBuf_LenBytes + 7 * 2 + 3) & 0xFFFFFFFC;
	pPPP->Recv_Buffer = (unsigned char*)malloc(pPPP->RecvBuf_LenBytes);
	if (pPPP->Recv_Buffer == NULL)
		return -2;
	pPPP->ARP_Call_Flag = ARP_Call_Flag;
	pPPP->Datagram_MaxBytes = Datagram_MaxBytes;
	pPPP->pW_RecvBuf = 0, pPPP->pR_RecvBuf = 0;
	pPPP->Refresh_RecvBuf = 1;
	pPPP->SubIO = (TSS_HANDLER_TYPE*)SubIO;
	pPPP->Send = PPP_Send;
	pPPP->Recv = PPP_Recv;
	return 0;
}
/* END of PPP_Protocol_Init */

#undef PPP_INNER_TYPE
