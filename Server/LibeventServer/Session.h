#pragma once
#include <event2/bufferevent.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>

class Session {
public:
    explicit Session(bufferevent* bev);
    ~Session();

    void OnRead();
    void OnEvent(short events);

    static void AddSession(bufferevent* bev, std::shared_ptr<Session> session);
    static void RemoveSession(bufferevent* bev);
    static std::shared_ptr<Session> GetSession(bufferevent* bev);

private:
    bufferevent* bev_;
    std::string readBuffer_;
    static std::unordered_map<bufferevent*, std::shared_ptr<Session>> sessionMap_;
    static std::mutex sessionMapMutex_;
};