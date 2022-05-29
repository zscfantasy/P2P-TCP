/*-----------------------------------------------------------
**
** 版权:    
**
** 文件名:  PPP_Protocol.h
**
** 描述:    声明通过点对点通信协议PPP(Point to Point Protocol)进行通讯所需要使用的宏定义和函数
**
** 设计注记:
**
** 作者:
**     
**
** 更改历史:
**    2018年9月17日  创建本文件
**    $Revision$
**    $Date$
**    $Author$
**    $Log$
**
**-----------------------------------------------------------
*/
#ifndef _PPP_Protocol_h
#define _PPP_Protocol_h

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

		char PPP_Data[36];
	} PPP_STRUCT_TYPE;


	/*---------------------------------
	**         函数声明
	**---------------------------------
	*/
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
	extern int PPP_Protocol_Init(PPP_STRUCT_TYPE * const Handler, int const ARP_Call_Flag, int const Datagram_MaxBytes, int const RecvBuf_LenBytes, void * const SubIO);

#ifdef __cplusplus
}
#endif

#endif
