/*-----------------------------------------------------------
**
** ��Ȩ:    
**
** �ļ���:  PPP_Protocol.h
**
** ����:    ����ͨ����Ե�ͨ��Э��PPP(Point to Point Protocol)����ͨѶ����Ҫʹ�õĺ궨��ͺ���
**
** ���ע��:
**
** ����:
**     
**
** ������ʷ:
**    2018��9��17��  �������ļ�
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
	**            ���Ͷ���
	**---------------------------------
	*/
	typedef struct
	{
		int(*Send)(void * const Handler, void * const Payload, int const LenBytes);
		int(*Recv)(void * const Handler, void * const Buffer, int const MaxBytes);

		char PPP_Data[36];
	} PPP_STRUCT_TYPE;


	/*---------------------------------
	**         ��������
	**---------------------------------
	*/
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
	extern int PPP_Protocol_Init(PPP_STRUCT_TYPE * const Handler, int const ARP_Call_Flag, int const Datagram_MaxBytes, int const RecvBuf_LenBytes, void * const SubIO);

#ifdef __cplusplus
}
#endif

#endif
