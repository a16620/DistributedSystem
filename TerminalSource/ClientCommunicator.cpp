#include "ClientCommunicator.h"
#include "Container.h"
#pragma warning(disable: 4996)
void ClientCommunicator::Start(ULONG addr, u_short port)
{
	UuidCreate(&myAddress);
	link = new SocketSerial(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
	sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = htonl(addr);
	while (connect(link->getRaw(), (sockaddr*)&sa, sizeof(sa)) == -1);
	levent = WSACreateEvent();
	WSAEventSelect(link->getRaw(), levent, FD_READ | FD_CLOSE);
	run = true;
}

void ClientCommunicator::Stop()
{
	run = false;
	WSACloseEvent(levent);
}

void ClientCommunicator::Release()
{
	delete link;
	Packet::TransmitionPacket* p;
	while (!input.empty()) {
		if (input.try_pop(p)) {
			delete p;
		}
	}

	while (!output.empty()) {
		if (output.try_pop(p)) {
			delete p;
		}
	}
}

void ClientCommunicator::Task()
{
	ULONGLONG counter = 0;
	RecyclerBuffer<512> buffer;
	char tBuffer[128];
	u_long length = 0;
	using Packet::TransmitionPacket;
	WSANETWORKEVENTS networkEvents;
	while (run) {
		auto idx = WSAWaitForMultipleEvents(1, &levent, FALSE, 100, FALSE);
		if (idx == WSA_WAIT_FAILED || idx == WSA_WAIT_TIMEOUT)
			goto endRecv;
		idx = idx - WSA_WAIT_EVENT_0;
		if (WSAEnumNetworkEvents(link->getRaw(), levent, &networkEvents) == SOCKET_ERROR)
			goto endRecv;

		if (networkEvents.lNetworkEvents & FD_READ) {
			u_long hasCount;
			ioctlsocket(link->getRaw(), FIONREAD, &hasCount);
			if (buffer.Length - buffer.Used() >= hasCount) {
			int readbytes = link->Read(tBuffer, 128);
			buffer.push(tBuffer, readbytes);
			if (length == 0 && buffer.Used() >= sizeof(u_long)) {
				buffer.poll(tBuffer, 4);
				length = ntohl(*reinterpret_cast<u_long*>(tBuffer));
			}

			if (length != 0 && buffer.Used() >= (length + sizeof(Packet::PacketFrame) - sizeof(u_long))) {
				auto packetBuffer = new BYTE[length + sizeof(Packet::PacketFrame)];
				buffer.poll(reinterpret_cast<char*>(packetBuffer + sizeof(u_long)), length + sizeof(Packet::PacketFrame) - sizeof(u_long));
				auto pp = reinterpret_cast<Packet::PacketFrame*>(packetBuffer);
				pp->length = length;

				switch (ntohs(pp->SubType)) {
				case Packet::PRT_INTER_ROUTING:
				{
					addressTable[UUIDNtoH(reinterpret_cast<Packet::RoutingPacket*>(pp)->address)] = ntohl(reinterpret_cast<Packet::RoutingPacket*>(pp)->hop);
					delete[] packetBuffer;
					break;
				}
				case Packet::PRT_TRANSMITION_ICMP:
				{
					auto ddd = reinterpret_cast<Packet::ICMPPacket*>(pp)->from;
					auto dd = UUIDNtoH(reinterpret_cast<Packet::ICMPPacket*>(pp)->from);
					addressTable.erase(dd);
				}
				default:
				{
					output.push(reinterpret_cast<Packet::TransmitionPacket*>(packetBuffer));
				}
				}
				length = 0;
			}
			}
		}

		if (networkEvents.lNetworkEvents & FD_CLOSE) {
			run = false;
			break;
		}

		endRecv:
		if (!input.empty()) {
			TransmitionPacket* packet;
			if (input.try_pop(packet)) {
				if (ntohs(packet->SubType) == Packet::PRT_INNER_FETCH_ADDR)
				{
					std::string address;
					char tb[40];
					for (auto ad : addressTable) {
						char* add;
						UuidToStringA(&ad.first, (RPC_CSTR*)&add);
						address.append(add);
						if (ad.first == myAddress)
							sprintf(tb, " Loopback\n");
						else
							sprintf(tb, " hop: %d\n", ad.second);
						address.append(tb);
					}
					auto opack = new BYTE[sizeof(Packet::PacketFrame) + address.size()+1];
					Packet::PacketFrame frame(address.size(), htons(Packet::PRT_INNER_FETCH_ADDR));
					memcpy(opack, &frame, sizeof(Packet::PacketFrame));
					memcpy(opack + sizeof(Packet::PacketFrame), address.c_str(), address.size());
					opack[sizeof(Packet::PacketFrame) + address.size()] = 0;
					output.push(reinterpret_cast<Packet::TransmitionPacket*>(opack));
				}
				else {
					packet->from = UUIDHtoN(myAddress);
					if (!SendCompletely(link, reinterpret_cast<char*>(packet), sizeof(Packet::PacketFrame) + ntohl(packet->length))) {
						run = false;
					}
				}
				delete packet;
			}
		}

		if (counter < GetTickCount64()) {
			counter = GetTickCount64() + 5000;
			auto prp = Packet::Encode( Packet::Generate<Packet::RoutingPacket>(htonl(0), UUIDHtoN(myAddress)));
			if (!SendCompletely(link, (char*)prp, sizeof(Packet::RoutingPacket))) {
				run = false;
			}
			delete prp;
		}
	}
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