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

// 특이사항 : 원형 큐이기 때문에 할당 크기가 BUFFER_SIZE 보다 1 커야함
RingBuffer::RingBuffer(void)
{
	iOutPos_ = iInPos_ = 0;
}

RingBuffer::~RingBuffer(void)
{
}

// Return:  (int) 현재 Buffer에서 Enqueue되어있는 크기
int RingBuffer::GetUseSize(void)
{
	int iRet;
	GetUseSize_MACRO(iRet);
	return iRet;
}

// Return:(int)현재 Buffer에 추가로 Enequeue 가능한 크기
int RingBuffer::GetFreeSize(void)
{
	int iRet;
	GetFreeSize_MACRO(iRet);
	return iRet;
}

//--------------------------------------------------------------------
// 기능: Ring Buffer에 Data 삽입
// Parameters:	IN (char *) Ring Buffer에 Data를 넣을 대상 Buffer
//				IN (int) 복사할 크기
// Return:		(int) Ring Buffer에 들어간 크기(사실상 0 혹은 sizeToPut 둘중 하나다)
// 특이사항 : Enqueue정책은 sizeToPut() > GetFreeSize()이면 0을 반환한다.
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
		// 반환하는 쪽에서는 연결을 끊어버려야함.
		return 0;
	}
	GetDirectEnqueueSize_MACRO(iDirectEnqueueSize); // 2

	// 직선으로 인큐 가능한사이즈가 넣으려는 사이즈보다 크거나 같으면 한번만 복사
	if (iDirectEnqueueSize >= iSizeToPut) 
		iFirstSize = iSizeToPut;
	else // 두번에 나눠서 복사해야함.
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
// 기능: Ring Buffer에서 Data 꺼냄
// Parameters:	OUT (char *) Ring Buffer의 Data를 받을 대상 Buffer
//				IN (int) 복사할 크기
// Return:		(int) Ring Buffer에서 꺼내서 pDest에 복사한 데이터의 크기
// 특이사항 : Dequeue정책은 sizeToRead > GetUseSize()이면 Dequeue를 제대로 수행하지않고 0을 반환한다.
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
		// 들어있는 데이터보다 많은 데이터를 읽으려고 하면 그냥 반환한다.
		return 0;
	}
	GetDirectDequeueSize_MACRO(iDirectDequeueSize);

	// 직선으로 디큐 가능한사이즈가 읽으려는 사이즈보다 크거나 같으면 한번만 복사
	if (iDirectDequeueSize > iSizeToOut) 
		iFirstSize = iSizeToOut;
	else // 두번에 나눠서 복사해야함
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

// 해당함수는 Dequeue와 거의 같지만 복사만 수행하고 front의 위치가 바뀌지 않는다. 
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
		// 들어있는 데이터보다 많은 데이터를 읽으려고 하면 그냥 반환한다.
		return 0;
	}

	GetDirectDequeueSize_MACRO(iDirectPeekSize);
	if (iDirectPeekSize > iSizeToPeek) // 잘려서 두번에 걸쳐서 복사
		iFirstSize = iSizeToPeek;
	else // 한번에 복사
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


// 기능 : rear_ + 1부터 큐의 맨앞으로 이동하지 않고 Enqueue가능한 최대 크기를 반환함.
// 즉 rear+ 1부터 front_ - 1까지의 거리 혹은 rear_ + 1부터 
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

// sizeToMove만큼 rear_를 이동시키고 현재 rear_를 반환함.
int RingBuffer::MoveInPos(int iSizeToMove)
{
	MoveInOrOutPos_MACRO(iInPos_, iSizeToMove);
	return iInPos_;
}

// sizeToMove만큼 front_이동시키고 현재 rear_를 반환함.
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


