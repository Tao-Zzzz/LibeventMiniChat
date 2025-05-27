#include "Session.h"
#include "Logger.h"
#include <event2/buffer.h>

std::unordered_map<bufferevent*, std::shared_ptr<Session>> Session::sessionMap_;
std::mutex Session::sessionMapMutex_;

Session::Session(bufferevent* bev) : bev_(bev) {}

Session::~Session() {
    Logger::Info("Session destroyed");
}

void Session::OnRead() {
    auto input = bufferevent_get_input(bev_);
    size_t len = evbuffer_get_length(input);
    if (len == 0) {
        return;
    }

    std::vector<char> buffer(len);
    size_t n = evbuffer_remove(input, buffer.data(), len);
    readBuffer_.append(buffer.data(), n);
    // Logger::Info("Received {} bytes", n);

    evutil_socket_t fd = bufferevent_getfd(bev_);
    // 回显数据
    bufferevent_write(bev_, buffer.data(), n);
    Logger::Info("Received {} from {}", buffer.data(), fd);
}

void Session::OnEvent(short events) {
    // 处理断开或错误事件
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        Logger::Info("Client disconnected or error occurred (events={})", events);
        RemoveSession(bev_);
        bufferevent_free(bev_);
    }
}

void Session::AddSession(bufferevent* bev, std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(sessionMapMutex_);
    sessionMap_[bev] = session;
}

void Session::RemoveSession(bufferevent* bev) {
    std::lock_guard<std::mutex> lock(sessionMapMutex_);
    sessionMap_.erase(bev);
}

std::shared_ptr<Session> Session::GetSession(bufferevent* bev) {
    std::lock_guard<std::mutex> lock(sessionMapMutex_);
    auto it = sessionMap_.find(bev);
    return (it != sessionMap_.end()) ? it->second : nullptr;
}