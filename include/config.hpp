#pragma once

#include "HMUI/ViewController.hpp"
#include "config-utils/shared/config-utils.hpp"

namespace Config {
    inline std::vector<std::pair<int, int>> const Resolutions = {{1280, 720}, {1920, 1080}, {2560, 1440}};
    inline std::vector<std::string> const ResolutionStrings = {"720p", "1080p", "1440p"};
    static std::vector<std::string> const MixModeStrings = {"Combine", "Duck", "Add"};

    void CreateMenu(HMUI::ViewController* self, bool firstActivation, bool, bool);
    void UpdateMenu();
    void Invalidate();
}

DECLARE_CONFIG(Config) {
    CONFIG_VALUE(Port, std::string, "Connection Port", "3308", "The port to listen for connections on");

    CONFIG_VALUE(Width, int, "Resolution Width", Config::Resolutions[0].first);
    CONFIG_VALUE(Height, int, "Resolution Height", Config::Resolutions[0].second);
    CONFIG_VALUE(Bitrate, int, "Stream Bitrate", 10000, "The bitrate of the stream in kbps");
    CONFIG_VALUE(FPS, float, "Stream FPS", 30, "The frames per second of the stream");
    CONFIG_VALUE(FOV, float, "Stream FOV", 80, "The fov of the stream camera");

    CONFIG_VALUE(FPFC, bool, "FPFC", false);

    CONFIG_VALUE(Smoothing, float, "Camera Smoothing", 1, "The amount of smoothing to apply to the streamed camera");
    // CONFIG_VALUE(Hollywood, bool, "Separate Camera", false);
    CONFIG_VALUE(Mic, bool, "Record Microphone", true, "Whether to include microphone audio");

    CONFIG_VALUE(GameVolume, float, "Game Volume", 1, "The volume level of the game audio");
    CONFIG_VALUE(MicVolume, float, "Microphone Volume", 1, "The volume level of the microphone audio");
    CONFIG_VALUE(MicThreshold, float, "Microphone Threshold", 1, "At what loudness should noises from the microphone be heard");
    CONFIG_VALUE(MixMode, int, "Mixing Mode", 1, "How to combine the microphone audio with the game audio");
};
