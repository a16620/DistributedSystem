#pragma once
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#include <concurrent_queue.h>
#include <unordered_map>
#include <future>
#include "Packet.h"
#include "SocketSerial.h"
#include "def.h"

class ClientCommunicator
{
	Serial* link;
	WSAEVENT levent;
	Address myAddress;
	std::unordered_map<Address, size_t> addressTable;
public:
	std::atomic_bool run;
	concurrency::concurrent_queue<Packet::TransmitionPacket*> input, output;

	void Start(ULONG addr);
	void Stop();
	void Release();
	void Task();
};

bool SendCompletely(Serial* serial, const char* buffer, size_t length);