#include "config.hpp"

#if __has_include("bsml/shared/BSML.hpp")
#include "System/Net/Dns.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPHostEntry.hpp"
#include "System/Net/Sockets/AddressFamily.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "metacore/shared/ui.hpp"
#include "socket.hpp"

static BSML::IncrementSetting* CreateEnumIncrement(
    const BSML::Lite::TransformWrapper& parent,
    std::string_view name,
    std::vector<std::string> const& enumStrings,
    int value,
    std::function<void(int)> onChange
) {
    auto object = BSML::Lite::CreateIncrementSetting(parent, name, 0, 1, value, 0, enumStrings.size() - 1);
    object->onChange = [onChange, object, enumStrings](float value) {
        onChange((int) value);
        object->text->text = enumStrings[value];
    };
    object->text->text = enumStrings[object->currentValue];
    SetButtons(object);
    return object;
}

static void SetEnumIncrement(BSML::IncrementSetting* increment, std::vector<std::string> const& enumStrings, int value, std::string fallback = "") {
    if (value < -1)
        value = -1;
    else if (value > enumStrings.size())
        value = enumStrings.size();
    increment->currentValue = value;
    increment->text->text = value > 0 && value < enumStrings.size() ? enumStrings[value] : fallback;
    increment->decButton->interactable = value > 0;
    increment->incButton->interactable = value < enumStrings.size() - 1;
}

static std::string GetIP() {
    using namespace System::Net;
    auto host = Dns::GetHostEntry(Dns::GetHostName());
    for (auto ip : host->AddressList) {
        if (ip->AddressFamily == Sockets::AddressFamily::InterNetwork)
            return ip->ToString();
    }
    return "Not Found";
}

static bool init = false;
static TMPro::TextMeshProUGUI* ip;
static HMUI::InputFieldView* port;
static BSML::IncrementSetting* resolution;
static BSML::SliderSetting* bitrate;
static BSML::SliderSetting* fps;
static BSML::SliderSetting* fov;
static BSML::SliderSetting* smoothness;
static BSML::ToggleSetting* mic;
static BSML::SliderSetting* gameVolume;
static BSML::SliderSetting* micVolume;
static BSML::SliderSetting* micThreshold;
static BSML::IncrementSetting* mixMode;

