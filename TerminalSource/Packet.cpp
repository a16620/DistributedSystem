#include "Packet.h"

Packet::PacketFrame* Packet::Encode(PacketFrame* packet)
{
	packet->length = htonl(packet->length);
	packet->SubType = htons(packet->SubType);
	return packet;
}