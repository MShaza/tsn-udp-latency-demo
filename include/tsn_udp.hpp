#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace tsn_udp {

// Two flow types: control vs logging
enum class FlowKind : std::uint8_t {
    Control = 1,
    Logging = 2,
};

// Packet we send over UDP.
// flow    = which flow (Control / Logging)
// seq     = sequence number
// send_ns = send time in nanoseconds since client start
struct FlowPacket {
    FlowKind flow;
    std::uint64_t seq;
    std::int64_t  send_ns;
};

using steady_clock = std::chrono::steady_clock;

inline const char* flow_to_string(FlowKind f) {
    switch (f) {
        case FlowKind::Control: return "CTRL";
        case FlowKind::Logging: return "LOG ";
        default:                return "UNK ";
    }
}

// =========================
//   SERVER IMPLEMENTATION
// =========================

inline void run_server(std::uint16_t port) {
    // Create a UDP socket (AF_INET = IPv4, SOCK_DGRAM = UDP)
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }

    // Allow reusing the address so you can restart quickly without
    // "address already in use" errors.
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        perror("setsockopt SO_REUSEADDR");
    }

    // Prepare local address to bind to: 0.0.0.0:<port>
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;           // IPv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // listen on all interfaces
    addr.sin_port        = htons(port);       // convert port to network byte order

    // Bind the socket to the local address (so we actually "own" this port)
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return;
    }

    std::cout << "[SERVER] Listening on UDP port " << port << "...\n";

    FlowPacket pkt{};          // buffer for incoming packet
    sockaddr_in src{};         // sender's address
    socklen_t src_len = sizeof(src);

    auto start = steady_clock::now();

    std::uint64_t received_ctrl = 0;
    std::uint64_t received_log  = 0;

    // Main receive loop
    while (true) {
        ssize_t n = recvfrom(fd,
                             &pkt,
                             sizeof(pkt),
                             0,
                             reinterpret_cast<sockaddr*>(&src),
                             &src_len);
        if (n < 0) {
            perror("recvfrom");
            break;
        }

        if (n != static_cast<ssize_t>(sizeof(pkt))) {
            std::cerr << "[SERVER] Received unexpected size: "
                      << n << " bytes\n";
            continue;
        }

        auto now    = steady_clock::now();
        std::int64_t now_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();

        std::int64_t latency_ns = now_ns - pkt.send_ns;
        double latency_us = latency_ns / 1000.0;

        if (pkt.flow == FlowKind::Control) {
            ++received_ctrl;
            if (received_ctrl % 100 == 0) {
                std::cout << "[SERVER][" << flow_to_string(pkt.flow) << "] seq="
                          << pkt.seq << " latency=" << latency_us << " us\n";
            }
        } else if (pkt.flow == FlowKind::Logging) {
            ++received_log;
            if (received_log % 100 == 0) {
                std::cout << "[SERVER][" << flow_to_string(pkt.flow) << "] seq="
                          << pkt.seq << " latency=" << latency_us << " us\n";
            }
        } else {
            std::cerr << "[SERVER] Unknown flow kind, seq=" << pkt.seq
                      << " latency=" << latency_us << " us\n";
        }
    }

    close(fd);
}

// =========================
//   CLIENT IMPLEMENTATION
// =========================

// Common implementation used by both control & logging flows.
inline void run_client_common(const std::string& server_ip,
                              std::uint16_t      port,
                              int                period_us,
                              std::uint64_t      max_packets,
                              FlowKind           flow,
                              int                tos_value) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }

    // Set IP_TOS if requested (for control we use 0x10, for logging 0x00)
    if (tos_value >= 0) {
        int tos = tos_value;
        if (setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0) {
            perror("setsockopt IP_TOS");
        }
    }

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port   = htons(port);
    if (inet_pton(AF_INET, server_ip.c_str(), &dst.sin_addr) != 1) {
        std::cerr << "Invalid server IP: " << server_ip << "\n";
        close(fd);
        return;
    }

    std::cout << "[CLIENT][" << flow_to_string(flow) << "] Sending to "
              << server_ip << ":" << port
              << " every " << period_us << " us, up to "
              << max_packets << " packets.\n";

    auto start     = steady_clock::now();
    auto next_send = start;

    FlowPacket pkt{};
    pkt.flow = flow;

    for (std::uint64_t i = 0; i < max_packets; ++i) {
        next_send += std::chrono::microseconds(period_us);
        std::this_thread::sleep_until(next_send);

        auto now = steady_clock::now();
        std::int64_t send_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();

        pkt.seq     = i;
        pkt.send_ns = send_ns;

        ssize_t n = sendto(fd,
                           &pkt,
                           sizeof(pkt),
                           0,
                           reinterpret_cast<sockaddr*>(&dst),
                           sizeof(dst));
        if (n < 0) {
            perror("sendto");
            break;
        }
    }

    std::cout << "[CLIENT][" << flow_to_string(flow) << "] Done sending.\n";
    close(fd);
}

// Control = high-priority flow (TOS = 0x10)
inline void run_control_client(const std::string& server_ip,
                               std::uint16_t      port,
                               int                period_us,
                               std::uint64_t      max_packets) {
    run_client_common(server_ip, port, period_us, max_packets,
                      FlowKind::Control, 0x10);
}

// Logging = best-effort flow (TOS = 0x00 / default)
inline void run_logging_client(const std::string& server_ip,
                               std::uint16_t      port,
                               int                period_us,
                               std::uint64_t      max_packets) {
    run_client_common(server_ip, port, period_us, max_packets,
                      FlowKind::Logging, 0x00);
}

} // namespace tsn_udp
