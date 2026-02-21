#include "uuid.h"
#include <random>
#include <sstream>

namespace orderbook {

std::string generateUuid() {
    // Use thread_local to avoid re-creating RNG on every call
    thread_local std::mt19937 gen(std::random_device{}());
    thread_local std::uniform_int_distribution<> dis(0, 15);
    thread_local std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";  // Version 4 UUID
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);  // Variant
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 12; i++) {
        ss << dis(gen);
    }

    return ss.str();
}

}