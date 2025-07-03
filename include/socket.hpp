#pragma once

#include <functional>

#include "stream.pb.h"

namespace Socket {
    void Init();
    bool Start();
    void Stop(std::function<void()> stopped = nullptr);
    void Refresh(std::function<void(bool)> done = nullptr);
    void Send(PacketWrapper const& packet, void* exclude = nullptr);
}
