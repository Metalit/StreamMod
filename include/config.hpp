#pragma once

#include "config-utils/shared/config-utils.hpp"

inline std::vector<std::pair<int, int>> const resolutions = {{1280, 720}, {1920, 1080}, {2560, 1440}};

// clang-format off
DECLARE_CONFIG(Config,
    CONFIG_VALUE(Port, std::string, "Connection Port", "3308", "The port to listen for connections on");

    CONFIG_VALUE(Width, int, "Resolution Width", resolutions[0].first);
    CONFIG_VALUE(Height, int, "Resolution Height", resolutions[0].second);
    CONFIG_VALUE(Bitrate, int, "Stream Bitrate", 10000, "The bitrate of the stream in kbps");
    CONFIG_VALUE(FPS, float, "Stream FPS", 30, "The frames per second of the stream");
    CONFIG_VALUE(FOV, float, "Stream FOV", 80, "The fov of the stream camera");

    CONFIG_VALUE(Smoothing, float, "Camera Smoothing", 1, "The amount of smoothing to apply to the streamed camera");
    // CONFIG_VALUE(Hollywood, bool, "Separate Camera", false);
    CONFIG_VALUE(Mic, bool, "Record Microphone", true, "Whether to include microphone audio");

    CONFIG_VALUE(GameVolume, float, "Game Volume", 1, "The volume level of the game audio");
    CONFIG_VALUE(MicVolume, float, "Microphone Volume", 1, "The volume level of the microphone audio");
    CONFIG_VALUE(MicThreshold, float, "Microphone Threshold", 0.5, "At what loudness should noises from the microphone be heard");
    CONFIG_VALUE(MixMode, int, "Mixing Mode", 1, "How to combine the microphone audio with the game audio");
)
// clang-format on
