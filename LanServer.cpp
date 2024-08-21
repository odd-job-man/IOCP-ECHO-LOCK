#include "IHandler.h"
#include "LanServer.h"
#include "Packet.h"
#include <process.h>

#include "Logger.h"

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"LoggerMt.lib")



unsigned __stdcall IOCPWorkerThread(LPVOID arg);
unsigned __stdcall AcceptThread(LPVOID arg);

ULONGLONG g_ullID;

#define SERVERPORT 6000

SOCKET g_ListenSock;

BOOL LanServer::Start()
{
	int retval;
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		__debugbreak();
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"WSAStartUp OK!");
	// NOCT에 0들어가면 논리프로세서 수만큼을 설정함
	hcp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	if (!hcp_)
		__debugbreak();
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"Create IOCP OK!");

	SYSTEM_INFO si;
	GetSystemInfo(&si);

	HANDLE hIOCPWorkerThread;
	HANDLE hAcceptThread;

	hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, 0, nullptr);
	if (!hAcceptThread)
		__debugbreak();
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE AccpetThread OK!");

	for (DWORD i = 0; i < si.dwNumberOfProcessors; ++i)
	{
		hIOCPWorkerThread = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, this, 0, nullptr);
		if (!hIOCPWorkerThread)
			__debugbreak();
		CloseHandle(hIOCPWorkerThread);
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE IOCP WorkerThread OK Num : %u!", si.dwNumberOfProcessors);

	g_ListenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (g_ListenSock == INVALID_SOCKET)
		__debugbreak();
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE Listen SOCKET OK");

	// bind
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(g_ListenSock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
		__debugbreak();
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"bind OK");

	// listen
	retval = listen(g_ListenSock, SOMAXCONN);
	if (retval == SOCKET_ERROR)
		__debugbreak();
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"listen() OK");

	linger linger;
	linger.l_linger = 0;
	linger.l_onoff = 1;
	setsockopt(g_ListenSock, IPPROTO_TCP, SO_LINGER, (char*)&linger, sizeof(linger));

	InitializeSRWLock(&SessionUMapLock_);
	return 0;
}

unsigned __stdcall IOCPWorkerThread(LPVOID arg)
{
	LanServer* pLanServer;
	Session* pSession;
	WSAOVERLAPPED* pOverlapped;
	//클라이언트 정보 얻기
	SOCKADDR_IN clientaddr;
	int addrlen = sizeof(clientaddr);
	DWORD dwNOBT;
	BOOL bGQCSRet;
	DWORD dwRefCnt;
	DWORD dwErrCode;

	pLanServer = (LanServer*)arg;

	while (1)
	{
		pOverlapped = nullptr;
		dwNOBT = 0;
		pSession = nullptr;
		bGQCSRet = GetQueuedCompletionStatus(pLanServer->hcp_, &dwNOBT, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&pOverlapped, INFINITE);

		if (!pOverlapped && !dwNOBT && !pSession)
			return 0;

		dwRefCnt = _InterlockedDecrement(&pSession->dwRefCnt);
		if (dwNOBT == 0)
		{
			if (dwRefCnt > 0)
				continue;

			dwErrCode = WSAGetLastError();
			pLanServer->ReleaseSession(pSession);
			continue;
		}

		// I/O가 실패햇을때의 예외처리,	
		if (!bGQCSRet && pOverlapped)
		{
			if (dwRefCnt > 0)
				continue;

			dwErrCode = WSAGetLastError();
			if (dwErrCode == WSAECONNRESET)
				goto lb_GQCSDiscon;

		lb_GQCSDiscon:
			pLanServer->ReleaseSession(pSession);
			continue;
		}

		EnterCriticalSection(&pSession->sessionLock);
		if (&pSession->recvOverlapped == pOverlapped)
		{
			SHORT shHeader;
			Packet pckt;

			pSession->recvRB.MoveInPos(dwNOBT);
			while (1)
			{
				int iPeekRet = pSession->recvRB.Peek((char*)&shHeader, sizeof(shHeader));
				if (iPeekRet == 0)
					break;

				int iUseSize = pSession->recvRB.GetUseSize();
				if (iUseSize < sizeof(shHeader) + shHeader)
					break;

				pSession->recvRB.MoveOutPos(sizeof(shHeader));
				pSession->recvRB.Dequeue(pckt.GetBufferPtr(), shHeader);
				pckt.MoveWritePos(shHeader);
				if (!pLanServer->OnRecv(pSession->ullID, &pckt))
				{
					LeaveCriticalSection(&pSession->sessionLock);
					pLanServer->ReleaseSession(pSession);
					goto lb_next;
				}
				pckt.Clear();
			}
			if (!pLanServer->RecvPost(pSession))
			{
				LeaveCriticalSection(&pSession->sessionLock);
				pLanServer->ReleaseSession(pSession);
				goto lb_next;
			}
			LeaveCriticalSection(&pSession->sessionLock);
		}
		else
		{
			pSession->sendRB.MoveOutPos(dwNOBT);
			pSession->bSendingInProgress = FALSE;

			int iUseSize = pSession->sendRB.GetUseSize();
			if (iUseSize <= 0)
			{
				LeaveCriticalSection(&pSession->sessionLock);
				goto lb_next;
			}

			if (!pLanServer->SendPost(pSession))
			{
				LeaveCriticalSection(&pSession->sessionLock);
				pLanServer->ReleaseSession(pSession);
				goto lb_next;
			}
			LeaveCriticalSection(&pSession->sessionLock);
		}
	lb_next:;
	}
}

