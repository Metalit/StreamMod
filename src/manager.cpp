#include "manager.hpp"

#include "GlobalNamespace/MainCamera.hpp"
#include "GlobalNamespace/MainCameraCullingMask.hpp"
#include "UnityEngine/AudioListener.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/SpatialTracking/TrackedPoseDriver.hpp"
#include "UnityEngine/StereoTargetEyeMask.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "audio.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "config.hpp"
#include "hollywood/shared/hollywood.hpp"
#include "main.hpp"
#include "math.hpp"
#include "socket.hpp"

static bool initialized = false;

static Hollywood::CameraCapture* cameraStream = nullptr;
static StreamMod::AudioCapture* audioStream = nullptr;

static UnityEngine::Vector3 smoothPosition;
static UnityEngine::Quaternion smoothRotation;

static void MakeCamera(UnityEngine::Camera* main) {
    if (cameraStream)
        return;

    logger.debug("creating camera capture");
    main->gameObject->active = false;
    auto camera = UnityEngine::Object::Instantiate(main);
    camera->tag = "Untagged";
    main->gameObject->active = true;

    UnityEngine::Object::DontDestroyOnLoad(camera->gameObject);

    while (camera->transform->childCount > 0)
        UnityEngine::Object::DestroyImmediate(camera->transform->GetChild(0)->gameObject);

    if (auto comp = camera->GetComponent<GlobalNamespace::MainCameraCullingMask*>())
        UnityEngine::Object::DestroyImmediate(comp);
    if (auto comp = camera->GetComponent<GlobalNamespace::MainCamera*>())
        UnityEngine::Object::DestroyImmediate(comp);
    if (auto comp = camera->GetComponent<UnityEngine::SpatialTracking::TrackedPoseDriver*>())
        UnityEngine::Object::DestroyImmediate(comp);

    camera->clearFlags = main->clearFlags;
    camera->nearClipPlane = main->nearClipPlane;
    camera->farClipPlane = main->farClipPlane;
    camera->backgroundColor = main->backgroundColor;
    camera->hideFlags = main->hideFlags;
    camera->depthTextureMode = main->depthTextureMode;
    camera->cullingMask = main->cullingMask;

    camera->stereoTargetEye = UnityEngine::StereoTargetEyeMask::None;

    cameraStream = camera->gameObject->AddComponent<Hollywood::CameraCapture*>();
    cameraStream->onOutputUnit = [](uint8_t* data, size_t length) {
        static int frameIdx = 0;
        PacketWrapper packet;
        auto& video = *packet.mutable_videoframe();
        *video.mutable_data() = {(char*) data, length};
        Socket::Send(packet);
    };

    smoothPosition = main->transform->position;
    smoothRotation = main->transform->rotation;
    camera->transform->SetPositionAndRotation(smoothPosition, smoothRotation);

    camera->gameObject->active = true;
}

static void RefreshAudio();

static void MakeAudio(UnityEngine::AudioListener* listener) {
    if (audioStream || !listener)
        return;

    logger.debug("creating audio capture");
    audioStream = listener->gameObject->AddComponent<StreamMod::AudioCapture*>();
    audioStream->onDisable = [](StreamMod::AudioCapture*) {
        RefreshAudio();
    };
    audioStream->callback = [](std::vector<float> const& samples, int sampleRate, int channels) {
        PacketWrapper packet;
        auto& audio = *packet.mutable_audioframe();
        audio.set_channels(channels);
        audio.set_samplerate(sampleRate);
        *audio.mutable_data() = {samples.begin(), samples.end()};
        Socket::Send(packet);
    };
    audioStream->SetMicCapture(getConfig().Mic.GetValue());
}

static void RefreshAudio() {
    if (!audioStream)
        return;

    logger.debug("refreshing audio capture");
    audioStream->OnDestroy();
    UnityEngine::Object::DestroyImmediate(audioStream);
    audioStream = nullptr;
    auto listeners = UnityEngine::Resources::FindObjectsOfTypeAll<UnityEngine::AudioListener*>();
    auto listener = listeners->FirstOrDefault([](UnityEngine::AudioListener* l) { return l->isActiveAndEnabled; });
    if (!listener)
        BSML::MainThreadScheduler::ScheduleAfterTime(0.5, RefreshAudio);
    else
        MakeAudio(listener);
}

void Manager::Init() {
    if (initialized)
        return;

    Socket::Init();

    getConfig().Mic.AddChangeEvent([](bool val) {
        if (audioStream)
            audioStream->SetMicCapture(val);
    });

    logger.info("initialized streaming manager");
    initialized = true;
}

void Manager::SetCamera(UnityEngine::Camera* main) {
    MakeCamera(main);
    MakeAudio(main->GetComponent<UnityEngine::AudioListener*>());
}

void Manager::SetFollowLocation(UnityEngine::Vector3 pos, UnityEngine::Quaternion rot) {
    if (!cameraStream)
        return;
    float smoothing = getConfig().Smoothing.GetValue();
    if (smoothing >= 0.1) {
        float deltaTime = UnityEngine::Time::get_deltaTime() * 2 / smoothing;
        smoothPosition = EaseLerp(smoothPosition, pos, UnityEngine::Time::get_time(), deltaTime);
        smoothRotation = Slerp(smoothRotation, rot, deltaTime);
    } else {
        smoothPosition = pos;
        smoothRotation = rot;
    }
    cameraStream->transform->SetPositionAndRotation(smoothPosition, smoothRotation);
}

static void HandleSettings(Settings const& settings, void* source) {
    getConfig().Width.SetValue(settings.horizontal(), false);
    getConfig().Height.SetValue(settings.vertical(), false);
    getConfig().Bitrate.SetValue(settings.bitrate(), false);
    getConfig().FPS.SetValue(settings.fps(), false);
    getConfig().FOV.SetValue(settings.fov(), false);
    getConfig().Smoothing.SetValue(settings.smoothness(), false);
    getConfig().Mic.SetValue(settings.mic(), false);
    getConfig().Save();
    Manager::SendSettings(source);
    Manager::RestartCapture();
    Config::UpdateMenu();
}

void Manager::HandleMessage(PacketWrapper const& packet, void* source) {
    switch (packet.Packet_case()) {
        case PacketWrapper::kSettings:
            HandleSettings(packet.settings(), source);
            break;
        default:
            break;
    }
}

void Manager::SendSettings(void* source) {
    PacketWrapper packet;
    auto& settings = *packet.mutable_settings();
    settings.set_horizontal(getConfig().Width.GetValue());
    settings.set_vertical(getConfig().Height.GetValue());
    settings.set_bitrate(getConfig().Bitrate.GetValue());
    settings.set_fps(getConfig().FPS.GetValue());
    settings.set_fov(getConfig().FOV.GetValue());
    settings.set_smoothness(getConfig().Smoothing.GetValue());
    settings.set_mic(getConfig().Mic.GetValue());
    logger.debug("sending settings except to {}", source);
    Socket::Send(packet, source);
}

void Manager::RestartCapture() {
    if (!cameraStream)
        return;
    logger.debug("refreshing camera capture");
    cameraStream->Stop();
    cameraStream->Init(
        getConfig().Width.GetValue(),
        getConfig().Height.GetValue(),
        getConfig().FPS.GetValue(),
        getConfig().Bitrate.GetValue() * 1000,
        getConfig().FOV.GetValue()
    );
    // nothing to do for audio for now
}
