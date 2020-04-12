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

void Communicator::Push(void* ptr, ULONG ctx)
{
	Context _ctx;
	_ctx.ctx = ctx;
	_ctx.packet = reinterpret_cast<Packet::TransmitionPacket*>(ptr);
	waiting.push(_ctx);
}

void Communicator::PushNew(void* ptr, ULONG ctx)
{
	Context _ctx;
	_ctx.ctx = ctx;
	_ctx.packet = reinterpret_cast<Packet::TransmitionPacket*>(ptr);
	_ctx.packet->length = htonl(_ctx.packet->length);
	_ctx.packet->SubType = htons(_ctx.packet->SubType);
	waiting.push(_ctx);
}

void Communicator::Receive()
{
	WSANETWORKEVENTS networkEvents;
	DWORD idx;
	CBuffer cBuffer(128);

	while (true) {

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

			CBuffer tb(sizeof(Address));
			SOCKET s = accept(serial->getRaw(), reinterpret_cast<sockaddr*>(&addr), &szAcp);
			recv(s, tb.getBuffer(), sizeof(Address), 0);
			send(s, reinterpret_cast<char*>(&myAddress), sizeof(Address), 0);
			AddLink(s, *reinterpret_cast<Address*>(tb.getBuffer()));
		}

		if (networkEvents.lNetworkEvents & FD_READ) {
			int readbytes = serial->Read(cBuffer.getBuffer(), 128);
			auto buffer = buffers.at(idx);
			buffer->buffer.push(cBuffer, readbytes);
			if (buffer->length == -1 && buffer->buffer.Used() >= sizeof(long)) {
				buffer->buffer.poll(cBuffer, 4);
				buffer->length = ntohl(*reinterpret_cast<int*>(cBuffer.getBuffer()));
			}
			
			if (buffer->length != -1 && buffer->buffer.Used() >= (buffer->length+sizeof(short))) {
				CBuffer packetBuffer(buffer->length+sizeof(short));
				buffer->buffer.poll(packetBuffer, buffer->length+ sizeof(short));
				
				auto subType = *reinterpret_cast<short*>(packetBuffer.getBuffer());
				auto packet = reinterpret_cast<Packet::TransmitionPacket*>(new BYTE[buffer->length]+sizeof(short)+sizeof(long));
				memcpy(packet + sizeof(long), packetBuffer.getBuffer(), buffer->length+ sizeof(short));
				packet->length = htonl(buffer->length);
				Push(packet, 0);
				buffer->length = -1;
			}
			
		}

		if (networkEvents.lNetworkEvents & FD_CLOSE) {
			lock.lock();
			DisconnectLink(idx);
			lock.unlock();
		}
	}
}

void Communicator::Respond()
{
	register long counter = 0;
	Context ctx;
	Packet::TransmitionPacket* tpack;
	while (run) {
		if (!waiting.empty()) {
			if (waiting.try_pop(ctx)) {
				tpack = ctx.packet;
				auto packet = reinterpret_cast<Packet::TPacket*>(tpack);
				if (packet->to == myAddress)
				{
					switch (ntohs(packet->SubType)) {
					case Packet::PRT_ROUTING:
					{
						auto p = static_cast<Packet::RoutingPacket*>(tpack);
						router.Update(p->address, (Serial*)ctx.ctx, p->hop);
					}
					case Packet::PRT_DATA:
					{
						auto p = static_cast<Packet::DataPacket*>(packet);
						printf("DATA %4s\n", p->Data);
						break;
					}
					case Packet::PRT_ICMP:
					{
						auto p = static_cast<Packet::ICMPPacket*>(packet);
						printf("ICMP %d %d\n", p->flag, p->from);
						break;
					}
					case Packet::PRT_INNER_REMOVE:
					{
						Serial* serial = reinterpret_cast<Serial*>(ctx.ctx);
						router.Detach(serial);
						delete serial;
						break;
					}
					}
				}
				else
				{
					Serial* serial;
					if (router.Query(packet->to, &serial)) {
						int clen = ntohl(packet->length) + sizeof(long) + sizeof(short);
						if (!SendCompletely(serial, reinterpret_cast<char*>(packet), clen)) {
							if (ctx.ctx > 5)
								PushNew(new Packet::ICMPPacket(myAddress, packet->from, htonl(1)), 0);
							else {
								router.Detach(serial);
								Push(tpack, ctx.ctx++);
								tpack = nullptr;
							}
						}
					}
					else {
						PushNew(new Packet::ICMPPacket(myAddress, packet->from, htonl(2)), 0);
					}
				}


				if (tpack != nullptr)
					delete (BYTE*)tpack;
			}
		}
		else if (counter <= GetTickCount64()) {
			counter = GetTickCount64() + 250;
			auto dvec = router.GetRandomRoute();
			lock.lock();
			std::uniform_int_distribution<int> dis(0, links.size());
			Push(new Packet::RoutingPacket(dvec.distance + 1, dvec.address, atable[links.at(dis(gen))]), 0);
			lock.unlock();
		}
	}
}