unsigned __stdcall AcceptThread(LPVOID arg)
{
	SOCKET clientSock;
	SOCKADDR_IN clientAddr;
	int addrlen;
	LanServer* pLanServer = (LanServer*)arg;
	Session* pSession;
	addrlen = sizeof(clientAddr);

	while (1)
	{
		clientSock = accept(g_ListenSock, (SOCKADDR*)&clientAddr, &addrlen);
		if (clientSock == INVALID_SOCKET)
			break;

		if (!pLanServer->OnConnectionRequest())
		{
			closesocket(clientSock);
			continue;
		}
		++pLanServer->dwAcceptTPS;

		pSession = new Session;
		InitializeCriticalSection(&pSession->sessionLock);
		pSession->sock = clientSock;
		pSession->bSendingInProgress = FALSE;
		pSession->ullID = g_ullID;
		pSession->dwRefCnt = 0;
		pSession->recvRB.iInPos_ = pSession->recvRB.iOutPos_ = 0;
		pSession->sendRB.iInPos_ = pSession->sendRB.iOutPos_ = 0;
		CreateIoCompletionPort((HANDLE)pSession->sock, pLanServer->hcp_, (ULONG_PTR)pSession, 0);

		AcquireSRWLockExclusive(&pLanServer->SessionUMapLock_);
		pLanServer->sessionUMap_.insert(std::pair<ULONGLONG, Session*>{g_ullID, pSession});
		ReleaseSRWLockExclusive(&pLanServer->SessionUMapLock_);
		++g_ullID;

		// onAccept();
		pLanServer->RecvPost(pSession);
	}

	return 0;
}


#define MAX_SESSION 1000
BOOL LanServer::OnConnectionRequest()
{
	if (SessionNum_ + 1 >= MAX_SESSION)
		return FALSE;


	return TRUE;
}

void* LanServer::OnAccept(ULONGLONG ullID)
{
	return nullptr;
}

BOOL LanServer::OnRecv(ULONGLONG ullID, Packet* pPacket)
{
	ULONGLONG ullPayLoad;
	Packet sendPacket;
	(*pPacket) >> ullPayLoad;

	sendPacket << ullPayLoad;
	return SendPacket(ullID, &sendPacket);
}

BOOL LanServer::SendPacket(ULONGLONG ullID, Packet* pPacket)
{
	AcquireSRWLockShared(&SessionUMapLock_);
	auto iter = sessionUMap_.find(ullID);
	Session* pSession = iter->second;
	EnterCriticalSection(&pSession->sessionLock);
	ReleaseSRWLockShared(&SessionUMapLock_);

	SHORT shHeader = pPacket->GetUsedDataSize();
	pSession->sendRB.Enqueue((const char*)&shHeader, sizeof(shHeader));
	pSession->sendRB.Enqueue(pPacket->GetBufferPtr(), shHeader);
	LeaveCriticalSection(&pSession->sessionLock);
	return SendPost(pSession);
}


