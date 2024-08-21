#include "RingBuffer.h"
#include <math.h>
#include <memory.h>

#pragma warning(disable : 4700)

#define GetDirectEnqueueSize_MACRO(iRet) do\
{\
if(iInPos_ >= iOutPos_){\
	if(iOutPos_ > 0){\
		iRet = ACTUAL_SIZE - iInPos_;\
	}else{\
		iRet = BUFFER_SIZE - iInPos_;\
}}\
else{\
	iRet = iOutPos_ - iInPos_- 1;\
}\
}while(0)\

#define GetDirectDequeueSize_MACRO(iRet) do{\
if(iInPos_ >= iOutPos_){\
	iRet = iInPos_ - iOutPos_;\
}\
else{\
	iRet = ACTUAL_SIZE - iOutPos_;\
}\
}while(0)\


#define GetUseSize_MACRO(iRet) do\
{\
	if(iInPos_ >= iOutPos_){\
		iRet = iInPos_ - iOutPos_;\
	}\
	else{\
		iRet = ACTUAL_SIZE - iOutPos_ + iInPos_;\
	}\
}while(0)\

#define GetFreeSize_MACRO(iRet) do\
{\
int iUseSize;\
GetUseSize_MACRO(iUseSize);\
iRet = BUFFER_SIZE - iUseSize;\
}while(0)\


#define GetInStartPtr_MACRO(iRet) do{\
iRet = (Buffer_ + iInPos_);\
}while(0)\

#define GetOutStartPtr_MACRO(iRet) do{\
iRet = (Buffer_ + iOutPos_);\
}while(0)\

#define MoveInOrOutPos_MACRO(iPos_,iMoveSize) do{\
iPos_ = (iPos_ + iMoveSize) % (ACTUAL_SIZE);\
}while(0);\

// Ư�̻��� : ���� ť�̱� ������ �Ҵ� ũ�Ⱑ BUFFER_SIZE ���� 1 Ŀ����
RingBuffer::RingBuffer(void)
{
	iOutPos_ = iInPos_ = 0;
}

RingBuffer::~RingBuffer(void)
{
}

// Return:  (int) ���� Buffer���� Enqueue�Ǿ��ִ� ũ��
int RingBuffer::GetUseSize(void)
{
	int iRet;
	GetUseSize_MACRO(iRet);
	return iRet;
}

// Return:(int)���� Buffer�� �߰��� Enequeue ������ ũ��
int RingBuffer::GetFreeSize(void)
{
	int iRet;
	GetFreeSize_MACRO(iRet);
	return iRet;
}

//--------------------------------------------------------------------
// ���: Ring Buffer�� Data ����
// Parameters:	IN (char *) Ring Buffer�� Data�� ���� ��� Buffer
//				IN (int) ������ ũ��
// Return:		(int) Ring Buffer�� �� ũ��(��ǻ� 0 Ȥ�� sizeToPut ���� �ϳ���)
// Ư�̻��� : Enqueue��å�� sizeToPut() > GetFreeSize()�̸� 0�� ��ȯ�Ѵ�.
//--------------------------------------------------------------------
int RingBuffer::Enqueue(const char* pSource, int iSizeToPut)
{
	int iFreeSize;
	int iDirectEnqueueSize;
	int iFirstSize;
	int iSecondSize;
	char* pInStartPos;

	GetFreeSize_MACRO(iFreeSize); 
	if (iSizeToPut > iFreeSize)
	{
		// ��ȯ�ϴ� �ʿ����� ������ �����������.
		return 0;
	}
	GetDirectEnqueueSize_MACRO(iDirectEnqueueSize); // 2

	// �������� ��ť �����ѻ���� �������� ������� ũ�ų� ������ �ѹ��� ����
	if (iDirectEnqueueSize >= iSizeToPut) 
		iFirstSize = iSizeToPut;
	else // �ι��� ������ �����ؾ���.
		iFirstSize = iDirectEnqueueSize;

	GetInStartPtr_MACRO(pInStartPos);
	memcpy(pInStartPos, pSource, iFirstSize);
	MoveInOrOutPos_MACRO(iInPos_, iFirstSize);

	iSecondSize = iSizeToPut - iFirstSize;
	if (iSecondSize <= 0)
		return iFirstSize;

	GetInStartPtr_MACRO(pInStartPos);
	memcpy(pInStartPos, pSource + iFirstSize, iSecondSize);
	MoveInOrOutPos_MACRO(iInPos_, iSecondSize);
	return iFirstSize + iSecondSize;
}

