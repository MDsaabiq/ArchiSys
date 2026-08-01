#include "http_server.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace archisys {

// ── toLower ───────────────────────────────────────────────────────────────────
static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}

// ── readAll ───────────────────────────────────────────────────────────────────
// Reads bytes from socket into buf, up to maxBytes.
// Returns number of bytes read, 0 on disconnect, -1 on error.
static int readBytes(SOCKET s, char* buf, int len) {
    return recv(s, buf, len, 0);
}

// ── readHttpRequest ───────────────────────────────────────────────────────────
bool readHttpRequest(SOCKET s, HttpRequest& out) {
    std::string raw;
    raw.reserve(4096);

    char buf[4096];
    // Read until we have the full header section (\r\n\r\n)
    while (true) {
        int n = readBytes(s, buf, sizeof(buf) - 1);
        if (n <= 0) return false;
        raw.append(buf, n);
        if (raw.find("\r\n\r\n") != std::string::npos) break;
        if (raw.size() > 65536) return false; // safety
    }

    out.rawRequest = raw;

    // Parse request line
    std::istringstream stream(raw);
    std::string line;
    if (!std::getline(stream, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream rl(line);
    rl >> out.method >> out.path;

    // Parse headers
    int contentLength = 0;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break; // end of headers

        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key   = toLower(line.substr(0, colon));
        std::string value = line.substr(colon + 1);
        // trim leading whitespace from value
        size_t vs = value.find_first_not_of(" \t");
        if (vs != std::string::npos) value = value.substr(vs);
        out.headers[key] = value;

        if (key == "content-length") {
            try { contentLength = std::stoi(value); } catch(...) {}
        }
    }

    // Read body if Content-Length specified
    if (contentLength > 0) {
        // How many body bytes did we already read past \r\n\r\n ?
        size_t headerEnd = raw.find("\r\n\r\n");
        out.body = raw.substr(headerEnd + 4);

        // Read remaining body bytes
        while (static_cast<int>(out.body.size()) < contentLength) {
            int need = contentLength - static_cast<int>(out.body.size());
            int n    = readBytes(s, buf, std::min(need, (int)sizeof(buf) - 1));
            if (n <= 0) break;
            out.body.append(buf, n);
        }
    }

    return true;
}

// ── writeHttpResponse ─────────────────────────────────────────────────────────
void writeHttpResponse(SOCKET s, const HttpResponse& r) {
    std::string statusText;
    switch (r.status) {
        case 200: statusText = "OK";                    break;
        case 400: statusText = "Bad Request";           break;
        case 404: statusText = "Not Found";             break;
        case 500: statusText = "Internal Server Error"; break;
        default:  statusText = "Unknown";               break;
    }

    std::ostringstream resp;
    resp << "HTTP/1.1 " << r.status << " " << statusText << "\r\n"
         << "Content-Type: " << r.contentType << "\r\n"
         << "Content-Length: " << r.body.size() << "\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
         << "Access-Control-Allow-Headers: Content-Type\r\n"
         << "Connection: close\r\n"
         << "\r\n"
         << r.body;

    std::string text = resp.str();
    send(s, text.c_str(), static_cast<int>(text.size()), 0);
}

// ── writeSwitchingProtocols ───────────────────────────────────────────────────
void writeSwitchingProtocols(SOCKET s, const std::string& acceptKey) {
    std::ostringstream resp;
    resp << "HTTP/1.1 101 Switching Protocols\r\n"
         << "Upgrade: websocket\r\n"
         << "Connection: Upgrade\r\n"
         << "Sec-WebSocket-Accept: " << acceptKey << "\r\n"
         << "\r\n";
    std::string text = resp.str();
    send(s, text.c_str(), static_cast<int>(text.size()), 0);
}

} // namespace archisys
