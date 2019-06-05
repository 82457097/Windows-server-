#pragma once
#ifndef UNICODE
#define UNICODE
#endif // !UNICODE
#define WIN32_LEAN_AND_MEAN

#include<WS2tcpip.h>
#include<MSWSock.h>
#include<cstdio>
#include<iostream>
#include<functional>
#include"ServerSocket.h"

class Server {
public:
	Server(u_short port);//构造函数 传入一个端口号
	~Server();
	bool startAccept();
	void waitingForAccept();
	void waitingForIo();
	bool isRunning() const { return my_running; }
	void stop() { my_running = false; }
	typedef std::function<void(ServerSocket::pointer)> HandleNewConnect;
	HandleNewConnect newConn;

	ServerSocket::HandleRecvFunction socketRecv;
	ServerSocket::HandleClose socketClose;
	ServerSocket::HandleError socketError;

private:
	u_short my_port;
	SOCKET my_listenSocket;
	HANDLE my_completePort;
	LPFN_ACCEPTEX lpfnAcceptEx;
	SOCKET my_currentAccptSorcket;
	WSAOVERLAPPED my_acceptUnit;
	HANDLE my_ioCompletePort;
	bool my_running;
	std::vector<char> my_acceptBuffer;
	bool tryNewConn();
};