//--------------------------------------------------------------------
// ���: Ring Buffer���� Data ����
// Parameters:	OUT (char *) Ring Buffer�� Data�� ���� ��� Buffer
//				IN (int) ������ ũ��
// Return:		(int) Ring Buffer���� ������ pDest�� ������ �������� ũ��
// Ư�̻��� : Dequeue��å�� sizeToRead > GetUseSize()�̸� Dequeue�� ����� ���������ʰ� 0�� ��ȯ�Ѵ�.
//--------------------------------------------------------------------
int RingBuffer::Dequeue(char* pOutDest, int iSizeToOut)
{
	int iUseSize;
	int iDirectDequeueSize;
	int iFirstSize;
	int iSecondSize;
	char* pOutStartPos;

	GetUseSize_MACRO(iUseSize);
	if (iSizeToOut > iUseSize)
	{
		// ����ִ� �����ͺ��� ���� �����͸� �������� �ϸ� �׳� ��ȯ�Ѵ�.
		return 0;
	}
	GetDirectDequeueSize_MACRO(iDirectDequeueSize);

	// �������� ��ť �����ѻ���� �������� ������� ũ�ų� ������ �ѹ��� ����
	if (iDirectDequeueSize > iSizeToOut) 
		iFirstSize = iSizeToOut;
	else // �ι��� ������ �����ؾ���
		iFirstSize = iDirectDequeueSize;


	GetOutStartPtr_MACRO(pOutStartPos);
	memcpy(pOutDest, pOutStartPos, iFirstSize);
	MoveInOrOutPos_MACRO(iOutPos_, iFirstSize);

	iSecondSize = iSizeToOut - iFirstSize;
	if (iSecondSize <= 0)
		return iFirstSize;

	GetOutStartPtr_MACRO(pOutStartPos);
	memcpy(pOutDest + iFirstSize, pOutStartPos, iSecondSize);
	MoveInOrOutPos_MACRO(iOutPos_, iSecondSize);
	return iFirstSize + iSecondSize;
}

// �ش��Լ��� Dequeue�� ���� ������ ���縸 �����ϰ� front�� ��ġ�� �ٲ��� �ʴ´�. 
int RingBuffer::Peek(char* pOutTarget, int iSizeToPeek)
{
	int iUseSize;
	int iDirectPeekSize;
	int iFirstSize;
	int iSecondSize;
	char* pPeekStartPos;
	GetUseSize_MACRO(iUseSize);
	if (iSizeToPeek > iUseSize)
	{
		// ����ִ� �����ͺ��� ���� �����͸� �������� �ϸ� �׳� ��ȯ�Ѵ�.
		return 0;
	}

	GetDirectDequeueSize_MACRO(iDirectPeekSize);
	if (iDirectPeekSize > iSizeToPeek) // �߷��� �ι��� ���ļ� ����
		iFirstSize = iSizeToPeek;
	else // �ѹ��� ����
		iFirstSize = iDirectPeekSize;

	GetOutStartPtr_MACRO(pPeekStartPos);
	memcpy(pOutTarget,pPeekStartPos, iFirstSize);

	iSecondSize = iSizeToPeek - iFirstSize;
	if (iSecondSize <= 0)
		return iFirstSize;

	memcpy(pOutTarget + iFirstSize, Buffer_, iSecondSize);
	return iFirstSize + iSecondSize;
}

void RingBuffer::ClearBuffer(void)
{
	iInPos_ = iOutPos_ = 0;
}


// ��� : rear_ + 1���� ť�� �Ǿ����� �̵����� �ʰ� Enqueue������ �ִ� ũ�⸦ ��ȯ��.
// �� rear+ 1���� front_ - 1������ �Ÿ� Ȥ�� rear_ + 1���� 
int RingBuffer::DirectEnqueueSize(void)
{
	int iRet;
	GetDirectEnqueueSize_MACRO(iRet);
	return iRet;
}

int RingBuffer::DirectDequeueSize(void)
{
	int iRet;
	GetDirectDequeueSize_MACRO(iRet);
	return iRet;
}

// sizeToMove��ŭ rear_�� �̵���Ű�� ���� rear_�� ��ȯ��.
int RingBuffer::MoveInPos(int iSizeToMove)
{
	MoveInOrOutPos_MACRO(iInPos_, iSizeToMove);
	return iInPos_;
}

// sizeToMove��ŭ front_�̵���Ű�� ���� rear_�� ��ȯ��.
int RingBuffer::MoveOutPos(int iSizeToMove)
{
	MoveInOrOutPos_MACRO(iOutPos_, iSizeToMove);
	return iOutPos_;
}

char* RingBuffer::GetWriteStartPtr(void)
{
	char* pRet;
	GetInStartPtr_MACRO(pRet);
	return pRet;
}

char* RingBuffer::GetReadStartPtr(void)
{
	char* pRet;
	GetOutStartPtr_MACRO(pRet);
	return pRet;
}


