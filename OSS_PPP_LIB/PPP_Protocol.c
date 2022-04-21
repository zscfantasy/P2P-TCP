/*-----------------------------------------------------------
**
**
** 文件名:  PPP_Protocol.c
**
** 描述:    定义点对点通信协议PPP(Point to Point Protocol)的所有函数和全局变量
**
** 定义的函数:
**    PPP_Protocol_Init
**    PPP_Send
**    PPP_Recv
**
** 设计注记:
**			一次send最大不能超过16777216字节（16MB）
**
**
** 更改历史:
**    2018年9月17日  创建本文件
**	  2020年9月17日  针对PC模拟FC流模拟（TCP+PPP），支持最大一帧16MB，扩充PPP帧格式，长度改成4字节，去掉校验提高效率（by zsc）
**    $Revision$
**    $Date$
**    $Author$
**    $Log$
**
**    PPP帧格式:  0xA5, CheckSum, 长度低8位, 长度中间8位, 长度中间8位, 长度高8位, Datagram[...Len...], 0x5A
**                0     1         2          3            4            5          Len+5				   Len+6
**                Frame[...                     Len+7                         ...]
**              长度 = Len - 1, CheckSum对长度和数据报Datagram进行校验(2~Len+3)
**				4个字节表示的长度只是数据位的长度，不包括帧头帧尾
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
	** 函数名: Send
	**
	** 描述: TSS发送函数
	**
	** 输入参数:
	**          Handler : void * const, TSS句柄
	**          Payload : void * const, 发送数据指针
	**          LenBytes: int const, 发送数据长度
	**
	** 输出参数:
	**          返回值: int, >  0, 发送成功(返回发送数据长度, 单位:BYTE)
	**                         -1, 句柄无效
	**                         -2, Payload为空或LenBytes小于1
	**                      <= -3, 其他异常
	**
	** 设计注记:
	**
	**.EH-------------------------------------------------------------
	*/
	int(*Send)(void * const Handler, void * const Payload, int const LenBytes);

	/*.BH-------------------------------------------------------------
	**
	** 函数名: Recv
	**
	** 描述: TSS接收函数
	**
	** 输入参数:
	**          Handler : void * const, TSS句柄
	**          Buffer  : void * const, 存放接收数据的缓存指针
	**          MaxBytes: int const, Buffer的最大容量
	**
	** 输出参数:
	**          返回值: int, >= 1, 实际获取到的数据长度(单位:BYTE)
	**                          0, 无数据可接收
	**                         -1, 句柄无效
	**                         -2, Buffer为空或MaxBytes长度不足
	**                       <=-3, 其他异常
	**
	** 设计注记:
	**
	**.EH-------------------------------------------------------------
	*/
	int(*Recv)(void * const Handler, void * const Buffer, int const MaxBytes);
} TSS_HANDLER_TYPE;

typedef struct
{
	int (*Send)(void * const Handler, void * const Payload, int const LenBytes);
	int (*Recv)(void * const Handler, void * const Buffer,  int const MaxBytes);

	int ARP_Call_Flag; /*ARP协议调用标志, 0-非ARP调用 其余-ARP调用*/
	/*通道字节流接收缓存Recv_Buffer[RecvBuf_LenBytes], pR_RecvBuf和pW_RecvBuf分别为读写指针*/
	unsigned char *Recv_Buffer;
	int RecvBuf_LenBytes, pR_RecvBuf, pW_RecvBuf, Refresh_RecvBuf;
	/*通道发送帧缓存Frame[Frame_MaxBytes]*/
	unsigned char *Frame;
	int Datagram_MaxBytes; /*最大数据报长度*/

	TSS_HANDLER_TYPE *SubIO;
} PPP_INNER_TYPE;


/*.BH-------------------------------------------------------------
**
** 函数名: PPP_Send
**
** 描述: PPP协议发送函数
**
** 输入参数:
**          Handler : void * const, PPP句柄
**          Datagram: void * const, 待发送数据报指针
**          LenBytes: int const, 数据报长度
**
** 输出参数:
**          返回值: int, >  0, 发送成功(返回发送数据报长度, 单位:BYTE)
**                         -1, 句柄无效
**                         -2, Datagram为空或LenBytes小于1
**
** 设计注记:
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
** 函数名: PPP_Recv
**
** 描述: PPP协议接收函数
**
** 输入参数:
**          Handler : void * const, PPP句柄
**          Buffer  : void * const, 存放数据报的缓存指针
**          MaxBytes: int const, Buffer的长度
**
** 输出参数:
**          返回值: int, >= 1, 实际获取到的数据报长度(单位:BYTE)
**                          0, 无数据报可接收
**                         -1, 句柄无效
**                         -2, Buffer为空或MaxBytes长度不足
**
** 设计注记:
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
					  + (StreamBuf[pR_RecvBuf + 4] << 16) + (StreamBuf[pR_RecvBuf + 5] << 24) + 1 + 7; /*+1还原数据报长度 +7计入帧长度*/
				if (   (Length > (MaxBytes + 7)) /*MaxBytes理论上不应小于最大数据报长度(帧长度-帧头-校验和-4字节长度-帧尾)*/
					|| (Length > (pPPP->RecvBuf_LenBytes >> 1)) ) /*为避免Available永远小于Length, 要求缓存长度至少是最大帧长度的两倍*/
					ret_val = -2; /*长度异常(例如帧长度传输中出现差错被放大), 重新匹配帧标志字段*/
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
							ret_val = -2; /*校验失败, 重新匹配帧标志字段*/

						Length -= 7;
						pDatagram = &StreamBuf[pR_RecvBuf + 6];
						memcpy(Buffer, pDatagram, Length);
						ret_val = Length;
						pR_RecvBuf += Length + 7;
					}
					else
						ret_val = -2; /*帧尾匹配失败, 重新匹配帧标志字段*/
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
			for (i = 0; i < pW_RecvBuf; i++) /*本周期接收到不完全帧,搬移到流缓冲首部留待下周期处理*/
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
** 函数名: PPP_Protocol_Init
**
** 描述: PPP(Point to Point Protocol)协议初始化函数
**
** 输入参数:
**          Handler          : PPP_STRUCT_TYPE * const, 需初始化的PPP句柄
**          ARP_Call_Flag    : int const, ARP协议调用标志, 0-非ARP调用 其余-ARP调用
**          Datagram_MaxBytes: int const, 最大数据报长度(<=65536)
**          RecvBuf_LenBytes : int const, 接收流缓存长度, 由传输带宽和协议处理周期综合决定(不可小于最大数据报长度的两倍)
**          SubIO            : void * const, 下层IO句柄
**
** 输出参数:
**          返回值: int, >= 0, 初始化成功
**                         -1, 初始化失败(句柄无效)
**                         -2, 初始化失败(无效参数)
**                         -3, 申请缓存空间失败
**
** 设计注记:
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
		pPPP->Frame = (unsigned char*)malloc((Datagram_MaxBytes + 7 + 3) & 0xFFFFFFFC); /*+7:适配帧; +3&0xFFFFFFFC:取4的整数倍*/
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
