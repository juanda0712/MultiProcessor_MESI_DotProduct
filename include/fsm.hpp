#pragma once
#include <cstdint>

enum class MESI : uint8_t {
    I,  // Invalid
    S,  // Shared
    E,  // Exclusive
    M,  // Modified
    IS, // Intermedio: transición hacia Shared
    IM, // Intermedio: hacia Modified
    SM, // Intermedio: Shared → Modified
    EM  // Intermedio: Exclusive → Modified
};

static inline const char* mesi_str(MESI s) {
    switch (s) {
        case MESI::I:  return "I";
        case MESI::S:  return "S";
        case MESI::E:  return "E";
        case MESI::M:  return "M";
        case MESI::IS: return "IS";
        case MESI::IM: return "IM";
        case MESI::SM: return "SM";
        case MESI::EM: return "EM";
        default:       return "?";
    }
}
