#pragma once
#include <string>
#include <WinSock2.h> // Windows Socket ��ͷ�ļ�
#include <WS2tcpip.h> // ��ѡ�����ڸ߼����ܣ���getaddrinfo��
#include <Windows.h>  // Windows API ͷ�ļ�

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
    