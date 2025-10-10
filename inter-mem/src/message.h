#pragma once
#include <cstdint>

enum class MsgType : uint8_t {
    READ,
    READX,      // Read for exclusive (write intent)
    WRITE,
    INVALIDATE,
    WRITEBACK,
    RESPONSE
};

struct BusMessage {
    uint32_t msg_id;
    uint8_t  src;       // PE id 0..3
    MsgType  type;
    uint32_t address;   // byte address
    uint8_t  size;      // bytes (usually 8)
    uint64_t data;      // valid for WRITE/RESPONSE
    uint64_t timestamp; // user-defined timestamp
};
