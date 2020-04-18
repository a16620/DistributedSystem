#pragma once
#include "def.h"
#include "Router.h"

namespace Packet {
	enum {
		PRT_INTER_ROUTING,
		PRT_TRANSMITION_DATA,
		PRT_TRANSMITION_ICMP
	};

	struct PacketFrame {
		u_long length;
		u_short SubType, ext;

		PacketFrame(u_long length, u_short subType) : length(length), SubType(subType) {}
	};

	struct TransmitionPacket : PacketFrame {
		Address from, to;
		TransmitionPacket(u_long length, u_short subType, Address from, Address to) : PacketFrame(length, subType), from(from), to(to) {}
	};

	struct RoutingPacket : PacketFrame {
		Address address;
		size_t hop;

		RoutingPacket(size_t hop, Address address) : PacketFrame(sizeof(Address) + sizeof(size_t), PRT_INTER_ROUTING) {
			this->hop = hop;
			this->address = address;
		}
	};

	struct DataPacket : TransmitionPacket {
		DataPacket(Address from, Address to) : TransmitionPacket(0, PRT_TRANSMITION_DATA, from, to) {}
	};

	struct ICMPPacket : TransmitionPacket {
		ULONG flag;
		ICMPPacket(Address from, Address to, ULONG flag) : TransmitionPacket(sizeof(ULONG)+2*sizeof(Address), PRT_TRANSMITION_ICMP, from, to), flag(flag) {}
	};

	PacketFrame* Encode(PacketFrame* packet);
	template <class Base, class ...T>
	PacketFrame* Generate(T&& ...ty)
	{
		Base base(ty...);
		auto p = new BYTE[sizeof(Base)];
		memcpy(p, &base, sizeof(Base));
		return reinterpret_cast<PacketFrame*>(p);
	}
}