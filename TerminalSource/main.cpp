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
				buffer[i] = 0;
				i = 0;
				if (strcmp(buffer, "") != 0)
				{
					auto pp = InterpretPacket(buffer);
					if (pp != nullptr)
						com.input.push(reinterpret_cast<Packet::TransmitionPacket*>(Packet::Encode(pp)));
					else
						printf("exp\n");
				}
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
		catch (std::exception e){
			fprintf(stdout, "err: %s\n", e.what());
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
		char* addr;
		UUID uad = UUIDNtoH(packet->from);
		UuidToStringA(&uad, (RPC_CSTR*)&addr);
		sprintf(result, "DATA from: %s data: %s", addr, ((char*)packet + sizeof(Packet::TransmitionPacket)));
		break;
	}
	case Packet::PRT_TRANSMITION_ICMP:
	{
		char* addr;
		UUID uad = UUIDNtoH(packet->from);
		UuidToStringA(&uad, (RPC_CSTR*)&addr);
		UuidToStringA(&uad, (RPC_CSTR*)&addr);
		sprintf(result, "Message wasn't sent to: %s", addr);
		break;
	}
	case Packet::PRT_INNER_FETCH_ADDR:
	{
		fwrite((char*)packet + sizeof(Packet::PacketFrame), 1, packet->length, stdout);
		strcpy(result, "fetch");
		break;
	}
	}
	return string(result);
}

//To Communicator
Packet::TransmitionPacket* InterpretPacket(string command)
{
	auto params = Split(command, ' ');
	
	if (params[0].compare("DATA") == 0) {
		if (params.size() >= 3) {
			UUID dst;
			if (UuidFromStringA((RPC_CSTR)(params[1].c_str()), &dst) == RPC_S_OK)
			{
				auto packet = reinterpret_cast<Packet::DataPacket*>(new BYTE[sizeof(Packet::DataPacket) + params[2].size()+1]);
				memcpy((BYTE*)packet + sizeof(Packet::DataPacket), params[2].c_str(), params[2].size() + 1);
				packet->SubType = Packet::PRT_TRANSMITION_DATA;
				packet->length = params[2].size() + 1 + sizeof(Packet::TransmitionPacket) - sizeof(Packet::PacketFrame);
				packet->to = UUIDHtoN(dst);

				return packet;
			}
		}
	}
	else if (params[0].compare("ADDR") == 0) {
		auto p =reinterpret_cast<Packet::PacketFrame*>(new BYTE[sizeof(Packet::PacketFrame)]);
		p->SubType = Packet::PRT_INNER_FETCH_ADDR;
		return reinterpret_cast<Packet::TransmitionPacket*>(p);
	}
	else if (params[0].compare("quit") == 0) {
		throw std::exception("quit was entered");
	}

	return nullptr;
}

std::vector<string> Split(const string& origin, char d) {
	std::vector<string> list;

	int offset = 0;
	bool slicing = false;
	for (int i = 0; i < origin.length(); ++i) {
		if (!slicing && origin[i] != d) {
			slicing = true;
			offset = i;
			i--;
			continue;
		}
		if (slicing && origin[i] == d) {
			size_t pLen = i - offset;
			
			list.push_back(origin.substr(offset, pLen));
			slicing = false;
		}
	}

	if (slicing) {
		list.push_back(origin.substr(offset, origin.length()-offset));
	}
	return list;
}