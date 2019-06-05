#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN

#include<winsock2.h>
#include<WS2tcpip.h>
#include<MSWSock.h>
#include<memory>
#include<deque>
#include<vector>
#include<functional>
#include<mutex>

class ServerSocket;
struct OverUnit : WSAOVERLAPPED {
	ServerSocket* socket;
	int type;
};

class ServerSocket : public std::enable_shared_from_this<ServerSocket> {
public:
	using pointer = std::shared_ptr<ServerSocket>;
	typedef std::function<void(const pointer&, const char* data, int size)>
		HandleRecvFunction;
	typedef std::function<void(const pointer&)> HandleClose;
	HandleRecvFunction handleRecv;
	HandleClose handleClose;
	typedef std::function<void(const pointer&, int, int)> HandleError;
	HandleError handleError;

	ServerSocket(int id, SOCKET s);
	~ServerSocket();
	void onError(int errorType, int errorCode);
	void onClosed();
	void onFinishedSend(int transfered);
	void onFinishedRecv(int transfered);
	int id() const { return my_id; }
	void startRecv();
	void sendMessage(const std::string &info);
	void trySendMore();

private:
	int my_id;
	SOCKET my_socket;
	using Buffer = std::vector<char>;
	Buffer my_currentRecv;
	std::deque<Buffer> my_recvBuffers;
	std::mutex my_mutex;
	std::deque<std::string> my_sendBuffers;
	OverUnit my_recvUnit;
	OverUnit my_sendUnit;
	void sendFrontBuffer();
	WSABUF my_sendWSA;
	WSABUF my_recvWSA;
};