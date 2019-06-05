#include"ServerSocket.h"
#include<cassert>
#include<iostream>
#include<stdexcept>
#pragma warning(disable : 4996)
ServerSocket::ServerSocket(int id, SOCKET s)
  :	my_id(id), my_socket(s), my_currentRecv(8192) {
	memset(&my_recvUnit, 0, sizeof(my_recvUnit));
	my_recvUnit.socket = this;
	my_recvUnit.type = 0;
	memset(&my_sendUnit, 0, sizeof(my_sendUnit));
	my_sendUnit.socket = this;
	my_sendUnit.type = 1;
}

ServerSocket::~ServerSocket() {
	closesocket(my_socket);
}

void ServerSocket::onError(int errorType, int errorCode) {
	if (handleError) {
		auto self = shared_from_this();
		handleError(self, errorType, errorCode);
	}
}

void ServerSocket::onFinishedSend(int transfered) {

}

void ServerSocket::onFinishedRecv(int transfered) {
	auto self = shared_from_this();
	handleRecv(self, my_currentRecv.data(), transfered);
	startRecv();
}

void ServerSocket::onClosed() {
	if (handleClose) {
		auto self = shared_from_this();
		handleClose(self);
	}
}

void ServerSocket::sendMessage(const std::string& info) {
	std::lock_guard<std::mutex> lock(my_mutex);
	bool sending = !my_sendBuffers.empty();
	my_sendBuffers.push_back(info);
	if (!sending) {
		sendFrontBuffer();
	}
}

void ServerSocket::sendFrontBuffer() {
	//WSABUF buf
	my_sendWSA.len = my_sendBuffers.front().size();
	my_sendWSA.buf = (char*)my_sendBuffers.front().data();
	auto result = WSASend(my_socket, &my_sendWSA, 1, nullptr, 0, &my_sendUnit, nullptr);
	if (result != 0) {
		auto error = WSAGetLastError();
		if (error != WSA_IO_PENDING) {
			throw std::runtime_error("bad for send message");
		}
	}
}

void ServerSocket::trySendMore() {
	std::lock_guard<std::mutex> lock(my_mutex);
	assert(!my_sendBuffers.empty());
	my_sendBuffers.pop_front();
	if (!my_sendBuffers.empty()) {
		sendFrontBuffer();
	}
}

void ServerSocket::startRecv() {
	//WSABUF buf;
	my_recvWSA.len = my_currentRecv.size();
	my_recvWSA.buf = (char*)my_currentRecv.data();

	DWORD flag = 0;
	auto ret = WSARecv(my_socket, &my_recvWSA, 1, nullptr, &flag,
		static_cast<WSAOVERLAPPED*>(&my_recvUnit), nullptr);
	if (ret != 0) {
		auto code = WSAGetLastError();
		if (code != WSA_IO_PENDING) {
			std::cout << "error for" << code << "\n";
			throw std::runtime_error("bad for sdtart recv");
		}
	}
}