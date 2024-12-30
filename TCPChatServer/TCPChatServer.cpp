/*
작성자:	202413243 김준겸
작성일:	2024_12_07

[게임네트워크기초 기말평가 과제] 채팅 프로그램 구현하기
TCP 채팅 서버 프로그램 
*/

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <vector>
#include <thread>
#include <iostream>

#define SERVER_PORT	9000
#define BUFFER_SIZE	1024		

std::vector<SOCKET> clientSockets;	
CRITICAL_SECTION cs;

// Send a message to all clients
void BroadcastMessage(const char* message, int length, SOCKET senderSocket) {
	EnterCriticalSection(&cs);
	for (auto socket : clientSockets) {
		if (socket != senderSocket) {
			send(socket, message, length, 0);
		}
	}
	LeaveCriticalSection(&cs);
}

DWORD WINAPI ClientThread(LPVOID arg) {
	int retval;
	SOCKET clientSocket = (SOCKET)arg;
	SOCKADDR_IN clientAddress;
	int addrlen = sizeof(clientAddress);
	getpeername(clientSocket, (SOCKADDR*)&clientAddress, &addrlen);
	char buffer[BUFFER_SIZE + 1];

	while (true) {
		// Receive a data
		retval = recv(clientSocket, buffer, BUFFER_SIZE, 0);
		if (retval == SOCKET_ERROR) {
			std::cout << "Receive failed.\n";
			break;
		}
		else if (retval == 0) {
			char addr[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &clientAddress.sin_addr, addr, sizeof(addr));
			std::cout << "[SERVER] Terminate client connections: IP address = " << addr
				<< ", Port number = " << ntohs(clientAddress.sin_port) << '\n';
			break;
		}

		buffer[retval] = '\0';

		// Send a data
		BroadcastMessage(buffer, retval, clientSocket);
	}

	// Remove from client socket list
	EnterCriticalSection(&cs);
	auto it = std::find(clientSockets.begin(), clientSockets.end(), clientSocket);
	if (it != clientSockets.end()) {
		clientSockets.erase(it);
	}
	LeaveCriticalSection(&cs);

	// Close the client socket
	closesocket(clientSocket);

	return 0;
}

int main(int argc, char* argv[]) {
	// Initialize WinSock
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cout << "WSAStartup failed.\n";
		return 1;
	}

	// Initialize critical section 
	InitializeCriticalSection(&cs);

	// Create a socket 
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket == INVALID_SOCKET) {
		std::cout << "Create socket failed.\n";
		return 1; 
	}

	// Set socket options
	DWORD optionValue = TRUE;
	if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optionValue, sizeof(optionValue)) == SOCKET_ERROR) {
		std::cout << "Set SO_REUSEADDR option failed.\n";
		return 1;
	}
	if (setsockopt(listenSocket, SOL_SOCKET, SO_KEEPALIVE, (const char*)&optionValue, sizeof(optionValue)) == SOCKET_ERROR) {
		std::cout << "Set SO_KEEPALIVE option failed.\n";
		return 1;
	}

	// Setting socket address information 
	SOCKADDR_IN serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(SERVER_PORT);
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

	// Socket Binding
	if (bind(listenSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
		std::cout << "Bind the socket failed.\n";
		return 1;
	}
		
	// Socket Listening
	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
		std::cout << "Socket listen failed.\n";
		return 1;
	}
		
	std::cout << " * Started the TCP chat server on port " << ntohs(serverAddress.sin_port) << ".\n";

	SOCKADDR_IN clientAddress;
	while (TRUE) {
		// Client accept 
		int addrlen = sizeof(clientAddress);
		SOCKET clientSocket = accept(listenSocket, (SOCKADDR*)&clientAddress, &addrlen);
		if (clientSocket == INVALID_SOCKET) {
			std::cout << "Client accept failed.\n";
			break;
		}

		// Print the client information.
		char addr[INET_ADDRSTRLEN]; 
		inet_ntop(AF_INET, &clientAddress.sin_addr, addr, sizeof(addr));
		std::cout << "[SERVER] A client is connected: IP address = " << addr
			<< ", Port number = " << ntohs(clientAddress.sin_port) << '\n';

		// Add to client socket list
		EnterCriticalSection(&cs);
		clientSockets.push_back(clientSocket);
		LeaveCriticalSection(&cs);

		// Create a thread. 
		HANDLE hThread = CreateThread(NULL, 0, ClientThread, (LPVOID)clientSocket, 0, NULL);
		if (hThread == NULL) closesocket(clientSocket);
		else CloseHandle(hThread);
	}

	// Close the listen socket
	closesocket(listenSocket);

	// Delete the critical section
	DeleteCriticalSection(&cs);

	// Close the WinSock 
	WSACleanup();

	return 0;
}