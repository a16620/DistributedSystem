#include "Communicator.h"
#include "Router.h"
#include <random>

void Communicator::AddListener(SocketSerial* lst)
{
	WSAEVENT e = WSACreateEvent();
	WSAEventSelect(lst->getRaw(), e, FD_ACCEPT);
	events.push(e);
	links.push(static_cast<Serial*>(lst));
	buffers.push(nullptr);
}

void Communicator::Push(void* ptr)
{
	waiting.push(reinterpret_cast<Packet::TransmitionPacket*>(ptr));
}

void Communicator::Receive()
{
	WSANETWORKEVENTS networkEvents;
	DWORD idx;
	CBuffer cBuffer(128);

	while (run) {

		idx = WSAWaitForMultipleEvents(events.size(), events.getArray(), FALSE, 1000, FALSE);
		if (idx == WSA_WAIT_FAILED || idx == WSA_WAIT_TIMEOUT)
			continue;
		idx = idx - WSA_WAIT_EVENT_0;
		if (WSAEnumNetworkEvents(links.at(idx)->getRaw(), events.at(idx), &networkEvents) == SOCKET_ERROR)
			continue;

		auto serial = links.at(idx);

		if (networkEvents.lNetworkEvents & FD_ACCEPT) {
			sockaddr_in addr;
			int szAcp = sizeof(sockaddr_in);
			SOCKET s = accept(serial->getRaw(), reinterpret_cast<sockaddr*>(&addr), &szAcp);
			AddLink(s);
		}

		if (networkEvents.lNetworkEvents & FD_READ) {
			auto buffer = buffers.at(idx);
			u_long hasCount;
			ioctlsocket(serial->getRaw(), FIONREAD, &hasCount);
			if (buffer->buffer.Length - buffer->buffer.Used() >= hasCount)
			{
				int readbytes = serial->Read(cBuffer.getBuffer(), 128);
				buffer->buffer.push(cBuffer, readbytes);
			}
			if (buffer->length == 0 && buffer->buffer.Used() >= sizeof(long)) {
				buffer->buffer.poll(cBuffer, 4);
				buffer->length = ntohl(*reinterpret_cast<u_long*>(cBuffer.getBuffer()));
			}
			
			if (buffer->length != 0 && buffer->buffer.Used() >= (buffer->length+4)) {
				CBuffer packetBuffer(buffer->length+4);
				buffer->buffer.poll(packetBuffer, buffer->length+4);
				
				auto packet = reinterpret_cast<Packet::PacketFrame*>(new BYTE[buffer->length+sizeof(Packet::PacketFrame)]);
				memcpy((BYTE*)packet + sizeof(long), packetBuffer.getBuffer(), buffer->length+4);
				packet->length = htonl(buffer->length);
				switch (ntohs(packet->SubType)) {
				case Packet::PRT_INTER_ROUTING:
				{
					auto rp = reinterpret_cast<Packet::RoutingPacket*>(packet);
					rLock.lock();
					router.Update(rp->address, serial, ntohl(rp->hop));
					rLock.unlock();
					delete[] (BYTE*)packet;
					break;
				}
				default:
				{
					Push(packet);
				}
				}
				buffer->length = 0;
			}
			
		}

		if (networkEvents.lNetworkEvents & FD_CLOSE) {
			DisconnectLink(idx);
		}
	}
}

void Communicator::Respond()
{
	register ULONGLONG counter = 0;
	Packet::TransmitionPacket* packet;
	while (run) {
		if (!waiting.empty()) {
			if (waiting.try_pop(packet)) {
				Serial* serial;
				rLock.lock();
				if (router.Query(packet->to, &serial)) {
					int clen = ntohl(packet->length) + sizeof(Packet::PacketFrame);
					for (int i = 0; i < clen; ++i)
						printf("%d ", *((BYTE*)packet + i));
					printf("\n\n");
					if (!SendCompletely(serial, reinterpret_cast<char*>(packet), clen)) {
						router.Detach(serial);
						Push(packet);
						packet = nullptr;
					}
				}
				else {
					Push(Packet::Encode(Packet::Generate<Packet::ICMPPacket>(packet->to, packet->from, htonl(1))));
				}
				rLock.unlock();


				if (packet != nullptr)
					delete[] (BYTE*)packet;
			}
		}
		else if (counter <= GetTickCount64()) {
			counter = GetTickCount64() + 3000;
			rLock.lock();
			Router::dvec dvec;
			auto res = router.GetRandomRoute(dvec);
			rLock.unlock();
			if (!res)
				continue;
			auto rpk = Packet::Encode(Packet::Generate<Packet::RoutingPacket>(htonl(dvec.distance), dvec.address));
			sLock.lock();
			for (int i = 1; i < links.size(); ++i) {
				Serial* nb = links.at(i);
				SendCompletely(nb, reinterpret_cast<char*>(rpk), sizeof(Packet::RoutingPacket));
			}
			sLock.unlock();
			delete[] (BYTE*)rpk;
		}
	}
}

void Communicator::Stop()
{
	run = false;
}

void Communicator::AddLink(SOCKET s)
{
	WSAEVENT e = WSACreateEvent();
	WSAEventSelect(s, e, FD_READ | FD_CLOSE);
	events.push(e);
	Serial* serial = new SocketSerial(s);
	sLock.lock();
	links.push(serial);
	sLock.unlock();
	buffers.push(new TemporaryBuffer);
}

void Communicator::DisconnectLink(int idx)
{
	WSACloseEvent(events.at(idx));
	events.pop(idx);
	rLock.lock();
	auto p = links.at(idx);
	router.Detach(p);
	rLock.unlock();
	sLock.lock();
	links.pop(idx);
	delete p;
	sLock.unlock();
	delete buffers.at(idx);
	buffers.pop(idx);
}

void Communicator::Release()
{
	while (!waiting.empty()) {
		Packet::TransmitionPacket* packet;
		if (waiting.try_pop(packet)) {
			delete[] (BYTE*)packet;
		}
	}

	for (int i = 0; i < links.size(); i++) {
		delete links.at(i);
	}
}

void Communicator::Start(ULONG tnode)
{
	auto listener = new SocketSerial(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
	sockaddr_in server;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_family = AF_INET;
	server.sin_port = htons(7777);
	bind(listener->getRaw(), reinterpret_cast<sockaddr*>(&server), sizeof(sockaddr_in));
	listen(listener->getRaw(), 5);

	AddListener(listener);
	run = true;

	if (tnode == INADDR_NONE)
		return;
	sockaddr_in tar;
	tar.sin_family = AF_INET;
	tar.sin_port = htons(7777);
	tar.sin_addr.s_addr = htonl(tnode);
	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	while (connect(s, reinterpret_cast<sockaddr*>(&tar), sizeof(sockaddr_in)) != 0);

	AddLink(s);
}

bool SendCompletely(Serial* serial, const char* buffer, size_t length) {
	size_t r = 0;
	do
	{
		int r_ = serial->Write(buffer + r, length - r);
		if (r_ == -1)
			return false;
		r += r_;
	} while (r < length);
	return true;
}