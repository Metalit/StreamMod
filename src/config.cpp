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
    std::vector<std::string> const enumStrings,
    int value,
    std::function<void(int)> onChange
) {
    auto object = BSML::Lite::CreateIncrementSetting(parent, name, 0, 1, value, 0, enumStrings.size() - 1);
    object->onChange = [onChange, object, enumStrings](float value) {
        onChange((int) value);
        object->text->set_text(enumStrings[value]);
    };
    object->text->text = enumStrings[object->currentValue];
    SetButtons(object);
    return object;
}

static StringW GetIP() {
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

static void UpdateStream() {
    Manager::SendSettings();
    Manager::RestartCapture();
}

void Config::CreateMenu(HMUI::ViewController* self, bool firstActivation, bool, bool) {
    if (!firstActivation) {
        UpdateMenu();
        return;
    }

    auto vertical = BSML::Lite::CreateVerticalLayoutGroup(self);
    vertical->childControlHeight = false;
    vertical->childForceExpandHeight = false;
    vertical->spacing = 1;

    ip = BSML::Lite::CreateText(vertical, "Local IP", {0, 0}, {0, 9});

    auto horizontal = BSML::Lite::CreateHorizontalLayoutGroup(vertical);

    BSML::Lite::CreateText(horizontal, "Port");

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
        Socket::Refresh(nullptr);
    });

    resolution = CreateEnumIncrement(vertical, "Resolution", resolutionStrings, 0, [](int value) {
        if (value >= resolutions.size())
            return;
        auto const& [width, height] = resolutions[value];
        getConfig().Width.SetValue(width);
        getConfig().Height.SetValue(height);
        UpdateStream();
    });

    bitrate = BSML::Lite::CreateSliderSetting(vertical, "Bitrate", 1000, getConfig().Bitrate.GetValue(), 1000, 20000, 0.5, true, {}, [](float value) {
        getConfig().Bitrate.SetValue(value);
        UpdateStream();
    });
    bitrate->formatter = [](float value) {
        return fmt::format("{} kbps", (int) value);
    };

    fps = BSML::Lite::CreateSliderSetting(vertical, "FPS", 5, getConfig().FPS.GetValue(), 10, 90, 0.5, true, {}, [](float value) {
        getConfig().FPS.SetValue(value);
        UpdateStream();
    });

    fov = BSML::Lite::CreateSliderSetting(vertical, "FOV", 1, getConfig().FOV.GetValue(), 50, 100, 0.5, true, {}, [](float value) {
        getConfig().FOV.SetValue(value);
        UpdateStream();
    });

    smoothness = BSML::Lite::CreateSliderSetting(vertical, "Smoothness", 0.1, getConfig().Smoothing.GetValue(), 0, 2, 0.5, true, {}, [](float value) {
        getConfig().Smoothing.SetValue(value);
        Manager::SendSettings();
    });

    mic = BSML::Lite::CreateToggle(vertical, "Enable Mic", getConfig().Mic.GetValue(), [](bool value) {
        getConfig().Mic.SetValue(value);
        Manager::SendSettings();
    });

    init = true;
    UpdateMenu();
}

void Config::UpdateMenu() {
    if (!init)
        return;

    ip->text = fmt::format("Local IP: {}", GetIP());
    port->text = getConfig().Port.GetValue();

    std::string str = "Custom";
    int idx = -1;

    int width = getConfig().Width.GetValue();
    int height = getConfig().Height.GetValue();
    for (int i = 0; i < resolutions.size(); i++) {
        if (resolutions[i].first == width && resolutions[i].second == height) {
            str = resolutionStrings[i];
            idx = i;
            break;
        }
    }

    resolution->currentValue = idx;
    resolution->text->text = str;
    resolution->decButton->interactable = idx > 0;
    resolution->incButton->interactable = idx < resolutions.size() - 1;

    bitrate->set_Value(getConfig().Bitrate.GetValue());
    fps->set_Value(getConfig().FPS.GetValue());
    fov->set_Value(getConfig().FOV.GetValue());
    smoothness->set_Value(getConfig().Smoothing.GetValue());
    MetaCore::UI::InstantSetToggle(mic, getConfig().Mic.GetValue());
}

void Config::Invalidate() {
    init = false;
}
#else
void Config::CreateMenu(HMUI::ViewController* self, bool firstActivation, bool, bool) {}
void Config::UpdateMenu() {}
void Config::Invalidate() {}
#endif
