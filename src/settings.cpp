#include "settings.hpp"

#if __has_include("bsml/shared/BSML.hpp")
#include "HMUI/Touchable.hpp"
#include "System/Net/Dns.hpp"
#include "System/Net/IPAddress.hpp"
#include "System/Net/IPHostEntry.hpp"
#include "System/Net/Sockets/AddressFamily.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/Helpers/creation.hpp"
#include "config.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "metacore/shared/ui.hpp"
#include "socket.hpp"

DEFINE_TYPE(StreamMod, MainSettings);
DEFINE_TYPE(StreamMod, VisibilityAudioSettings);
DEFINE_TYPE(StreamMod, PositionSettings);
DEFINE_TYPE(StreamMod, Settings);

using namespace StreamMod;

MainSettings* MainSettings::instance = nullptr;
VisibilityAudioSettings* VisibilityAudioSettings::instance = nullptr;
PositionSettings* PositionSettings::instance = nullptr;

template <class T>
static inline T* LazyCreate(T*& instance, char const* name) {
    if (!instance) {
        instance = BSML::Helpers::CreateViewController<T*>();
        instance->name = name;
        MetaCore::Engine::SetOnDestroy(instance, [&instance]() { instance = nullptr; });
    }
    return instance;
}

template <class O, class T>
static void AddHoverHint(O const& object, ConfigUtils::ConfigValue<T>& configValue) {
    if (!configValue.GetHoverHint().empty())
        BSML::Lite::AddHoverHint(object, configValue.GetHoverHint());
}

static BSML::ToggleSetting*
AddConfigValueToggle(BSML::Lite::TransformWrapper const& parent, ConfigUtils::ConfigValue<bool>& configValue, function_ptr_t<void, void*> update) {
    auto object = BSML::Lite::CreateToggle(parent, configValue.GetName(), configValue.GetValue(), [&configValue, update](bool value) {
        configValue.SetValue(value);
        update(nullptr);
    });
    AddHoverHint(object, configValue);
    return object;
}

