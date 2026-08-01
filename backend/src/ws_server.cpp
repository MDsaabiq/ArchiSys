// stdlib before winsock2
#include <vector>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <string>
#include <cstdint>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
// Make recv/send non-blocking via ioctlsocket
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <fcntl.h>
#endif

#include "ws_server.hpp"

namespace archisys {

// ─────────────────────────────────────────────────────────────────────────────
// SHA-1 (RFC 3174) — hand-rolled, no dependencies
// ─────────────────────────────────────────────────────────────────────────────
void WsClient::sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
    uint32_t h0 = 0x67452301u, h1 = 0xEFCDAB89u,
             h2 = 0x98BADCFEu, h3 = 0x10325476u, h4 = 0xC3D2E1F0u;

    auto rotl = [](uint32_t v, int n) -> uint32_t {
        return (v << n) | (v >> (32 - n));
    };

    // Pre-processing: build padded message
    uint64_t bitLen = static_cast<uint64_t>(len) * 8;
    std::vector<uint8_t> msg(data, data + len);
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) msg.push_back(0x00);
    for (int i = 7; i >= 0; --i) msg.push_back(static_cast<uint8_t>(bitLen >> (i * 8)));

    for (size_t i = 0; i < msg.size(); i += 64) {
        uint32_t w[80];
        for (int j = 0; j < 16; ++j) {
            w[j] = (uint32_t(msg[i+j*4])   << 24) |
                   (uint32_t(msg[i+j*4+1]) << 16) |
                   (uint32_t(msg[i+j*4+2]) <<  8) |
                    uint32_t(msg[i+j*4+3]);
        }
        for (int j = 16; j < 80; ++j)
            w[j] = rotl(w[j-3] ^ w[j-8] ^ w[j-14] ^ w[j-16], 1);

        uint32_t a=h0, b=h1, c=h2, d=h3, e=h4;
        for (int j = 0; j < 80; ++j) {
            uint32_t f, k;
            if      (j < 20) { f = (b & c) | (~b & d);         k = 0x5A827999u; }
            else if (j < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1u; }
            else if (j < 60) { f = (b & c) | (b & d) | (c & d);k = 0x8F1BBCDCu; }
            else              { f = b ^ c ^ d;                   k = 0xCA62C1D6u; }
            uint32_t temp = rotl(a,5) + f + e + k + w[j];
            e = d; d = c; c = rotl(b,30); b = a; a = temp;
        }
        h0+=a; h1+=b; h2+=c; h3+=d; h4+=e;
    }
    uint32_t H[5] = {h0,h1,h2,h3,h4};
    for (int i = 0; i < 5; ++i) {
        out[i*4+0] = (H[i] >> 24) & 0xFF;
        out[i*4+1] = (H[i] >> 16) & 0xFF;
        out[i*4+2] = (H[i] >>  8) & 0xFF;
        out[i*4+3] =  H[i]        & 0xFF;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Base64 encode
// ─────────────────────────────────────────────────────────────────────────────
std::string WsClient::base64Encode(const uint8_t* data, size_t len) {
    static const char* B64 =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = uint32_t(data[i]) << 16;
        if (i+1 < len) v |= uint32_t(data[i+1]) << 8;
        if (i+2 < len) v |= uint32_t(data[i+2]);
        out += B64[(v >> 18) & 0x3F];
        out += B64[(v >> 12) & 0x3F];
        out += (i+1 < len) ? B64[(v >> 6) & 0x3F] : '=';
        out += (i+2 < len) ? B64[(v     ) & 0x3F] : '=';
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Compute Sec-WebSocket-Accept
// ─────────────────────────────────────────────────────────────────────────────
std::string WsClient::computeAcceptKey(const std::string& clientKey) {
    std::string magic = clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    uint8_t digest[20];
    sha1(reinterpret_cast<const uint8_t*>(magic.data()), magic.size(), digest);
    return base64Encode(digest, 20);
}

// ─────────────────────────────────────────────────────────────────────────────
// Frame encode (server→client: no mask, opcode 0x1 = text)
// ─────────────────────────────────────────────────────────────────────────────
std::string WsClient::encodeTextFrame(const std::string& payload) {
    std::string frame;
    frame.reserve(payload.size() + 10);
    frame += char(0x81); // FIN + opcode TEXT
    size_t sz = payload.size();
    if (sz <= 125) {
        frame += char(sz);
    } else if (sz <= 65535) {
        frame += char(126);
        frame += char((sz >> 8) & 0xFF);
        frame += char( sz       & 0xFF);
    } else {
        frame += char(127);
        for (int i = 7; i >= 0; --i)
            frame += char((sz >> (i * 8)) & 0xFF);
    }
    frame += payload;
    return frame;
}

// ─────────────────────────────────────────────────────────────────────────────
// Frame decode (client→server: always masked per RFC 6455)
// Returns true and sets opcode+payload if a complete frame was parsed.
// consumed = how many bytes were used from buf.
// ─────────────────────────────────────────────────────────────────────────────
bool WsClient::decodeFrame(const uint8_t* buf, size_t len,
                           uint8_t& opcode, std::string& payload,
                           size_t& consumed) {
    if (len < 2) return false;
    opcode       = buf[0] & 0x0F;
    bool masked  = (buf[1] & 0x80) != 0;
    size_t plen  =  buf[1] & 0x7F;
    size_t offset = 2;

    if (plen == 126) {
        if (len < 4) return false;
        plen   = (size_t(buf[2]) << 8) | buf[3];
        offset = 4;
    } else if (plen == 127) {
        if (len < 10) return false;
        plen = 0;
        for (int i = 0; i < 8; ++i) plen = (plen << 8) | buf[2 + i];
        offset = 10;
    }

    if (masked) {
        if (len < offset + 4 + plen) return false;
        const uint8_t* mask = buf + offset;
        offset += 4;
        payload.resize(plen);
        for (size_t i = 0; i < plen; ++i)
            payload[i] = char(buf[offset + i] ^ mask[i % 4]);
    } else {
        if (len < offset + plen) return false;
        payload.assign(reinterpret_cast<const char*>(buf + offset), plen);
    }
    consumed = offset + plen;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// WsClient public interface
// ─────────────────────────────────────────────────────────────────────────────
WsClient::WsClient(SOCKET s) : sock_(s) {}

WsClient::~WsClient() { close(); }

bool WsClient::doHandshake(const std::string& rawRequest) {
    // Find Sec-WebSocket-Key header
    std::string key;
    std::istringstream ss(rawRequest);
    std::string line;
    while (std::getline(ss, line)) {
        // Normalize
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if (lower.rfind("sec-websocket-key:", 0) == 0) {
            key = line.substr(18);
            while (!key.empty() && key.front() == ' ') key.erase(key.begin());
        }
    }
    if (key.empty()) return false;

    std::string accept = computeAcceptKey(key);
    writeSwitchingProtocols(sock_, accept);

    // Set a 100ms recv timeout so the receive loop is non-blocking
    // without toggling FIONBIO (which races with send() on Windows)
#ifdef _WIN32
    DWORD tv = 100; // milliseconds
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv2 { 0, 100000 };
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&tv2), sizeof(tv2));
#endif
    return true;
}

bool WsClient::send(const std::string& text) {
    if (closed_) return false;
    std::string frame = encodeTextFrame(text);
    int n = ::send(sock_, frame.c_str(), static_cast<int>(frame.size()), 0);
    if (n <= 0) { closed_ = true; return false; }
    return true;
}

bool WsClient::recv(std::string& out) {
    if (closed_) return false;

    uint8_t buf[65536];
    int n = ::recv(sock_, reinterpret_cast<char*>(buf), sizeof(buf), 0);

    if (n <= 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK || err == WSAETIMEDOUT) return false;
#endif
        closed_ = true;
        return false;
    }

    uint8_t opcode = 0;
    size_t  consumed = 0;
    if (!decodeFrame(buf, n, opcode, out, consumed)) return false;

    if (opcode == 0x8) { // Close
        // Send close frame back
        std::string closeFrame;
        closeFrame += char(0x88); closeFrame += char(0x00);
        ::send(sock_, closeFrame.c_str(), 2, 0);
        closed_ = true;
        return false;
    }
    if (opcode == 0x9) { // Ping → send Pong
        std::string pong;
        pong += char(0x8A); pong += char(0x00);
        ::send(sock_, pong.c_str(), 2, 0);
        return false;
    }
    return opcode == 0x1 || opcode == 0x2; // text or binary
}

void WsClient::close() {
    if (!closed_) {
        // Send close frame
        std::string closeFrame;
        closeFrame += char(0x88); closeFrame += char(0x00);
        ::send(sock_, closeFrame.c_str(), 2, 0);
        closesocket(sock_);
        closed_ = true;
    }
}

} // namespace archisys
