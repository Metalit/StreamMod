#pragma once

#if __has_include("bsml/shared/BSML.hpp")
#include "HMUI/FlowCoordinator.hpp"
#include "HMUI/InputFieldView.hpp"
#include "HMUI/ViewController.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "bsml/shared/BSML/Components/Settings/IncrementSetting.hpp"
#include "bsml/shared/BSML/Components/Settings/SliderSetting.hpp"
#include "bsml/shared/BSML/Components/Settings/ToggleSetting.hpp"
#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(StreamMod, MainSettings, HMUI::ViewController) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool, bool);
    DECLARE_INSTANCE_METHOD(void, UpdateMenu);

    DECLARE_STATIC_METHOD(MainSettings*, GetInstance);

   private:
    TMPro::TextMeshProUGUI* ip;
    HMUI::InputFieldView* port;
    BSML::ToggleSetting* preview;
    BSML::IncrementSetting* resolution;
    BSML::SliderSetting* bitrate;
    BSML::SliderSetting* fps;
    BSML::SliderSetting* fov;
    BSML::ToggleSetting* orthographic;

    static MainSettings* instance;
};

DECLARE_CLASS_CODEGEN(StreamMod, VisibilityAudioSettings, HMUI::ViewController) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool, bool);
    DECLARE_INSTANCE_METHOD(void, UpdateMenu);

    DECLARE_STATIC_METHOD(VisibilityAudioSettings*, GetInstance);

   private:
    BSML::SliderSetting* gameVolume;
    BSML::ToggleSetting* mic;
    BSML::SliderSetting* micVolume;
    BSML::SliderSetting* micThreshold;
    BSML::IncrementSetting* mixMode;

    BSML::IncrementSetting* noteVis;
    BSML::IncrementSetting* wallVis;
    BSML::ToggleSetting* avatarVis;
    BSML::ToggleSetting* uiVis;
    BSML::ToggleSetting* cutParticleVis;

    static VisibilityAudioSettings* instance;
};

DECLARE_CLASS_CODEGEN(StreamMod, PositionSettings, HMUI::ViewController) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool, bool);
    DECLARE_INSTANCE_METHOD(void, SetIncrementsActive);
    DECLARE_INSTANCE_METHOD(void, UpdateMenu);

    DECLARE_STATIC_METHOD(PositionSettings*, GetInstance);

   private:
    BSML::IncrementSetting* cameraMode;
    BSML::SliderSetting* smoothness;
    std::array<BSML::IncrementSetting*, 3> smoothPosition;
    std::array<BSML::IncrementSetting*, 3> smoothRotation;
    std::array<BSML::IncrementSetting*, 3> position;
    std::array<BSML::IncrementSetting*, 3> rotation;

    static PositionSettings* instance;
};

DECLARE_CLASS_CODEGEN(StreamMod, Settings, HMUI::FlowCoordinator) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::FlowCoordinator::DidActivate, bool firstActivation, bool, bool);
    DECLARE_OVERRIDE_METHOD_MATCH(void, BackButtonWasPressed, &HMUI::FlowCoordinator::BackButtonWasPressed, HMUI::ViewController* topViewController);

    DECLARE_STATIC_METHOD(void, UpdateMenus);
};

void UpdateSettings();
#else
inline void UpdateMenu() {}
#endif
