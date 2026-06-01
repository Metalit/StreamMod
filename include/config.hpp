#pragma once

#include "HMUI/ViewController.hpp"
#include "config-utils/shared/config-utils.hpp"

namespace Config {
    inline std::vector<std::pair<int, int>> const Resolutions = {{1280, 720}, {1920, 1080}, {2560, 1440}};
    inline std::vector<std::string> const ResolutionStrings = {"720p", "1080p", "1440p"};
    inline std::vector<std::string> const MixModeStrings = {"Combine", "Duck", "Add"};
    inline std::vector<std::string> const CameraModeStrings = {"Smooth", "Third Person"};
    inline std::vector<std::string> const NoteVisibilityStrings = {"Default", "Hidden", "No Debris"};
    inline std::vector<std::string> const WallVisibilityStrings = {"Default", "Hidden", "Frame Only"};
    inline std::vector<std::string> const AvatarVisibilityStrings = {"Default", "Hidden"};

    void CreateMenu(HMUI::ViewController* self, bool firstActivation, bool, bool);
    void UpdateMenu();
    void Invalidate();
}

DECLARE_CONFIG(Config) {
    CONFIG_VALUE(Port, std::string, "Connection Port", "3308", "The port to listen for connections on");

    CONFIG_VALUE(Width, int, "Resolution Width", Config::Resolutions[0].first);
    CONFIG_VALUE(Height, int, "Resolution Height", Config::Resolutions[0].second);
    CONFIG_VALUE(Bitrate, int, "Stream Bitrate", 10000, "The bitrate of the stream in kbps");

    CONFIG_VALUE(GameVolume, float, "Game Volume", 1, "The volume level of the game audio");
    CONFIG_VALUE(Mic, bool, "Record Microphone", true, "Whether to include microphone audio");
    CONFIG_VALUE(MicVolume, float, "Microphone Volume", 1, "The volume level of the microphone audio");
    CONFIG_VALUE(MicThreshold, float, "Microphone Threshold", 1, "At what loudness should noises from the microphone be heard");
    CONFIG_VALUE(MixMode, int, "Mixing Mode", 0, "How to combine the microphone audio with the game audio");

    CONFIG_VALUE(CameraMode, int, "Camera Mode", 0);
    CONFIG_VALUE(CameraPos, UnityEngine::Vector3, "Camera Position", UnityEngine::Vector3(0, 0, -1), "The camera position or offset");
    CONFIG_VALUE(CameraRot, UnityEngine::Vector3, "Camera Rotation", UnityEngine::Vector3(0, 0, 0), "The camera rotation or offset");
    CONFIG_VALUE(Smoothing, float, "Camera Smoothing", 1, "The amount of smoothing to apply to the camera");
    CONFIG_VALUE(Tilt, float, "Camera Tilt", 0, "Makes the camera look more upwards or downwards");

    CONFIG_VALUE(NoteVisibility, int, "Note Visibility", 2, "How to display notes in the stream");
    CONFIG_VALUE(WallVisibility, int, "Wall Visibility", 0, "How to display walls in the stream");
    CONFIG_VALUE(AvatarVisibility, bool, "Avatar Visibility", false, "Whether to display avatars in the stream");
    CONFIG_VALUE(UIVisibility, bool, "UI Visibility", false, "Whether to display UI in the stream");
    CONFIG_VALUE(CutParticleVisibility, bool, "Cut Particle Visibility", true, "Whether to display cut particles in the stream");

    CONFIG_VALUE(FPS, float, "Stream FPS", 30, "The frames per second of the stream");
    CONFIG_VALUE(FOV, float, "Stream FOV", 70, "The fov of the stream camera");
    CONFIG_VALUE(Orthographic, bool, "Orthographic", false, "Removes perspective from the camera");

    // CONFIG_VALUE(Hollywood, bool, "Separate Camera", false);
};
