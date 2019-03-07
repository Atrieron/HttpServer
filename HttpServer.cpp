// HttpServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <winsock2.h>
#include <ws2def.h>
#include <WS2tcpip.h>
#include <string>
#include <thread>
#include <future>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

#define HTPP_PORT "80"
#define HTPPS_PORT "443"
#define DEFAULT_BUFLEN 512

void doServeClient(SOCKET ClientSocket) {
	char recvbuf[DEFAULT_BUFLEN];
	int iResult, iSendResult;
	int recvbuflen = DEFAULT_BUFLEN;

	// Receive until the peer shuts down the connection
	do {

		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			printf("Bytes received: %d\n", iResult);

			std::string recievedString = std::string::basic_string(recvbuf, recvbuflen);

			std::cout << recievedString;

			// Echo the buffer back to the sender
			//iSendResult = send(ClientSocket, recvbuf, iResult, 0);
			//if (iSendResult == SOCKET_ERROR) {
			//	printf("send failed: %d\n", WSAGetLastError());
			//	closesocket(ClientSocket);
			//	WSACleanup();
			//	return;
			//}
			//printf("Bytes sent: %d\n", iSendResult);
		}
		else if (iResult == 0)
			printf("Connection closing...\n");
		else {
			printf("recv failed: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			WSACleanup();
			return;
		}

	} while (iResult > 0);

	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return;
	}

	closesocket(ClientSocket);
	WSACleanup();
}

class ServerListner
{
private:
	std::promise<void> exitSignal;
	std::future<void> futureObj;
	SOCKET ListenSocket;

	void run() {
		while (futureObj.wait_for(std::chrono::milliseconds(1)) == std::future_status::timeout)
		{
			if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
				printf("Listen failed with error: %ld\n", WSAGetLastError());
				closesocket(ListenSocket);
				WSACleanup();
				return;
			}

			SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
			if (ClientSocket == INVALID_SOCKET) {
				printf("accept failed: %d\n", WSAGetLastError());
				closesocket(ListenSocket);
				WSACleanup();
				return;
			};

			std::thread clientServingThread(doServeClient, ClientSocket);
		}

		closesocket(ListenSocket);
		WSACleanup();
	}
public:
	ServerListner(SOCKET ListenSocket) {
		this->ListenSocket = ListenSocket;
		futureObj = exitSignal.get_future();
	}

	void operator()() {
		run();
	}

	void Stop() {
		exitSignal.set_value();
	}
};

int main()
{
	WSADATA wsaData;

	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}

	struct addrinfo *result = NULL, *ptr = NULL, hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the local address and port to be used by the server
	iResult = getaddrinfo(NULL, HTPP_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	SOCKET ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

	if (ListenSocket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	ServerListner* serverListener = new ServerListner(ListenSocket);
	
	std::thread serverListeningThread([&]()
	{
		(*serverListener)();
	});

	bool doContinue = true;
	while (doContinue)
	{
		std::string consoleCommand;
		std::cin >> consoleCommand;

		
		if (!consoleCommand.compare("exit")) {
			doContinue = false;
		}
	}

	serverListener->Stop();
	serverListeningThread.join();

	return 0;
}