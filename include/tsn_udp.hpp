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

// This struct is the payload we send over UDP.
// seq     = sequence number of the packet
// send_ns = send time in nanoseconds since the sender's start time
struct ControlPacket {
    uint64_t seq;
    int64_t  send_ns;
};

// Short alias so we don't have to type std::chrono::steady_clock every time
using steady_clock = std::chrono::steady_clock;

// =========================
//   SERVER IMPLEMENTATION
// =========================

inline void run_server(uint16_t port) {
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
    addr.sin_family      = AF_INET;              // IPv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY);    // listen on all interfaces
    addr.sin_port        = htons(port);          // convert port to network byte order

    // Bind the socket to the local address (so we actually "own" this port)
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return;
    }

    std::cout << "[SERVER] Listening on UDP port " << port << "...\n";

    ControlPacket pkt{};        // buffer for incoming packet
    sockaddr_in src{};          // will hold sender's address
    socklen_t src_len = sizeof(src);

    // Start time reference for latency calculations
    auto start    = steady_clock::now();
    uint64_t received = 0;      // count how many packets we've received

    // Main receive loop: runs forever until an error occurs
    while (true) {
        ssize_t n = recvfrom(fd,
                             &pkt,
                             sizeof(pkt),
                             0,
                             reinterpret_cast<sockaddr*>(&src),
                             &src_len);
        if (n < 0) {
            perror("recvfrom");
            break;  // exit loop on error
        }

        // If size doesn't match our struct, ignore it
        if (n != static_cast<ssize_t>(sizeof(pkt))) {
            std::cerr << "[SERVER] Received unexpected size: " << n << " bytes\n";
            continue;
        }

        // Current time since 'start' in nanoseconds
        auto   now    = steady_clock::now();
        int64_t now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();

        // Latency = (receive time) - (send time embedded in packet)
        int64_t latency_ns = now_ns - pkt.send_ns;

        received++;
        // Print every 100th packet to avoid spamming
        if (received % 100 == 0) {
            double latency_us = latency_ns / 1000.0; // convert ns -> microseconds
            std::cout << "[SERVER] seq=" << pkt.seq
                      << " latency=" << latency_us << " us\n";
        }
    }

    close(fd);  // cleanup
}

// =========================
//   CLIENT IMPLEMENTATION
// =========================

inline void run_client(const std::string& server_ip,
                       uint16_t          port,
                       int               period_us,
                       uint64_t          max_packets) {
    // Create UDP socket
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }

    // Mark packets with IP_TOS = 0x10 so we can prioritize them with 'tc' later.
    int tos = 0x10;
    if (setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0) {
        perror("setsockopt IP_TOS");
    }

    // Set up destination address (server)
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port   = htons(port);
    if (inet_pton(AF_INET, server_ip.c_str(), &dst.sin_addr) != 1) {
        std::cerr << "Invalid server IP: " << server_ip << "\n";
        close(fd);
        return;
    }

    std::cout << "[CLIENT] Sending to " << server_ip << ":" << port
              << " every " << period_us << " us, up to "
              << max_packets << " packets.\n";

    // Reference start time for timestamps
    auto start     = steady_clock::now();
    auto next_send = start;

    ControlPacket pkt{};

    for (uint64_t i = 0; i < max_packets; ++i) {
        // Compute the desired send time for this iteration
        next_send += std::chrono::microseconds(period_us);

        // Sleep until that absolute time (time-triggered behavior)
        std::this_thread::sleep_until(next_send);

        // Timestamp just before sending
        auto   now     = steady_clock::now();
        int64_t send_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();

        pkt.seq     = i;
        pkt.send_ns = send_ns;

        // Send one UDP datagram containing our ControlPacket struct
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

    std::cout << "[CLIENT] Done sending.\n";
    close(fd);
}
