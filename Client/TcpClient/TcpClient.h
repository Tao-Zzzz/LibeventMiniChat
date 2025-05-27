#pragma once
#include <string>
#include <WinSock2.h> // Windows Socket 主头文件
#include <WS2tcpip.h> // 可选，用于高级功能（如getaddrinfo）
#include <Windows.h>  // Windows API 头文件

#pragma comment(lib, "Ws2_32.lib")

class TcpClient {
public:
    TcpClient(const std::string& host, int port);
    ~TcpClient();
    bool Connect();
    void Run();

private:
    std::string host_;
    int port_;
    SOCKET sock_;
};
    