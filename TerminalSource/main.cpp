#include <concurrent_queue.h>
#include <iostream>
#include <thread>
#include <vector>
#include "ClientCommunicator.h"
#include "Packet.h"
#pragma warning(disable:4996)
using namespace std;

std::string DescriptPacket(Packet::TransmitionPacket* packet);
Packet::TransmitionPacket* InterpretPacket(string command);
std::vector<string> Split(const string& origin, char d);

int main() {
	char buffer[100];
	int i = 0;
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	ClientCommunicator com;
	com.Start(INADDR_LOOPBACK);
	thread cTh([&] { com.Task(); });

	while (com.run) {
		try {
			char c = fgetc(stdin);
			if (c == '\n')
			{
				buffer[i] = '\n';
				buffer[i + 1] = 0;
				i = 0;
				auto pp = InterpretPacket(buffer);
				if (pp != nullptr)
					com.input.push(reinterpret_cast<Packet::TransmitionPacket*>(Packet::Encode(pp)));
				else
					printf("exp\n");
				while (!com.output.empty())
				{
					Packet::TransmitionPacket* packet;
					if (com.output.try_pop(packet)) {
						fprintf(stdout, "%s\n", DescriptPacket(packet).c_str());
						delete[] (BYTE*)packet;
					}
				}
			}
			else {
				buffer[i++] = c;
			}
		}
		catch (...){
			com.Stop();
			break;
		}
	}
	cTh.join();
	com.Release();
	WSACleanup();
	return 0;
}

//To User
string DescriptPacket(Packet::TransmitionPacket* packet)
{
	char result[128] = "";
	switch (ntohs(packet->SubType)) {
	case Packet::PRT_TRANSMITION_DATA:
	{
		for (int i = 0; i < packet->length + sizeof(Packet::PacketFrame); i++)
			printf("%d ", *((char*)packet + i));
		printf("\n");
		sprintf(result, "DATA from: %d data: %s", packet->from, ((char*)packet + sizeof(Packet::TransmitionPacket)));
		break;
	}
	case Packet::PRT_TRANSMITION_ICMP:
	{
		sprintf(result, "Message wasn't sent to: %d", packet->from);
		break;
	}
	}
	return string(result);
}

//To Communicator
Packet::TransmitionPacket* InterpretPacket(string command)
{
	
	if (true) {
		if (true) {
			auto packet = reinterpret_cast<Packet::DataPacket*>(new BYTE[sizeof(Packet::DataPacket)+4]);
			memcpy((BYTE*)packet + sizeof(Packet::DataPacket), "AAAA", 4);
			packet->SubType = Packet::PRT_TRANSMITION_DATA;
			packet->length = 4+sizeof(Packet::TransmitionPacket)-sizeof(Packet::PacketFrame);
			packet->from = 1;
			packet->to = 1;

			return packet;
		}
	}

	return nullptr;
}

std::vector<string> Split(const string& origin, char d) {
	std::vector<string> list;

	char buffer[100];
	int offset = 0;
	bool slicing = false;
	for (int i = 0; i < origin.length(); ++i) {
		if (!slicing && origin[i] != d) {
			slicing = true;
			i--;
			continue;
		}
		if (slicing && (origin[i] == d || origin[i] == '\0')) {
			size_t pLen = i - offset;
			
			list.push_back(origin.substr(offset, pLen));
			offset = i + 1;
			slicing = false;
		}
	}
	return list;
}