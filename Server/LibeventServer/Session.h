#pragma once
#include <event2/bufferevent.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>

class Session {
public:
    Session(bufferevent* bev);
    ~Session();
    void OnRead();
    void OnEvent(short events);
    static void Broadcast(const void* data, size_t len, bufferevent* exclude_bev);
    static void AddSession(bufferevent* bev, std::shared_ptr<Session> session);
    static void RemoveSession(bufferevent* bev);
    static std::shared_ptr<Session> GetSession(bufferevent* bev);

private:
    bufferevent* bev_;
    std::string readBuffer_;
    std::string name_;
    static std::unordered_map<bufferevent*, std::shared_ptr<Session>> sessionMap_;
    static std::mutex sessionMapMutex_;
};