#pragma once
#include "def.h"
#include "Serial.h"
#include "SocketSerial.h"
#include "Container.h"
#include "Packet.h"
#include "Router.h"
#include <concurrent_queue.h>
#include <unordered_map>
#include <mutex>

constexpr int linkCount = 4;

class Communicator
{
	struct TemporaryBuffer {
		u_long length = 0;
		RecyclerBuffer<512> buffer;
	};

	SequentArrayList<Serial*, linkCount> links;
	SequentArrayList<WSAEVENT, linkCount> events;
	SequentArrayList<TemporaryBuffer*, linkCount> buffers;
	concurrency::concurrent_queue<Packet::TransmitionPacket*> waiting;
	std::mutex rLock, sLock;

	std::atomic_bool run;

	Router router;

	void AddListener(SocketSerial* lst);
	void Push(void* ptr);
	void AddLink(SOCKET s);
	void DisconnectLink(int idx);

public:
	void Start(ULONG tnode, u_short targetport, u_short port);
	void Release();

	void Receive();
	void Respond();

	void Stop();
};

bool SendCompletely(Serial* serial, const char* buffer, size_t length);