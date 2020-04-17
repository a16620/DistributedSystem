#include "Communicator.h"
#include <thread>
using std::thread;

int main() {
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	Communicator com;
	com.Start(INADDR_NONE);
	thread tI([&] { com.Receive(); });
	thread tO([&] { com.Respond(); });

	while (getchar() != '\n');
	com.Stop();
	tI.join();
	tO.join();
	com.Release();
	WSACleanup();

	return 0;
}