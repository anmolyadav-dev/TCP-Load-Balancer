/*
 * TCP Layer 4 Load Balancer — Async (Asio + epoll)
 *
 * Single-threaded, event-driven. Handles thousands of concurrent
 * connections without creating any threads per connection.
 * Uses Linux epoll under the hood via standalone Asio.
 *
 * Usage: ./lb <listen_port> <host:port> [host:port ...]
 * Example: ./lb 8080 127.0.0.1:8081 127.0.0.1:8082 127.0.0.1:8083
 */

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <array>
#include <csignal>
#include <asio.hpp>

using asio::ip::tcp;

// ── Backend config & round-robin ────────────────────────────

struct Backend { std::string host; uint16_t port; };

std::vector<Backend> backends;           // populated once at startup
std::atomic<int> next_backend{0};        // atomic for lock-free round-robin

Backend& pick_backend() {
    return backends[next_backend.fetch_add(1) % backends.size()];
}

// ── Session: one client ↔ backend connection pair ───────────
//
// Inheriting enable_shared_from_this ensures the Session stays
// alive while any async operation holds a shared_ptr to it.
// When all ops complete (or error), refcount hits 0, destructor
// runs, and both sockets are closed automatically — pure RAII.

class Session : public std::enable_shared_from_this<Session> {
    tcp::socket client_, backend_;
    std::array<char, 8192> cbuf_{}, bbuf_{};   // separate buffers per direction

public:
    Session(tcp::socket client, asio::io_context& ctx)
        : client_(std::move(client)), backend_(ctx) {}

    void start() {
        auto& b = pick_backend();
        auto self = shared_from_this();   // prevent destruction during async gap
        auto ep = tcp::endpoint(asio::ip::make_address(b.host), b.port);

        backend_.async_connect(ep,
            [this, self](std::error_code ec) {
                if (ec) return;           // Session destroyed → sockets closed
                relay(client_, backend_, cbuf_);   // client → backend
                relay(backend_, client_, bbuf_);   // backend → client
            });
    }

private:
    // Async relay: read from src → write to dst → repeat
    // Each call chains to the next via callbacks (no threads, no blocking)
    void relay(tcp::socket& src, tcp::socket& dst,
               std::array<char, 8192>& buf) {
        auto self = shared_from_this();

        src.async_read_some(asio::buffer(buf),
            [this, self, &src, &dst, &buf](std::error_code ec, size_t n) {
                if (ec) {
                    // Source closed/errored → tell dst "no more data"
                    std::error_code ign;
                    dst.shutdown(tcp::socket::shutdown_send, ign);
                    return;
                }
                // async_write guarantees ALL n bytes are written (unlike send)
                asio::async_write(dst, asio::buffer(buf.data(), n),
                    [this, self, &src, &dst, &buf](std::error_code ec, size_t) {
                        if (ec) return;        // write failed → stop relay
                        relay(src, dst, buf);  // loop: read next chunk
                    });
            });
    }
};

// ── Async accept loop ───────────────────────────────────────

void do_accept(tcp::acceptor& acc, asio::io_context& ctx) {
    acc.async_accept([&](std::error_code ec, tcp::socket client) {
        if (!ec)
            std::make_shared<Session>(std::move(client), ctx)->start();
        do_accept(acc, ctx);   // always queue the next accept
    });
}

// ── main ────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <listen_port> <host:port> [host:port ...]\n"
                  << "Example: " << argv[0]
                  << " 8080 127.0.0.1:8081 127.0.0.1:8082\n";
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    uint16_t listen_port = static_cast<uint16_t>(std::stoi(argv[1]));

    // Parse backend addresses
    for (int i = 2; i < argc; i++) {
        std::string s = argv[i];
        auto c = s.rfind(':');
        if (c == std::string::npos) {
            std::cerr << "Bad format: " << s << " (expected host:port)\n";
            return 1;
        }
        backends.push_back({s.substr(0, c),
                            static_cast<uint16_t>(std::stoi(s.substr(c + 1)))});
    }

    // Set up Asio
    asio::io_context ctx;

    tcp::acceptor acc(ctx);
    acc.open(tcp::v4());
    acc.set_option(tcp::acceptor::reuse_address(true));
    acc.bind({tcp::v4(), listen_port});
    acc.listen(128);

    do_accept(acc, ctx);

    std::cout << "⚡ Load balancer on :" << listen_port
              << " → " << backends.size() << " backend(s)  [async/epoll]\n";

    ctx.run();   // single-threaded event loop — never returns
}
