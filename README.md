# TSN-Style UDP Latency Demo (Linux + Namespaces + C++)

This project is a small **C++ / CMake** demo that simulates a **time-sensitive control flow** between two ECUs using **Linux network namespaces** and a **virtual Ethernet (veth) link**.

It lets you:

- Send **periodic UDP “control” packets** with timestamps (time-triggered traffic).
- Measure **one-way latency** and observe **jitter**.
- Use **IP_TOS marking + `tc` qdisc** to simulate a **TSN-style priority traffic class**.
- Add **background load with `iperf3`** and see how priority improves latency.

It’s not full TSN (no real 802.1Qbv scheduling), but it demonstrates the *core idea*:
> **critical control traffic gets better latency/jitter than best-effort traffic** when prioritized.

---

## 1. Project Structure

```text
.
├── CMakeLists.txt        # CMake build configuration
├── README.md             # This file
├── .gitignore            # Ignore build artifacts, etc.
├── include/
│   └── tsn_udp/
│       └── tsn_udp.hpp   # Header-only library (server + client)
└── src/
    └── main.cpp          # CLI entrypoint (server/client modes)
