#include "TcpClient.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

TcpClient::TcpClient(const std::string& host, int port)
    : host_(host), port_(port), sock_(INVALID_SOCKET) {}

TcpClient::~TcpClient() {
    if (sock_ != INVALID_SOCKET)
        closesocket(sock_);
    WSACleanup();
}

bool TcpClient::Connect() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        std::cerr << "WSAStartup failed\n";
        return false;
    }

    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_);
    inet_pton(AF_INET, host_.c_str(), &serverAddr.sin_addr);

    if (connect(sock_, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed\n";
        return false;
    }

    std::cout << "Connected to server.\n";
    return true;
}

void TcpClient::Run() {
    char buffer[512];
    std::string line;

    while (true) {
        std::cout << "Input: ";
        std::getline(std::cin, line);

        if (line == "exit") break;

        send(sock_, line.c_str(), static_cast<int>(line.size()), 0);

        int len = recv(sock_, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) break;

        buffer[len] = '\0';
        std::cout << "Server echoed: " << buffer << std::endl;
    }
}
