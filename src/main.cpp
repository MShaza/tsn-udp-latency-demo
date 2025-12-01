#include "tsn_udp.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage:\n"
                  << "  SERVER: " << argv[0] << " server <port>\n"
                  << "  CLIENT CONTROL: " << argv[0]
                  << " client control <server_ip> <port> <period_us> <num_packets>\n"
                  << "  CLIENT LOGGING: " << argv[0]
                  << " client logging <server_ip> <port> <period_us> <num_packets>\n";
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "server") {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " server <port>\n";
            return 1;
        }
        std::uint16_t port = static_cast<std::uint16_t>(std::stoi(argv[2]));
        tsn_udp::run_server(port);

    } else if (mode == "client") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0]
                      << " client <control|logging> <server_ip> <port> <period_us> <num_packets>\n";
            return 1;
        }

        std::string flow_mode = argv[2];

        if (flow_mode == "control") {
            // ./tsn_udp_demo client control <server_ip> <port> <period_us> <num_packets>
            if (argc != 7) {
                std::cerr << "Usage: " << argv[0]
                          << " client control <server_ip> <port> <period_us> <num_packets>\n";
                return 1;
            }

            std::string server_ip   = argv[3];
            std::uint16_t port      = static_cast<std::uint16_t>(std::stoi(argv[4]));
            int          period_us  = std::stoi(argv[5]);
            std::uint64_t num_pkts  = std::stoull(argv[6]);

            tsn_udp::run_control_client(server_ip, port, period_us, num_pkts);

        } else if (flow_mode == "logging") {
            // ./tsn_udp_demo client logging <server_ip> <port> <period_us> <num_packets>
            if (argc != 7) {
                std::cerr << "Usage: " << argv[0]
                          << " client logging <server_ip> <port> <period_us> <num_packets>\n";
                return 1;
            }

            std::string server_ip   = argv[3];
            std::uint16_t port      = static_cast<std::uint16_t>(std::stoi(argv[4]));
            int          period_us  = std::stoi(argv[5]);
            std::uint64_t num_pkts  = std::stoull(argv[6]);

            tsn_udp::run_logging_client(server_ip, port, period_us, num_pkts);

        } else {
            std::cerr << "Unknown client flow mode: " << flow_mode
                      << " (expected 'control' or 'logging')\n";
            return 1;
        }

    } else {
        std::cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }

    return 0;
}
