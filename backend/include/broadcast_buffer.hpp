#pragma once
// Include mutex before any windows headers
#include <string>
#include <mutex>

namespace archisys {

// Thread-safe single-slot buffer.
// Producer (sim thread) always overwrites with the latest JSON.
// Consumer (broadcaster thread) reads only the newest available message.
class BroadcastBuffer {
public:
    void put(std::string json) {
        std::lock_guard<std::mutex> lk(mtx_);
        latest_  = std::move(json);
        hasNew_  = true;
    }

    // Returns true and populates `out` if a new message is available.
    bool take(std::string& out) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!hasNew_) return false;
        out     = latest_;
        hasNew_ = false;
        return true;
    }

private:
    std::mutex  mtx_;
    std::string latest_;
    bool        hasNew_ = false;
};

} // namespace archisys
