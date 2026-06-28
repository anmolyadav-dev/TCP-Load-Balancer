#include <iostream>
#include <vector>
#include <string>
#include <asio.hpp>

using asio::ip::tcp;

struct Backend { std::string host; uint16_t port; };
std::vector<Backend> backends;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <listen_port> <host:port> ...\n";
        return 1;
    }
    // basic setup
    std::cout << "Acceptor setup initialized..." << std::endl;
    return 0;
}
