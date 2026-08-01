#pragma once

// WinSock2 must come before any windows.h
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  define SOCKET int
#  define INVALID_SOCKET (-1)
#  define SOCKET_ERROR   (-1)
#  define closesocket    close
#endif

#include <string>
#include <map>

namespace archisys {

// ── HTTP request ────────────────────────────────────────────────────────────
struct HttpRequest {
    std::string method;    // "GET", "POST"
    std::string path;      // "/start", "/ws"
    std::string body;
    std::map<std::string, std::string> headers;
    std::string rawRequest; // full original text (for WS handshake)

    bool isWebSocketUpgrade() const {
        auto it = headers.find("upgrade");
        return it != headers.end() &&
               it->second.find("websocket") != std::string::npos;
    }
};

// ── HTTP response ────────────────────────────────────────────────────────────
struct HttpResponse {
    int         status      = 200;
    std::string body;
    std::string contentType = "application/json";

    static HttpResponse ok(std::string body) {
        HttpResponse r; r.body = std::move(body); return r;
    }
    static HttpResponse error(int code, std::string msg) {
        HttpResponse r;
        r.status = code;
        r.body   = "{\"error\":\"" + msg + "\"}";
        return r;
    }
};

// ── Socket helpers ────────────────────────────────────────────────────────────
// Reads a complete HTTP request from socket (blocks until headers + body received).
// Returns false on disconnect or error.
bool readHttpRequest(SOCKET s, HttpRequest& out);

// Sends a complete HTTP response.
void writeHttpResponse(SOCKET s, const HttpResponse& r);

// Sends a raw HTTP 101 Switching Protocols response (WebSocket handshake).
void writeSwitchingProtocols(SOCKET s, const std::string& acceptKey);

} // namespace archisys
