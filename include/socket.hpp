#pragma once

#include <functional>

#include "stream.pb.h"

namespace Socket {
    void Init();
    void Refresh(std::function<void(bool)> done);
    void Send(PacketWrapper const& packet, void* exclude = nullptr);
}
