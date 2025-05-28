#include "TcpClient.h"

int main() {
    TcpClient client("127.0.0.1", 9999);
    if (client.Connect()) {
        client.Run();
    }
    return 0;
}
