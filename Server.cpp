#include"Server.h"
#include<string>
#include<cassert>
#pragma warning(disable : 4996)
Server::Server(u_short p)
  : my_port(p),
	my_listenSocket(INVALID_SOCKET),
	my_completePort(NULL),
	lpfnAcceptEx(nullptr),
	my_currentAccptSorcket(INVALID_SOCKET),
	my_ioCompletePort(NULL),
	my_running(false),my_acceptBuffer(1024) {}

Server::~Server() {
	if (my_listenSocket != INVALID_SOCKET) closesocket(my_listenSocket);
	CloseHandle(my_ioCompletePort);
	CloseHandle(my_completePort);
}

bool Server::startAccept() {
	my_completePort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (!my_completePort) {
		return false;
	}

	my_ioCompletePort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (!my_ioCompletePort) {
		return false;
	}

	my_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (my_listenSocket == INVALID_SOCKET) {
		return false;
	}

	CreateIoCompletionPort((HANDLE)my_listenSocket, my_completePort, 0, 0);

	hostent* localHost = gethostbyname("");
	char ip[64];
	inet_ntop(AF_INET, (struct in_addr*)*localHost->h_addr_list, ip, sizeof(ip));
	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr(ip);
	service.sin_port = htons(my_port);

	BOOL reuse = TRUE;
	setsockopt(my_listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse,
		sizeof(reuse));
	if (bind(my_listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
		std::cout << "bind failed with error:" << WSAGetLastError() << "\n";
		return false;
	}

	auto result = listen(my_listenSocket, 100);
	if (result == SOCKET_ERROR) {
		std::cout << "listen failed with error:" << WSAGetLastError() << "\n";
		return false;
	}

	DWORD dwBytes = 0;
	GUID guidAcceptEx = WSAID_ACCEPTEX;
	result = WSAIoctl(my_listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guidAcceptEx, sizeof(guidAcceptEx), &lpfnAcceptEx,
		sizeof(lpfnAcceptEx), &dwBytes, nullptr, nullptr);
	if (result == SOCKET_ERROR) {
		std::cout << "WSAIoctl failed with error:" << WSAGetLastError() << "\n";
		return false;
	}

	my_currentAccptSorcket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (my_currentAccptSorcket == INVALID_SOCKET)
		return false;

	//char lpOutputBuf[1024];
	int outBufLen = 1024;
	memset(&my_acceptUnit, 0, sizeof(my_acceptUnit));
	auto ret = lpfnAcceptEx(my_listenSocket, my_currentAccptSorcket,
		my_acceptBuffer.data(), 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
		nullptr, &my_acceptUnit);
	if (ret == FALSE && WSAGetLastError() == ERROR_IO_PENDING) {
		std::cout << "start listen" << ip << ":" << my_port << std::endl;
		my_running = true;
	}

	return my_running;
}

bool Server::tryNewConn() {
	//char lpPutputBuf[1024];
	int outBuflen = 1024;
	memset(&my_acceptUnit, 0, sizeof(WSAOVERLAPPED));
	auto ret = lpfnAcceptEx(my_listenSocket, my_currentAccptSorcket,
		my_acceptBuffer.data(), 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
		nullptr, &my_acceptUnit);
	if (ret == FALSE && WSAGetLastError() == ERROR_IO_PENDING) {
		return true;
	}
	return false;
}

void Server::waitingForAccept() {
	int id = 1;
	while (isRunning()) {
		DWORD bytes = 0;
		ULONG_PTR dwCompletionKey;
		LPOVERLAPPED lpOverlapped = nullptr;
		auto ok = GetQueuedCompletionStatus(my_completePort, &bytes,
			&dwCompletionKey, &lpOverlapped, 1000);
		if (ok) {
			std::string info(my_acceptBuffer.begin(), my_acceptBuffer.begin() + bytes);
			CreateIoCompletionPort((HANDLE)my_currentAccptSorcket, my_ioCompletePort, 0, 0);
			auto ns = std::make_shared<ServerSocket>(id, my_currentAccptSorcket);
			ns->handleClose = socketClose;
			ns->handleError = socketError;
			ns->handleRecv = socketRecv;
			if (newConn) {
				newConn(ns);
			}
		}
		++id;
		my_currentAccptSorcket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		ok = tryNewConn();
		if (!ok)
			break;
		else {
			auto errorCode = GetLastError();
			if (errorCode = WAIT_TIMEOUT)
				continue;
			std::cout << "has error " << errorCode << "in accept new client"
				<< WSAGetLastError() << "\n";
			break;
		}
	}
}

void Server::waitingForIo() {
	while (isRunning()) {
		DWORD bytes = 0;
		ULONG_PTR dwCompletionKey;
		LPOVERLAPPED lpOverlapped = nullptr;
		auto ok = GetQueuedCompletionStatus(my_ioCompletePort, &bytes,
			&dwCompletionKey, &lpOverlapped, 1000);
		if (ok) {
			auto unit = (OverUnit*)lpOverlapped;
			assert(unit);
			auto socket = unit->socket;
			if (bytes == 0) {
				socket->onClosed();
			}
			else {
				if (unit->type == 0) {
					socket->onFinishedRecv(bytes);
				}
				else if (unit->type == 1) {
					socket->onFinishedSend(bytes);
					socket->trySendMore();
				}
			}
		}
		else {
			auto errorCode = GetLastError();
			if (errorCode == WAIT_TIMEOUT)
				continue;
			std::cout << "has error " << errorCode << "in accept new client"
				<< WSAGetLastError() << "\n";
			if (!lpOverlapped) {
				break;
			}
			else {
				OverUnit* unit = (OverUnit*)lpOverlapped;
				auto socket = unit->socket;
				if (socket) {
					socket->onError(unit->type, WSAGetLastError());
				}
			}
		}
	}
}