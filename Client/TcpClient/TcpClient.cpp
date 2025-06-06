#include "TcpClient.h"
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

TcpClient::TcpClient(const std::string& host, int port)
    : host_(host), port_(port), sock_(INVALID_SOCKET) {}

TcpClient::~TcpClient() {
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
    }
    WSACleanup();
}

bool TcpClient::SetNonBlocking(SOCKET sock) {
    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
        std::cerr << "Set non-blocking failed: " << WSAGetLastError() << std::endl;
        return false;
    }
    return true;
}

bool TcpClient::Connect() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        return false;
    }

    // IP4, 流式SOCKET, TCP协议
    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return false;
    }

    if (!SetNonBlocking(sock_)) {
        closesocket(sock_);
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    // 网络字节序,大端
    serverAddr.sin_port = htons(port_);
    if (inet_pton(AF_INET, host_.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address: " << host_ << std::endl;
        closesocket(sock_);
        WSACleanup();
        return false;
    }

    if (connect(sock_, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            std::cerr << "Connection failed: " << error << std::endl;
            closesocket(sock_);
            WSACleanup();
            return false;
        }
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(sock_, &write_fds);
        timeval timeout = { 2, 0 }; // 设置两秒超时

        // select监控是否可写,当可写时才真正进行了连接
        if (select(static_cast<int>(sock_) + 1, nullptr, &write_fds, nullptr, &timeout) <= 0) {
            std::cerr << "Connection timeout or error: " << WSAGetLastError() << std::endl;
            closesocket(sock_);
            WSACleanup();
            return false;
        }
    }

    std::cout << "Connected to server " << host_ << ":" << port_ << std::endl;
    return true;
}

// 构造带长度前缀的消息，适配服务器的协议
void TcpClient::SendMessage(const std::string& message) {
    uint32_t msg_len = static_cast<uint32_t>(message.size());
    uint32_t net_len = htonl(msg_len);

    std::vector<char> data;
    // 插入头部 和 body
    data.insert(data.end(), (char*)&net_len, (char*)&net_len + sizeof(uint32_t));
    data.insert(data.end(), message.begin(), message.end());

    int sent = send(sock_, data.data(), static_cast<int>(data.size()), 0);
    if (sent == SOCKET_ERROR) {
        // 非阻塞可能返回数据未完全发送
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
        }
    }
    else {
        // 打印到新行，避免干扰当前输入
        std::cout << "\nSent: " << message << std::endl;
        PrintInputPrompt(input_buffer_);
    }
}

bool TcpClient::ReceiveMessage(std::string& message) {
    std::vector<char> buffer(4096);
    int len = recv(sock_, buffer.data(), buffer.size(), 0);

    if (len == SOCKET_ERROR) {
        int error = WSAGetLastError();
        // 无数据可读
        if (error == WSAEWOULDBLOCK) {
            return false;
        }
        std::cerr << "Receive failed: " << error << std::endl;
        return false;
    }
    if (len == 0) {
        std::cerr << "Connection closed by server" << std::endl;
        return false;
    }

    // 将读取到的数据存到读缓冲区
    read_buffer_.append(buffer.data(), len);
    // 如果数据长度大于头部
    while (read_buffer_.size() >= sizeof(uint32_t)) {
        uint32_t msg_len;
        memcpy(&msg_len, read_buffer_.data(), sizeof(uint32_t));
        // 转换为主机字节序
        msg_len = ntohl(msg_len);

        // 如果不够完整长度,那退出循环,等待下一次读
        if (read_buffer_.size() < sizeof(uint32_t) + msg_len) {
            break;
        }

        message = std::string(read_buffer_.data() + sizeof(uint32_t), msg_len);
        read_buffer_.erase(0, sizeof(uint32_t) + msg_len);
        return true;
    }
    return false;
}

void TcpClient::PrintInputPrompt(const std::string& current_input) {
    // 获取标准输出句柄和当前控制台信息（光标位置等）
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);

    // 只清空当前输入行
    COORD pos = { 0, csbi.dwCursorPosition.Y };
    SetConsoleCursorPosition(hConsole, pos);
    DWORD written;
    FillConsoleOutputCharacter(hConsole, ' ', csbi.dwSize.X, pos, &written);

    // 打印提示和当前输入
    std::cout << "Input: " << current_input << std::flush;
}

void TcpClient::Run() {
    if (sock_ == INVALID_SOCKET) {
        std::cerr << "Not connected to server\n";
        return;
    }

    // 用于 select 的文件描述符集
    fd_set read_fds;
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    input_buffer_.clear();
    PrintInputPrompt(input_buffer_);

    while (true) {
        // 初始化 select 文件描述符集
        FD_ZERO(&read_fds);
        FD_SET(sock_, &read_fds);

        // 检查标准输入
        INPUT_RECORD ir;
        DWORD events;
        PeekConsoleInput(hStdin, &ir, 1, &events);
        bool has_input = (events > 0);

        // 设置较短的超时
        timeval timeout = { 0, 10000 }; // 50ms 超时

        // select 监控
        int result = select(static_cast<int>(sock_) + 1, &read_fds, nullptr, nullptr, &timeout);
        if (result == SOCKET_ERROR) {
            std::cerr << "Select failed: " << WSAGetLastError() << std::endl;
            break;
        }

        // 处理网络消息
        if (FD_ISSET(sock_, &read_fds)) {
            std::string message;
            // 接收消息
            if (ReceiveMessage(message)) {
                std::cout << "\nReceived: " << message << std::endl;
                PrintInputPrompt(input_buffer_);
            }
            else if (read_buffer_.empty()) {
                std::cout << "\nConnection closed by server" << std::endl;
                break;
            }
        }

        // 处理用户输入
        if (has_input) {
            ReadConsoleInput(hStdin, &ir, 1, &events);
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                char c = ir.Event.KeyEvent.uChar.AsciiChar;
                if (c == '\r') { // 回车
                    if (input_buffer_ == "exit") {
                        break;
                    }
                    if (!input_buffer_.empty()) {
                        SendMessage(input_buffer_);
                        input_buffer_.clear();
                    }
                    PrintInputPrompt(input_buffer_);
                }
                else if (c == '\b' && !input_buffer_.empty()) { // 退格
                    input_buffer_.pop_back();
                    PrintInputPrompt(input_buffer_);
                }
                else if (c >= 32 && c <= 126) { // 可打印字符
                    input_buffer_ += c;
                    PrintInputPrompt(input_buffer_);
                }
            }
        }
    }

    // 清理
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
}