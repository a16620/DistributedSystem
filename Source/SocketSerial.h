#pragma once
#include "Serial.h"
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")

class SocketSerial :
	public Serial
{
	SOCKET m_socket;
public:
	SocketSerial(SOCKET s) : m_socket(s) {}
	virtual ~SocketSerial() override;

	SOCKET getRaw() const {
		return m_socket;
	}

	virtual int Write(const char* buffer, int length) override;
	virtual int Read(char* buffer, int length) override;
};

