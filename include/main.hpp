#pragma once

#include "beatsaber-hook/shared/utils/logging.hpp"

static constexpr auto logger = Paper::ConstLoggerContext(MOD_ID);

#include <chrono>

static inline uint64_t Time() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
