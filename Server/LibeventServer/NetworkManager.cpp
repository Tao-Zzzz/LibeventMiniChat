#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "NetworkManager.h"


#include "Logger.h"
#include "Session.h"
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <stdexcept>


NetworkManager::NetworkManager(int port) : port_(port) {
    if (port_ <= 0 || port_ > 65535) {
        throw std::invalid_argument("Invalid port number");
    }

    // windows初始化Winsock
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
#endif

    // 创建 libevent 事件循环基础对象
    base_ = event_base_new();
    if (!base_) {
        throw std::runtime_error("Failed to create event base");
    }

    // 配置监听地址和端口
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port_);

    // 创建监听器并绑定到端口
    listener_ = evconnlistener_new_bind(
        base_,                // baseEvent
        AcceptConn,           // 新连接回调函数
        this,                 // 传递 this 指针到回调
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, // 选项events：释放时关闭 socket，允许端口复用
        -1,                   // 默认 backlog
        (sockaddr*)&sin,      // 绑定地址
        sizeof(sin)
    );

    if (!listener_) {
        event_base_free(base_); // 失败时释放资源
        throw std::runtime_error("Failed to create listener");
    }

    // 记录服务器启动日志
    Logger::Info("Server listening on port {}", port_);
}

NetworkManager::~NetworkManager() {
    if (listener_) {
        evconnlistener_free(listener_);
    }
    if (base_) {
        event_base_free(base_);
    }
    // windows还要清理winsock
#ifdef _WIN32
    WSACleanup();
#endif
}

bool NetworkManager::Run() {
    if (event_base_dispatch(base_) == -1) {
        Logger::Error("Event loop failed");
        return false;
    }
    return true;
}

void NetworkManager::AcceptConn(evconnlistener*, evutil_socket_t fd, sockaddr*, int, void* arg) {
    auto* self = static_cast<NetworkManager*>(arg);
    // 为新的客户端 socket 创建 bufferevent
    auto* bev = bufferevent_socket_new(self->base_, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        Logger::Error("Failed to create bufferevent for fd={}", fd);
        return;
    }

    auto session = std::make_shared<Session>(bev);
    Session::AddSession(bev, session);
    bufferevent_setcb(bev, ReadCB, nullptr, EventCB, self);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
    Logger::Info("New connection accepted (fd={})", fd);
}

void NetworkManager::ReadCB(bufferevent* bev, void* arg) {
    auto session = Session::GetSession(bev);
    if (session) {
        session->OnRead();
    }
}

void NetworkManager::EventCB(bufferevent* bev, short events, void*) {
    auto session = Session::GetSession(bev);
    if (session) {
        session->OnEvent(events);
    }
}