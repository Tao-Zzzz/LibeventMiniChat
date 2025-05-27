#pragma once
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>

class NetworkManager {
public:
    explicit NetworkManager(int port);
    ~NetworkManager();
    bool Run();

private:
    static void AcceptConn(evconnlistener*, evutil_socket_t, sockaddr*, int, void*);
    static void ReadCB(bufferevent* bev, void* arg);
    static void EventCB(bufferevent* bev, short events, void* arg);

    event_base* base_;
    evconnlistener* listener_;
    int port_;
};