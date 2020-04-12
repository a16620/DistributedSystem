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

Address CreateRandomAddress();

class Communicator
{
	struct TemporaryBuffer {
		long length = -1;
		RecyclerBuffer<512> buffer;
	};

	struct Context {
		Packet::TransmitionPacket* packet;
		ULONG ctx;
	};


	SequentArrayList<Serial*, linkCount> links;
	SequentArrayList<WSAEVENT, linkCount> events;
	SequentArrayList<TemporaryBuffer*, linkCount> buffers;
	concurrency::concurrent_queue<Context> waiting;
	std::unordered_map<Serial*, Address> atable;
	std::mutex lock;

	std::atomic_bool run;

	Router router;
	Address myAddress;

	void AddListener(SocketSerial* lst);
	void Push(void* ptr, ULONG ctx);
	void PushNew(void* ptr, ULONG ctx);
	void AddLink(SOCKET s, Address addr);
	void DisconnectLink(int idx);

	void Release();
public:
	void Start(sockaddr_in tnode);

	void Receive();
	void Respond();

	void Stop();
};

bool SendCompletely(Serial* serial, const char* buffer, size_t length);