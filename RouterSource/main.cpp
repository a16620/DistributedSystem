#include "Communicator.h"
#include <thread>
#include <iostream>
#pragma warning(disable: 4996)
using std::thread;

int main() {
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	Communicator com;
	ULONG target = INADDR_NONE;
	std::string cmd;
	u_short port, tport;
	std::cout << "> ";
	std::cin >> port >> tport >> cmd;
	if (cmd == "nonaddr") {
		std::cout << "start without connection...";
	}
	else if (cmd == "loopback" || cmd == "localhost")
		target = INADDR_LOOPBACK;
	else
		target = inet_addr(cmd.c_str());
	com.Start(target, tport, port);
	thread tI([&] { com.Receive(); });
	thread tO([&] { com.Respond(); });
	std::cout << "started" << std::endl;
	do {
		std::cin >> cmd;
	} while (cmd.compare("quit") != 0);
	com.Stop();
	tI.join();
	tO.join();
	com.Release();
	WSACleanup();

	return 0;
}