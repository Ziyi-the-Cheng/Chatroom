#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <conio.h>

#pragma comment(lib, "ws2_32.lib")

//real one
//////////////////////////////////////////
#define DEFAULT_BUFFER_SIZE 1024

std::atomic<bool> close = false;


SOCKET client_socket = INVALID_SOCKET;


void SendMessageToServer(SOCKET client_socket, std::string message) {
    if (send(client_socket, message.c_str(), static_cast<int>(message.size()), 0) == SOCKET_ERROR) {
        std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
    }
}

void SendPrivateMessageToServer(SOCKET client_socket, std::string chatUser, std::string message) {
    chatUser = "#" + chatUser;
    if (send(client_socket, chatUser.c_str(), static_cast<int>(chatUser.size()), 0) == SOCKET_ERROR) {
        std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
    }
    send(client_socket, message.c_str(), static_cast<int>(message.size()), 0) == SOCKET_ERROR;
}

void Receive(SOCKET client_socket, std::string& message) {
    int count = 0;
    while (!close) {
        // Receive the reversed sentence from the server
        char buffer[DEFAULT_BUFFER_SIZE] = { 0 };
        int bytes_received = recv(client_socket, buffer, DEFAULT_BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            message = buffer;
            buffer[bytes_received] = '\0'; // Null-terminate the received data
            std::cout << "Received(" << count++ << "): " << buffer << std::endl;
        }
        else if (bytes_received == 0) {
            std::cout << "Connection closed by server." << std::endl;
        }
        else if (close) {
            std::cout << "Terminating connection\n";
        }
        else {
            std::cerr << "Receive failed with error: " << WSAGetLastError() << std::endl;
        }
        if (strcmp(buffer, "!bye") == 0) {
            close = true;
        }
    }
}

void client(std::string& message) {

    const char* host = "127.0.0.1"; // Server IP address
    unsigned int port = 65432;

    // Initialize WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return;
    }

    // Create a socket
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return;
    }

    // Resolve the server address and port
    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    //server_address.sin_port = htons(std::stoi(port));
    server_address.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &server_address.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    // Connect to the server
    if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Connection failed with error: " << WSAGetLastError() << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    std::cout << "Connected to the server." << std::endl;

    //  Send(client_socket);
     // Receive(client_socket);
    //std::thread t1 = std::thread(Send, client_socket);
    //std::thread t2 = std::thread(Receive, client_socket, std::ref(message));
    Receive(client_socket, message);
    //t1.join();
    //t2.join();

    WSACleanup();
}