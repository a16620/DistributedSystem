#include "SocketSerial.h"

SocketSerial::~SocketSerial()
{
	closesocket(m_socket);
}

int SocketSerial::Write(const char* buffer, int length)
{
	auto r = send(m_socket, buffer, length, 0);
	if (r == -1)
		throw "Socket Send Error";
	return r;
}

int SocketSerial::Read(char* buffer, int length)
{
	auto r = recv(m_socket, buffer, length, 0);
	if (r == -1)
		throw "Socket Recv Error";
	return r;
}