static BSML::IncrementSetting* CreateEnumIncrement(
    BSML::Lite::TransformWrapper const& parent,
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

static BSML::IncrementSetting* AddConfigValueIncrementEnum(
    BSML::Lite::TransformWrapper const& parent,
    ConfigUtils::ConfigValue<int>& configValue,
    std::vector<std::string> const& enumStrings,
    function_ptr_t<void, void*> update
) {
    auto object = CreateEnumIncrement(parent, configValue.GetName(), enumStrings, configValue.GetValue(), [&configValue, update](int value) {
        configValue.SetValue(value);
        update(nullptr);
    });
    AddHoverHint(object, configValue);
    return object;
}

static BSML::IncrementSetting* AddConfigValueIncrementFloat(
    BSML::Lite::TransformWrapper const& parent,
    ConfigUtils::ConfigValue<float>& configValue,
    int decimals,
    float increment,
    float min,
    float max,
    function_ptr_t<void, void*> update
) {
    auto object = BSML::Lite::CreateIncrementSetting(
        parent, configValue.GetName(), decimals, increment, configValue.GetValue(), min, max, [&configValue, update](float value) {
            configValue.SetValue(value);
            update(nullptr);
        }
    );
    SetButtons(object);
    AddHoverHint(object, configValue);
    return object;
}

template <class V>
requires(std::is_convertible_v<V, float>)
static BSML::SliderSetting* AddConfigValueSliderIncrement(
    BSML::Lite::TransformWrapper const& parent,
    ConfigUtils::ConfigValue<V>& configValue,
    float increment,
    float min,
    float max,
    float applyTime,
    function_ptr_t<void, void*> update
) {
    auto object = BSML::Lite::CreateSliderSetting(
        parent, configValue.GetName(), increment, configValue.GetValue(), min, max, applyTime, true, {}, [&configValue, update](float value) {
            configValue.SetValue(value);
            update(nullptr);
        }
    );
    AddHoverHint(object, configValue);
    return object;
}

inline std::array<BSML::IncrementSetting*, 3> AddConfigValueIncrementVector3(
    BSML::Lite::TransformWrapper const& parent,
    ConfigUtils::ConfigValue<UnityEngine::Vector3>& configValue,
    int decimals,
    float increment,
    float bigIncrement,
    function_ptr_t<void, void*> update
) {
    auto x = BSML::Lite::CreateIncrementSetting(
        parent, configValue.GetName() + " X", decimals, increment, configValue.GetValue().x, [&configValue, update](float value) {
            auto newValue = configValue.GetValue();
            newValue.x = value;
            configValue.SetValue(newValue);
            update(nullptr);
        }
    );
    MetaCore::UI::AddIncrementIncrement(x, bigIncrement);
    AddHoverHint(x, configValue);
    auto y = BSML::Lite::CreateIncrementSetting(
        parent, configValue.GetName() + " Y", decimals, increment, configValue.GetValue().y, [&configValue, update](float value) {
            auto newValue = configValue.GetValue();
            newValue.y = value;
            configValue.SetValue(newValue);
            update(nullptr);
        }
    );
    MetaCore::UI::AddIncrementIncrement(y, bigIncrement);
    AddHoverHint(y, configValue);
    auto z = BSML::Lite::CreateIncrementSetting(
        parent, configValue.GetName() + " Z", decimals, increment, configValue.GetValue().z, [&configValue, update](float value) {
            auto newValue = configValue.GetValue();
            newValue.z = value;
            configValue.SetValue(newValue);
            update(nullptr);
        }
    );
    MetaCore::UI::AddIncrementIncrement(z, bigIncrement);
    AddHoverHint(z, configValue);
    return {x, y, z};
}

static void SetEnumIncrement(BSML::IncrementSetting* increment, std::vector<std::string> const& enumStrings, int value, std::string fallback = "") {
    if (value < -1)
        value = -1;
    else if (value > enumStrings.size())
        value = enumStrings.size();
    increment->currentValue = value;
    increment->text->text = value >= 0 && value < enumStrings.size() ? enumStrings[value] : fallback;
    increment->decButton->interactable = value > 0;
    increment->incButton->interactable = value < enumStrings.size() - 1;
}

static inline UnityEngine::Transform* MakeVertical(BSML::Lite::TransformWrapper parent) {
    auto vertical = BSML::Lite::CreateVerticalLayoutGroup(parent);
    vertical->childControlHeight = false;
    vertical->childForceExpandHeight = false;
    vertical->spacing = 1;
    return vertical->transform;
}

static inline UnityEngine::Transform* MakeLayout(HMUI::ViewController* self, float verticalOffset = 0) {
    self->gameObject->AddComponent<HMUI::Touchable*>();
    auto ret = MakeVertical(self);
    if (verticalOffset != 0)
        self->rectTransform->sizeDelta = {0, verticalOffset * 2};
    return ret;
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

static int GetResolutionIndex() {
    int width = getConfig().Width.GetValue();
    int height = getConfig().Height.GetValue();
    for (int i = 0; i < Resolutions.size(); i++) {
        if (Resolutions[i].first == width && Resolutions[i].second == height)
            return i;
    }
    return -1;
}

void MainSettings::DidActivate(bool firstActivation, bool, bool) {
    std::string ipText = fmt::format("Local IP: {}", GetIP());

    if (!firstActivation) {
        ip->text = ipText;
        return;
    }

    auto layout = MakeLayout(this);

    ip = BSML::Lite::CreateText(layout, ipText, {0, 0}, {0, 6});

    auto horizontal = BSML::Lite::CreateHorizontalLayoutGroup(layout);
    horizontal->spacing = 2;
    BSML::Lite::CreateText(horizontal, "Port:")->alignment = TMPro::TextAlignmentOptions::MidlineLeft;
    port = BSML::Lite::CreateStringSetting(horizontal, "", getConfig().Port.GetValue(), [this](StringW value) {
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

    preview = BSML::Lite::CreateToggle(layout, getConfig().Preview.GetName(), getConfig().Preview.GetValue(), [](bool value) {
        getConfig().Preview.SetValue(value);
        // Toggle preview
    });
    AddHoverHint(preview, getConfig().Preview);

    resolution = CreateEnumIncrement(layout, "Resolution", ResolutionStrings, GetResolutionIndex(), [](int value) {
        auto const& [width, height] = Resolutions[value];
        getConfig().Width.SetValue(width);
        getConfig().Height.SetValue(height);
        Manager::UpdateVideoSettings();
    });

    bitrate = AddConfigValueSliderIncrement(layout, getConfig().Bitrate, 1000, 1000, 20000, 0.5, Manager::UpdateVideoSettings);
    bitrate->formatter = [](float value) {
        return fmt::format("{} kbps", (int) value);
    };
    fps = AddConfigValueSliderIncrement(layout, getConfig().FPS, 5, 10, 90, 0.1, Manager::UpdateCameraSettings);
    fov = AddConfigValueSliderIncrement(layout, getConfig().FOV, 1, 40, 90, 0.1, Manager::UpdateCameraSettings);
    orthographic = AddConfigValueToggle(layout, getConfig().Orthographic, Manager::UpdateCameraSettings);
}

void MainSettings::UpdateMenu() {
    if (!wasActivatedBefore)
        return;

    port->text = getConfig().Port.GetValue();
    SetEnumIncrement(resolution, ResolutionStrings, GetResolutionIndex(), "Custom");
    bitrate->set_Value(getConfig().Bitrate.GetValue());
    fps->set_Value(getConfig().FPS.GetValue());
    fov->set_Value(getConfig().FOV.GetValue());
    MetaCore::UI::InstantSetToggle(orthographic, getConfig().Orthographic.GetValue());
}

MainSettings* MainSettings::GetInstance() {
    return LazyCreate(instance, "StreamModMainSettings");
}

void VisibilityAudioSettings::DidActivate(bool firstActivation, bool, bool) {
    if (!firstActivation)
        return;

    auto layout = MakeLayout(this);
    auto audio = MakeVertical(layout);
    auto visibility = MakeVertical(layout);
    MetaCore::UI::CreateTextSegmentedControl(layout, {"Audio", "Visibility"}, [audio, visibility](int selected) {
        audio->gameObject->active = selected == 0;
        visibility->gameObject->active = selected == 1;
    })->transform->SetAsFirstSibling();

    gameVolume = AddConfigValueSliderIncrement(audio, getConfig().GameVolume, 0.1, 0, 2, 0, Manager::UpdateAudioSettings);
    mic = AddConfigValueToggle(audio, getConfig().Mic, Manager::UpdateAudioSettings);
    micVolume = AddConfigValueSliderIncrement(audio, getConfig().MicVolume, 0.1, 0, 2, 0, Manager::UpdateAudioSettings);
    micThreshold = AddConfigValueSliderIncrement(audio, getConfig().MicThreshold, 0.1, 0, 2, 0, Manager::UpdateAudioSettings);
    mixMode = AddConfigValueIncrementEnum(audio, getConfig().MixMode, MixModeStrings, Manager::UpdateAudioSettings);

    noteVis = AddConfigValueIncrementEnum(visibility, getConfig().NoteVisibility, NoteVisibilityStrings, Manager::UpdateCameraSettings);
    wallVis = AddConfigValueIncrementEnum(visibility, getConfig().WallVisibility, WallVisibilityStrings, Manager::UpdateCameraSettings);
    avatarVis = AddConfigValueToggle(visibility, getConfig().AvatarVisibility, Manager::UpdateCameraSettings);
    uiVis = AddConfigValueToggle(visibility, getConfig().UIVisibility, Manager::UpdateCameraSettings);
    cutParticleVis = AddConfigValueToggle(visibility, getConfig().CutParticleVisibility, Manager::UpdateCameraSettings);

    visibility->gameObject->active = false;
}

void VisibilityAudioSettings::UpdateMenu() {
    if (!wasActivatedBefore)
        return;

    gameVolume->set_Value(getConfig().GameVolume.GetValue());
    MetaCore::UI::InstantSetToggle(mic, getConfig().Mic.GetValue());
    micVolume->set_Value(getConfig().MicVolume.GetValue());
    micThreshold->set_Value(getConfig().MicThreshold.GetValue());
    SetEnumIncrement(mixMode, MixModeStrings, getConfig().MixMode.GetValue(), "Invalid");

    SetEnumIncrement(noteVis, NoteVisibilityStrings, getConfig().NoteVisibility.GetValue(), "Invalid");
    SetEnumIncrement(wallVis, WallVisibilityStrings, getConfig().WallVisibility.GetValue(), "Invalid");
    MetaCore::UI::InstantSetToggle(avatarVis, getConfig().AvatarVisibility.GetValue());
    MetaCore::UI::InstantSetToggle(uiVis, getConfig().UIVisibility.GetValue());
    MetaCore::UI::InstantSetToggle(cutParticleVis, getConfig().CutParticleVisibility.GetValue());
}

VisibilityAudioSettings* VisibilityAudioSettings::GetInstance() {
    return LazyCreate(instance, "StreamModVisibilityAudioSettings");
}

void PositionSettings::DidActivate(bool firstActivation, bool, bool) {
    if (!firstActivation)
        return;

    auto layout = MakeLayout(this);

    cameraMode = AddConfigValueIncrementEnum(layout, getConfig().CameraMode, CameraModeStrings, [](void*) {
        Manager::UpdateCameraSettings();
        instance->SetIncrementsActive();
    });
    smoothness = AddConfigValueSliderIncrement(layout, getConfig().Smoothing, 0.1, 0, 2, 0, Manager::UpdateCameraSettings);
    smoothPosition = AddConfigValueIncrementVector3(layout, getConfig().SmoothCameraPos, 2, 0.01, 0.1, Manager::UpdateCameraSettings);
    smoothRotation = AddConfigValueIncrementVector3(layout, getConfig().SmoothCameraRot, 0, 1, 10, Manager::UpdateCameraSettings);
    position = AddConfigValueIncrementVector3(layout, getConfig().CameraPos, 2, 0.05, 0.5, Manager::UpdateCameraSettings);
    rotation = AddConfigValueIncrementVector3(layout, getConfig().CameraRot, 0, 1, 10, Manager::UpdateCameraSettings);

    SetIncrementsActive();
}

void PositionSettings::SetIncrementsActive() {
    if (!wasActivatedBefore)
        return;

    int mode = getConfig().CameraMode.GetValue();

    for (auto& setting : smoothPosition)
        setting->gameObject->active = mode == 0;
    for (auto& setting : smoothRotation)
        setting->gameObject->active = mode == 0;
    for (auto& setting : position)
        setting->gameObject->active = mode == 1;
    for (auto& setting : rotation)
        setting->gameObject->active = mode == 1;
}

void PositionSettings::UpdateMenu() {
    if (!wasActivatedBefore)
        return;

    SetEnumIncrement(cameraMode, CameraModeStrings, getConfig().CameraMode.GetValue(), "Invalid");
    smoothness->set_Value(getConfig().Smoothing.GetValue());
    auto smoothPos = getConfig().SmoothCameraPos.GetValue();
    smoothPosition[0]->set_Value(smoothPos.x);
    smoothPosition[1]->set_Value(smoothPos.y);
    smoothPosition[2]->set_Value(smoothPos.z);
    auto smoothRot = getConfig().SmoothCameraRot.GetValue();
    smoothRotation[0]->set_Value(smoothRot.x);
    smoothRotation[1]->set_Value(smoothRot.y);
    smoothRotation[2]->set_Value(smoothRot.z);
    auto pos = getConfig().CameraPos.GetValue();
    position[0]->set_Value(pos.x);
    position[1]->set_Value(pos.y);
    position[2]->set_Value(pos.z);
    auto rot = getConfig().CameraRot.GetValue();
    rotation[0]->set_Value(rot.x);
    rotation[1]->set_Value(rot.y);
    rotation[2]->set_Value(rot.z);
    SetIncrementsActive();
}

PositionSettings* PositionSettings::GetInstance() {
    return LazyCreate(instance, "StreamModPositionSettings");
}

void Settings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (!firstActivation)
        return;

    showBackButton = true;
    static ConstString title("Stream Settings");
    SetTitle(title, HMUI::ViewController::AnimationType::In);

    ProvideInitialViewControllers(
        MainSettings::GetInstance(), VisibilityAudioSettings::GetInstance(), PositionSettings::GetInstance(), nullptr, nullptr
    );
}

void Settings::BackButtonWasPressed(HMUI::ViewController* topViewController) {
    _parentFlowCoordinator->DismissFlowCoordinator(this, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
}

void Settings::UpdateMenus() {
    MainSettings::GetInstance()->UpdateMenu();
    VisibilityAudioSettings::GetInstance()->UpdateMenu();
    PositionSettings::GetInstance()->UpdateMenu();
}

void UpdateSettings() {
    Settings::UpdateMenus();
}
#endif