void Communicator::Stop()
{
	run = false;
	Release();
}

void Communicator::AddLink(SOCKET s, Address addr)
{
	WSAEVENT e = WSACreateEvent();
	WSAEventSelect(s, e, FD_READ | FD_CLOSE);
	events.push(e);
	lock.lock();
	Serial* serial = new SocketSerial(s);
	links.push(serial);
	buffers.push(new TemporaryBuffer);
	atable[serial] = addr;
	lock.unlock();
}

void Communicator::DisconnectLink(int idx)
{
	WSACloseEvent(events.at(idx));
	events.pop(idx);
	Push(new Packet::TPacket(0, Packet::PRT_INNER_REMOVE, myAddress, myAddress), reinterpret_cast<ULONG>(links.at(idx)));
	atable.erase(links.at(idx));
	links.pop(idx);
	delete buffers.at(idx);
	buffers.pop(idx);
}

void Communicator::Release()
{
	while (!waiting.empty()) {
		Context ctx;
		if (waiting.try_pop(ctx)) {
			auto p = static_cast<Packet::TPacket*>(ctx.packet);
			if (p->to == myAddress) {
				delete ctx.packet;
			}
			else {
				Serial* serial;
				if (router.Query(p->to, &serial)) {
					if (!SendCompletely(serial, reinterpret_cast<char*>(ctx.packet), ntohl(ctx.packet->length)+sizeof(long)+sizeof(short))) {
						if (ctx.ctx > 5)
							Push(new Packet::ICMPPacket(myAddress, p->from, 2), 0);
						else {
							router.Detach(serial);
							Push(ctx.packet, ctx.ctx++);
						}
					}
					else {
						delete ctx.packet;
					}
				}
				else {
					Push(new Packet::ICMPPacket(myAddress, p->from, 2), 0);
				}
			}
		}
	}

	for (int i = 0; i < links.size(); i++) {
		delete links.at(i);
	}
}

void Communicator::Start(sockaddr_in tnode)
{
	myAddress = CreateRandomAddress();
	auto listener = new SocketSerial(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
	sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htonl(7777);
	bind(listener->getRaw(), reinterpret_cast<sockaddr*>(&server), sizeof(sockaddr_in));
	listen(listener->getRaw(), 5);

	AddListener(listener);

	if (tnode.sin_addr.s_addr == INADDR_NONE);
	return;
	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	while (connect(s, reinterpret_cast<sockaddr*>(&tnode), sizeof(sockaddr_in)) != 0);

	send(s, reinterpret_cast<char*>(&myAddress), sizeof(Address), 0);
	CBuffer tb(sizeof(Address));
	recv(s, tb.getBuffer(), sizeof(Address), 0);

	AddLink(s, *reinterpret_cast<Address*>(tb.getBuffer()));
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