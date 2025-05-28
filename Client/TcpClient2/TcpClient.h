#pragma once
#include <string>
#include <winsock2.h>

class TcpClient {
public:
    TcpClient(const std::string& host, int port);
    ~TcpClient();
    bool Connect();
    void Run();

private:
    bool SetNonBlocking(SOCKET sock);
    void SendMessage(const std::string& message);
    bool ReceiveMessage(std::string& message);
    void PrintInputPrompt(const std::string& current_input);

    std::string host_;
    int port_;
    SOCKET sock_;
    std::string read_buffer_; // ���ջ�����
    std::string input_buffer_; // �û����뻺����
};