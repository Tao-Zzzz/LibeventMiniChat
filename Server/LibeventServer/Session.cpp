#include "Session.h"
#include "Logger.h"
#include <event2/buffer.h>
#include <string>

std::unordered_map<bufferevent*, std::shared_ptr<Session>> Session::sessionMap_;
std::mutex Session::sessionMapMutex_;

// 静态计数器，用于生成唯一名称
static int client_counter = 0;

Session::Session(bufferevent* bev) : bev_(bev)
    ,name_("client" + std::to_string(++client_counter)) {}

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

    while (readBuffer_.size() >= sizeof(uint32_t)) {
        // 解析头部
        uint32_t msg_len;
        memcpy(&msg_len, readBuffer_.data(), sizeof(uint32_t));
        msg_len = ntohl(msg_len);

        if (readBuffer_.size() < sizeof(uint32_t) + msg_len) {
            break;
        }

        // 提取原始消息
        std::string message(readBuffer_.data() + sizeof(uint32_t), msg_len);
        readBuffer_.erase(0, sizeof(uint32_t) + msg_len);

        // 记录接收到的消息
        evutil_socket_t fd = bufferevent_getfd(bev_);
        Logger::Info("Received [{}] from {}", message, fd);

        // 构造新消息：from: [name]: [message]
        std::string broadcast_message = "from: " + name_ + ": " + message;
        uint32_t broadcast_len = static_cast<uint32_t>(broadcast_message.size());
        uint32_t net_len = htonl(broadcast_len);

        // 构造广播数据
        std::vector<char> broadcast_data;
        broadcast_data.insert(broadcast_data.end(), (char*)&net_len, (char*)&net_len + sizeof(uint32_t));
        broadcast_data.insert(broadcast_data.end(), broadcast_message.begin(), broadcast_message.end());

        // 广播消息
        Broadcast(broadcast_data.data(), broadcast_data.size(), bev_);
    }
}

void Session::OnEvent(short events) {
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        evutil_socket_t fd = bufferevent_getfd(bev_);
        Logger::Info("Client disconnected or error occurred (fd={}, events={})", fd, events);
        RemoveSession(bev_);
        bufferevent_free(bev_);
    }
}

void Session::Broadcast(const void* data, size_t len, bufferevent* exclude_bev) {
    std::lock_guard<std::mutex> lock(sessionMapMutex_);
    auto it = sessionMap_.begin();
    while (it != sessionMap_.end()) {
        auto* bev = it->first;
        if (bev == exclude_bev) {
            ++it;
            continue;
        }
        if (bufferevent_getfd(bev) != -1) {
            int ret = bufferevent_write(bev, data, len);
            if (ret != 0) {
                Logger::Error("Failed to write to client (fd={}) in broadcast", bufferevent_getfd(bev));
                auto to_remove = it++;
                RemoveSession(to_remove->first);
                bufferevent_free(to_remove->first);
            }
            else {
                ++it;
            }
        }
        else {
            auto to_remove = it++;
            RemoveSession(to_remove->first);
            bufferevent_free(to_remove->first);
        }
    }
}

void Session::AddSession(bufferevent* bev, std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(sessionMapMutex_);
    sessionMap_[bev] = session;
}

void Session::RemoveSession(bufferevent* bev) {
    std::lock_guard<std::mutex> lock(sessionMapMutex_);
    auto it = sessionMap_.find(bev);
    if (it != sessionMap_.end()) {
        Logger::Info("Removing session for fd={}", bufferevent_getfd(bev));
        sessionMap_.erase(it);
    }
}

std::shared_ptr<Session> Session::GetSession(bufferevent* bev) {
    std::lock_guard<std::mutex> lock(sessionMapMutex_);
    auto it = sessionMap_.find(bev);
    return (it != sessionMap_.end()) ? it->second : nullptr;
}