BOOL LanServer::RecvPost(Session* pSession)
{
	DWORD dwRefCnt;
	DWORD flags;
	int iRecvRet;
	DWORD dwErrCode;
	WSABUF wsa[2];

	EnterCriticalSection(&pSession->sessionLock);
	wsa[0].buf = pSession->recvRB.GetWriteStartPtr();
	wsa[0].len = pSession->recvRB.DirectEnqueueSize();
	wsa[1].buf = pSession->recvRB.Buffer_;
	wsa[1].len = pSession->recvRB.GetFreeSize() - wsa[0].len;

	ZeroMemory(&pSession->recvOverlapped, sizeof(WSAOVERLAPPED));
	InterlockedIncrement(&(pSession->dwRefCnt));
	flags = 0;
	iRecvRet = WSARecv(pSession->sock, wsa, 2, nullptr, &flags, &(pSession->recvOverlapped), nullptr);
	if (iRecvRet == SOCKET_ERROR)
	{
		dwErrCode = WSAGetLastError();
		if (dwErrCode == WSA_IO_PENDING)
			goto lb_release;

		dwRefCnt = InterlockedDecrement(&(pSession->dwRefCnt));
		if (dwRefCnt > 0)
			goto lb_release;

		if (dwErrCode == WSAECONNRESET)
			goto lb_disconnect;

		//logging
	lb_disconnect:
		LeaveCriticalSection(&pSession->sessionLock);
		return FALSE;
	}
lb_release:
	LeaveCriticalSection(&pSession->sessionLock);
	return TRUE;
}

BOOL LanServer::SendPost(Session* pSession)
{
	WSABUF wsa[2];
	EnterCriticalSection(&pSession->sessionLock);
	int iUseSize = pSession->sendRB.GetUseSize();
	int iDirectDeqSize = pSession->sendRB.DirectDequeueSize();
	int iBufLen = 0;
	int iSendRet;
	DWORD dwRefCnt;

	if (iUseSize == 0)
	{
		LeaveCriticalSection(&pSession->sessionLock);
		return TRUE;
	}

	if (pSession->bSendingInProgress)
	{
		LeaveCriticalSection(&pSession->sessionLock);
		return TRUE;
	}

	if (iUseSize <= iDirectDeqSize)
	{
		wsa[0].buf = pSession->sendRB.GetReadStartPtr();
		wsa[0].len = iUseSize;
		iBufLen = 1;
	}
	else
	{
		wsa[0].buf = pSession->sendRB.GetReadStartPtr();
		wsa[0].len = iDirectDeqSize;
		wsa[1].buf = pSession->sendRB.Buffer_;
		wsa[1].len = iUseSize - wsa[0].len;
		iBufLen = 2;
	}

	ZeroMemory(&(pSession->sendOverlapped), sizeof(WSAOVERLAPPED));

	dwRefCnt = InterlockedIncrement(&pSession->dwRefCnt);
	iSendRet = WSASend(pSession->sock, wsa, iBufLen, NULL, 0, &(pSession->sendOverlapped), NULL);
	if (iSendRet == SOCKET_ERROR)
	{
		DWORD dwErrCode = WSAGetLastError();
		if (dwErrCode == WSA_IO_PENDING)
		{
			LeaveCriticalSection(&pSession->sessionLock);
			return TRUE;
		}

		dwRefCnt = InterlockedDecrement(&pSession->dwRefCnt);
		if (dwRefCnt > 0)
		{
			LeaveCriticalSection(&pSession->sessionLock);
			return TRUE;
		}

		if (dwErrCode == WSAECONNRESET)
			goto lb_disconnect;

		LOG(L"Disconnect", ERR, TEXTFILE, L"WSASend() Fail Client Disconnect By ErrCode : %u", dwErrCode);
	lb_disconnect:
		LeaveCriticalSection(&pSession->sessionLock);
		return FALSE;
	}
	LeaveCriticalSection(&pSession->sessionLock);
	pSession->bSendingInProgress = TRUE;
	return TRUE;
}

void LanServer::ReleaseSession(Session* pSession)
{
	AcquireSRWLockExclusive(&SessionUMapLock_);
	sessionUMap_.erase(pSession->ullID);
	EnterCriticalSection(&pSession->sessionLock);
	LeaveCriticalSection(&pSession->sessionLock);
	closesocket(pSession->sock);
	delete pSession;
	ReleaseSRWLockExclusive(&SessionUMapLock_);
}

