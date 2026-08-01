#pragma once
#include "http_server.hpp"
#include <string>
#include <cstdint>

namespace archisys {

// ── WebSocket client wrapper ─────────────────────────────────────────────────
// Handles RFC 6455 handshake, frame encoding (server→client, unmasked),
// and frame decoding (client→server, always masked per RFC).
class WsClient {
public:
    explicit WsClient(SOCKET s);
    ~WsClient();

    // Complete the HTTP→WebSocket upgrade handshake.
    // Pass the full raw HTTP request text (including headers).
    // Returns false if handshake fails.
    bool doHandshake(const std::string& rawRequest);

    // Send a UTF-8 text frame to the client (server→client, no mask).
    // Returns false if the socket is closed or write fails.
    bool send(const std::string& text);

    // Non-blocking receive. Returns true and sets `out` if a complete
    // text/ping/close frame was read. Returns false if no data available.
    bool recv(std::string& out);

    void close();
    bool isClosed() const { return closed_; }
    SOCKET socket()  const { return sock_; }

private:
    SOCKET sock_;
    bool   closed_ = false;

    // SHA-1 + Base64 for the handshake accept key
    static std::string computeAcceptKey(const std::string& clientKey);
    static std::string base64Encode(const uint8_t* data, size_t len);
    static void        sha1(const uint8_t* data, size_t len, uint8_t out[20]);

    // Frame helpers
    static std::string encodeTextFrame(const std::string& payload);
    static bool        decodeFrame(const uint8_t* buf, size_t len,
                                   uint8_t& opcode, std::string& payload,
                                   size_t& consumed);
};

} // namespace archisys
