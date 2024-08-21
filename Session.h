#pragma once
#include <WinSock2.h>
#include <windows.h>
#include "RingBuffer.h"

struct Session
{
	CRITICAL_SECTION sessionLock;
	SOCKET sock;
	ULONGLONG ullID;
	BOOL bSendingInProgress;
	void* pClient;
	DWORD dwRefCnt;
	WSAOVERLAPPED recvOverlapped;
	WSAOVERLAPPED sendOverlapped;
	RingBuffer recvRB;
	RingBuffer sendRB;
};
