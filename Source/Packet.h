#pragma once
#include "def.h"
#include "Router.h"

namespace Packet {
	enum {
		PRT_ROUTING,
		PRT_DATA,
		PRT_ICMP,
		PRT_INNER_REMOVE
	};

	struct TransmitionPacket {
		long length;
		short SubType;

		TransmitionPacket(long length, short subType) : length(length), SubType(subType) {}
	};

	struct TPacket : TransmitionPacket {
		Address from, to;
		TPacket(long length, short subType, Address from, Address to) : TransmitionPacket(length, subType), from(from), to(to) {}
	};

	struct RoutingPacket : TPacket {
		size_t hop;
		Address address;

		RoutingPacket(size_t hop, Address address, Address to) : TPacket(sizeof(Address) + sizeof(size_t), PRT_ROUTING, 0, to) {
			this->hop = hop;
			this->address = address;
		}
	};

	struct DataPacket : TPacket {
		char Data[4];

		DataPacket(Address from, Address to, char a) : TPacket(4 + sizeof(Address) * 2, PRT_DATA, from, to) {
			for (int i = 0; i < 4; i++)
				Data[i] = a + i;
		}
	};

	struct ICMPPacket : TPacket {
		ULONG flag;
		ICMPPacket(Address from, Address to, ULONG flag) : TPacket(sizeof(Address) + sizeof(ULONG), PRT_ICMP, from, to), flag(flag) {}
	};
}