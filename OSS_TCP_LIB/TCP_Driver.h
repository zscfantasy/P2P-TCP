/*-----------------------------------------------------------
**
** 版权:    中国航空无线电电子研究所, 2017年
**
** 文件名:  TCP_Driver.h
**
** 描述:    声明了tcp通信所开放的所有接口
**
** 设计注记:
**
** 作者:
**     章诗晨, 2017年1月开始编写本文件
**
** 更改历史:
**    2017年1月3日  创建本文件
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
**         TCP宏定义
**---------------------------------
*/
/*UDP接收模式*/
#define TCP_RECV_BLOCK   0 /*阻塞式接收*/
#define TCP_RECV_NONBLK  1 /*非阻塞式接收*/

#ifdef __cplusplus
extern "C" {
#endif
	/*---------------------------------
	**            类型定义
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
