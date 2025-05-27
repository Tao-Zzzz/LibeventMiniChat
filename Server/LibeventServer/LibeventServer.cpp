#include "NetworkManager.h"
#include "Logger.h"

int main() {
    try {
        NetworkManager server(9999);
        server.Run();
    }
    catch (const std::exception& e) {
        Logger::Error("Failed to start server: {}", e.what());
        return 1;
    }
    return 0;
}