void Config::CreateMenu(HMUI::ViewController* self, bool firstActivation, bool, bool) {
    if (!firstActivation) {
        UpdateMenu();
        return;
    }

    auto settings = BSML::Lite::CreateScrollableSettingsContainer(self);

    ip = BSML::Lite::CreateText(settings, "Local IP:", {0, 0}, {0, 6});

    auto horizontal = BSML::Lite::CreateHorizontalLayoutGroup(settings);
    horizontal->spacing = 2;

    BSML::Lite::CreateText(horizontal, "Port:")->alignment = TMPro::TextAlignmentOptions::MidlineLeft;

    port = BSML::Lite::CreateStringSetting(horizontal, "", getConfig().Port.GetValue(), [](StringW value) {
        std::string str = value;
        try {
            std::stoi(str);
        } catch (...) {
            logger.info("ignoring invalid port input");
            port->text = getConfig().Port.GetValue();
            return;
        }

        getConfig().Port.SetValue(str);
        Socket::Refresh();
    });

    resolution = CreateEnumIncrement(settings, "Resolution", ResolutionStrings, 0, [](int value) {
        if (value >= Resolutions.size())
            return;
        auto const& [width, height] = Resolutions[value];
        getConfig().Width.SetValue(width);
        getConfig().Height.SetValue(height);
        Manager::UpdateSettings();
    });

    bitrate =
        BSML::Lite::CreateSliderSetting(settings, "Bitrate", 1000, getConfig().Bitrate.GetValue(), 1000, 20000, 0.5, true, {0, 0}, [](float value) {
            getConfig().Bitrate.SetValue(value);
            Manager::UpdateSettings();
        });
    bitrate->formatter = [](float value) {
        return fmt::format("{} kbps", (int) value);
    };

    fps = BSML::Lite::CreateSliderSetting(settings, "FPS", 5, getConfig().FPS.GetValue(), 10, 90, 0.5, true, {0, 0}, [](float value) {
        getConfig().FPS.SetValue(value);
        Manager::UpdateSettings();
    });

    fov = BSML::Lite::CreateSliderSetting(settings, "FOV", 1, getConfig().FOV.GetValue(), 50, 100, 0.5, true, {0, 0}, [](float value) {
        getConfig().FOV.SetValue(value);
        Manager::UpdateSettings();
    });

    smoothness =
        BSML::Lite::CreateSliderSetting(settings, "Smoothness", 0.1, getConfig().Smoothing.GetValue(), 0, 2, 0.5, true, {0, 0}, [](float value) {
            getConfig().Smoothing.SetValue(value);
            Manager::UpdateSettings();
        });

    mic = BSML::Lite::CreateToggle(settings, "Enable Mic", getConfig().Mic.GetValue(), [](bool value) {
        getConfig().Mic.SetValue(value);
        Manager::UpdateSettings();
    });

    gameVolume =
        BSML::Lite::CreateSliderSetting(settings, "Game Volume", 0.1, getConfig().GameVolume.GetValue(), 0, 2, 0.5, true, {0, 0}, [](float value) {
            getConfig().GameVolume.SetValue(value);
            Manager::UpdateSettings();
        });

    micVolume =
        BSML::Lite::CreateSliderSetting(settings, "Mic Volume", 0.1, getConfig().MicVolume.GetValue(), 0, 2, 0.5, true, {0, 0}, [](float value) {
            getConfig().MicVolume.SetValue(value);
            Manager::UpdateSettings();
        });

    micThreshold =
        BSML::Lite::CreateSliderSetting(settings, "Mic Threshold", 0.1, getConfig().MicThreshold.GetValue(), 0, 2, 0.5, true, {0, 0}, [](float value) {
            getConfig().MicThreshold.SetValue(value);
            Manager::UpdateSettings();
        });

    mixMode = CreateEnumIncrement(settings, "Mic Mixing", MixModeStrings, 0, [](int value) {
        getConfig().MixMode.SetValue(value);
        Manager::UpdateSettings();
    });

    init = true;
    UpdateMenu();
}

void Config::UpdateMenu() {
    if (!init)
        return;

    ip->text = fmt::format("Local IP: {}", GetIP());
    port->text = getConfig().Port.GetValue();

    int idx = -1;

    int width = getConfig().Width.GetValue();
    int height = getConfig().Height.GetValue();
    for (int i = 0; i < Resolutions.size(); i++) {
        if (Resolutions[i].first == width && Resolutions[i].second == height) {
            idx = i;
            break;
        }
    }
    SetEnumIncrement(resolution, ResolutionStrings, idx, "Custom");

    bitrate->set_Value(getConfig().Bitrate.GetValue());
    fps->set_Value(getConfig().FPS.GetValue());
    fov->set_Value(getConfig().FOV.GetValue());
    smoothness->set_Value(getConfig().Smoothing.GetValue());
    MetaCore::UI::InstantSetToggle(mic, getConfig().Mic.GetValue());
    gameVolume->set_Value(getConfig().GameVolume.GetValue());
    micVolume->set_Value(getConfig().MicVolume.GetValue());
    micThreshold->set_Value(getConfig().MicThreshold.GetValue());
    SetEnumIncrement(mixMode, MixModeStrings, getConfig().MixMode.GetValue(), "Invalid");
}

void Config::Invalidate() {
    init = false;
}
#else
void Config::CreateMenu(HMUI::ViewController* self, bool firstActivation, bool, bool) {}
void Config::UpdateMenu() {}
void Config::Invalidate() {}
#endif
