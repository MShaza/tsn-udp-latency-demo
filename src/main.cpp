#include "tsn_udp.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage:\n"
                  << "  SERVER: " << argv[0] << " server <port>\n"
                  << "  CLIENT: " << argv[0]
                  << " client <server_ip> <port> <period_us> <num_packets>\n";
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "server") {
        // Expect: ./app server <port>
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " server <port>\n";
            return 1;
        }
        uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));
        run_server(port);

    } else if (mode == "client") {
        // Expect: ./app client <server_ip> <port> <period_us> <num_packets>
        if (argc != 6) {
            std::cerr << "Usage: " << argv[0]
                      << " client <server_ip> <port> <period_us> <num_packets>\n";
            return 1;
        }

        std::string server_ip   = argv[2];
        uint16_t    port        = static_cast<uint16_t>(std::stoi(argv[3]));
        int         period_us   = std::stoi(argv[4]);
        uint64_t    num_packets = std::stoull(argv[5]);

        run_client(server_ip, port, period_us, num_packets);

    } else {
        std::cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }

    return 0;
}
