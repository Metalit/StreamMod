#include "manager.hpp"

#include "GlobalNamespace/MainCamera.hpp"
#include "GlobalNamespace/MainCameraCullingMask.hpp"
#include "UnityEngine/AudioListener.hpp"
#include "UnityEngine/CameraClearFlags.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/SpatialTracking/TrackedPoseDriver.hpp"
#include "UnityEngine/StereoTargetEyeMask.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "audio.hpp"
#include "camera-utils/shared/CamerasManager.hpp"
#include "camera-utils/shared/VisibilityLayer.hpp"
#include "config.hpp"
#include "fpfc.hpp"
#include "hollywood/shared/hollywood.hpp"
#include "main.hpp"
#include "math.hpp"
#include "metacore/shared/input.hpp"
#include "metacore/shared/unity.hpp"
#include "settings.hpp"
#include "socket.hpp"

static bool initialized = false;
static int sampleRate = -1;
static int channels = -1;
static uint64_t audioTime = 0;

static Hollywood::CameraCapture* cameraStream = nullptr;
static StreamMod::AudioCapture* audioStream = nullptr;
static bool waiting = false;
static bool capturing = false;

static UnityEngine::Vector3 smoothPosition;
static UnityEngine::Quaternion smoothRotation;
static UnityEngine::Vector3 positionOffset;
static UnityEngine::Quaternion rotationOffset;

static int ApplyEnumMask(int mask, int value, int layer, int extra) {
    switch (value) {
        case 1:
            return mask & ~(layer | extra);
        case 2:
            return (mask | layer) & ~extra;
        default:
            return mask | layer | extra;
    }
}

static int ApplyBoolMask(int mask, bool value, int layer) {
    if (value)
        return mask | layer;
    return mask & ~layer;
}

static void SendVideoFrame(uint8_t* data, size_t length) {
    PacketWrapper packet;
    auto& video = *packet.mutable_videoframe();
    *video.mutable_data() = {(char*) data, length};
    video.set_time(Time());
    Socket::Send(packet);
}

static void CheckUpdateAudioConfig(int inSampleRate, int inChannels) {
    if (channels == inChannels)
        return;
    sampleRate = inSampleRate;
    channels = inChannels;
    Manager::UpdateAudioConfig();
}

static void SendAudioFrame(std::span<float> samples) {
    if (!audioTime)
        audioTime = Time();
    PacketWrapper packet;
    auto& audio = *packet.mutable_audioframe();
    audio.set_time(audioTime);
    *audio.mutable_data() = {samples.begin(), samples.end()};
    Socket::Send(packet);
    // What happens if we miss audio? Can we avoid it happening?
    audioTime += AudioTimeIncrease(samples.size(), sampleRate, channels);
}

static void UpdateCamera();

static void MakeCamera(UnityEngine::Camera* main) {
    if (cameraStream)
        return;

    logger.debug("creating camera capture");
    main->gameObject->active = false;
    auto camera = UnityEngine::Object::Instantiate(main);
    camera->name = "StreamingCamera";
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
    if (auto comp = camera->GetComponent<UnityEngine::AudioListener*>())
        UnityEngine::Object::DestroyImmediate(comp);

    camera->stereoTargetEye = UnityEngine::StereoTargetEyeMask::None;
    camera->clearFlags = UnityEngine::CameraClearFlags::SolidColor;
    camera->backgroundColor = {0, 0, 0, 0};
    camera->farClipPlane = 5000;
    camera->nearClipPlane = main->nearClipPlane;
    camera->hideFlags = main->hideFlags;
    camera->depthTextureMode = main->depthTextureMode;

    cameraStream = camera->gameObject->AddComponent<Hollywood::CameraCapture*>();
    cameraStream->onOutputUnit = SendVideoFrame;

    CameraUtils::CamerasManager::RegisterDesktopCamera(camera);

    UpdateCamera();
    camera->gameObject->active = true;
}

static void RefreshAudio();
static void UpdateMic();

static void MakeAudio(UnityEngine::AudioListener* listener) {
    if (audioStream || !listener)
        return;

    logger.debug("creating audio capture");
    audioStream = listener->gameObject->AddComponent<StreamMod::AudioCapture*>();
    audioStream->onDisable = [](StreamMod::AudioCapture*) {
        MetaCore::Engine::ScheduleMainThread(RefreshAudio);  // since this destroys the component, we can't do it in the OnDisable callback
    };
    audioStream->callback = [](std::span<float> samples, int inSampleRate, int inChannels) {
        CheckUpdateAudioConfig(inSampleRate, inChannels);
        SendAudioFrame(samples);
    };
    UpdateMic();
}

static void StopAudio() {
    if (!audioStream)
        return;
    logger.debug("stopping audio capture");
    audioStream->OnDestroy();
    UnityEngine::Object::DestroyImmediate(audioStream);
    audioStream = nullptr;
    audioTime = 0;
}

static void RefreshAudio() {
    logger.debug("refreshing audio capture");
    StopAudio();
    auto listeners = UnityEngine::Resources::FindObjectsOfTypeAll<UnityEngine::AudioListener*>();
    auto listener = listeners->FirstOrDefault([](UnityEngine::AudioListener* l) { return l->isActiveAndEnabled; });
    MakeAudio(listener);
}

static void UpdateCamera() {
    UpdateMic();
    int mode = getConfig().CameraMode.GetValue();
    if (mode == 2)
        FPFC::GetControllers();
    else
        FPFC::ReleaseControllers();
    if (!cameraStream)
        return;
    auto camera = cameraStream->GetComponent<UnityEngine::Camera*>();
    if (mode == 0) {
        positionOffset = getConfig().SmoothCameraPos.GetValue();
        rotationOffset = UnityEngine::Quaternion::Euler(getConfig().SmoothCameraRot.GetValue());
        auto pose = MetaCore::Input::GetHeadPose();
        smoothPosition = pose.position + positionOffset;
        smoothRotation = pose.rotation;
    } else if (mode == 1) {
        positionOffset = getConfig().CameraPos.GetValue();
        rotationOffset = UnityEngine::Quaternion::Euler(getConfig().CameraRot.GetValue());
        cameraStream->transform->SetPositionAndRotation(positionOffset, rotationOffset);
    }
    int mask = camera->cullingMask;
    mask = ApplyEnumMask(mask, getConfig().NoteVisibility.GetValue(), CameraUtils::VL_Note, CameraUtils::VL_NoteDebris);
    // VL_FixMRAlpha is also used for wall cores in Camera2
    // todo: apply this layer to wall cores
    mask = ApplyEnumMask(mask, getConfig().WallVisibility.GetValue(), CameraUtils::VL_Obstacle, CameraUtils::VL_FixMRAlpha);
    mask = ApplyBoolMask(mask, getConfig().AvatarVisibility.GetValue(), CameraUtils::VL_Avatar);
    mask = ApplyBoolMask(mask, getConfig().UIVisibility.GetValue(), CameraUtils::VL_UI);
    mask = ApplyBoolMask(mask, getConfig().CutParticleVisibility.GetValue(), CameraUtils::VL_CutEffectParticles);
    camera->cullingMask = mask;
    camera->fieldOfView = getConfig().FOV.GetValue();
    camera->orthographic = getConfig().Orthographic.GetValue();
}

static void UpdateMic() {
    if (audioStream)
        audioStream->SetMicCapture(getConfig().Mic.GetValue() && getConfig().CameraMode.GetValue() != 2);
}

void Manager::Init() {
    if (initialized)
        return;

    Socket::Init();
    Socket::Start();

    logger.info("initialized streaming manager");
    initialized = true;
}

void Manager::Update() {
    if (!cameraStream)
        return;
    switch (getConfig().CameraMode.GetValue()) {
        case 1:
            break;
        case 2:
            cameraStream->transform->rotation = FPFC::GetRotation();
            cameraStream->transform->Translate(FPFC::GetMovement());
            FPFC::MoveControllers(cameraStream->transform);
            break;
        default: {
            auto pose = MetaCore::Input::GetHeadPose();
            float smoothing = getConfig().Smoothing.GetValue();
            if (smoothing >= 0.01) {
                float deltaTime = UnityEngine::Time::get_deltaTime() * 2 / smoothing;
                smoothRotation = Slerp(smoothRotation, pose.rotation, deltaTime);
                smoothPosition = EaseLerp(smoothPosition, pose.position, UnityEngine::Time::get_time(), deltaTime);
            } else {
                smoothRotation = pose.rotation;
                smoothPosition = pose.position;
            }
            auto rotation = smoothRotation * rotationOffset;
            cameraStream->transform->SetPositionAndRotation(smoothPosition + rotation * positionOffset, rotation);
            break;
        }
    }
}

void Manager::Invalidate() {
    if (cameraStream)
        UnityEngine::Object::DestroyImmediate(cameraStream->gameObject);
    cameraStream = nullptr;
    if (audioStream)
        UnityEngine::Object::DestroyImmediate(audioStream);
    audioStream = nullptr;
    audioTime = 0;
    waiting = capturing;
    capturing = false;
}

void Manager::SetCamera(UnityEngine::Camera* main) {
    MakeCamera(main);
    if (waiting) {
        MakeAudio(main->GetComponent<UnityEngine::AudioListener*>());
        RestartCapture();
        waiting = false;
    }
}

static void HandleVideoSettings(VideoSettings const& settings, void* source) {
    auto& config = getConfig();
    config.Width.SetValue(settings.horizontal(), false);
    config.Height.SetValue(settings.vertical(), false);
    config.Bitrate.SetValue(settings.bitrate(), false);
    config.Save();
    Manager::UpdateVideoSettings(source);
    UpdateSettings();
}

static void HandleAudioSettings(AudioSettings const& settings, void* source) {
    auto& config = getConfig();
    config.GameVolume.SetValue(settings.gamevolume(), false);
    config.Mic.SetValue(settings.mic(), false);
    config.MicVolume.SetValue(settings.micvolume(), false);
    config.MicThreshold.SetValue(settings.micthreshold(), false);
    config.MixMode.SetValue(settings.micmix(), false);
    config.Save();
    Manager::UpdateAudioSettings(source);
    UpdateSettings();
}

static void HandleCameraSettings(CameraSettings const& settings, void* source) {
    auto& config = getConfig();
    auto const& position = settings.position();
    config.CameraMode.SetValue(position.cameramode(), false);
    auto const& pos = position.position();
    config.CameraPos.SetValue({pos.x(), pos.y(), pos.z()}, false);
    auto const& rot = position.rotation();
    config.CameraRot.SetValue({rot.x(), rot.y(), rot.z()}, false);
    auto const& offset = position.offset();
    config.SmoothCameraPos.SetValue({offset.x(), offset.y(), offset.z()}, false);
    auto const& rotOffset = position.rotationoffset();
    config.SmoothCameraRot.SetValue({rotOffset.x(), rotOffset.y(), rotOffset.z()}, false);
    config.Smoothing.SetValue(position.smoothness(), false);
    auto const& visibility = settings.visibility();
    config.NoteVisibility.SetValue(visibility.notes(), false);
    config.WallVisibility.SetValue(visibility.walls(), false);
    config.AvatarVisibility.SetValue(visibility.avatar(), false);
    config.UIVisibility.SetValue(visibility.ui(), false);
    config.CutParticleVisibility.SetValue(visibility.cutparticles(), false);
    config.FPS.SetValue(settings.fps(), false);
    config.FOV.SetValue(settings.fov(), false);
    config.Orthographic.SetValue(settings.orthographic(), false);
    config.Save();
    Manager::UpdateCameraSettings(source);
    UpdateSettings();
}

static void HandleInput(Input const& input) {
    if (getConfig().CameraMode.GetValue() != 2)
        return;
    FPFC::MouseMove({input.dx(), input.dy()});
    if (input.mousedown())
        FPFC::MouseDown();
    if (input.mouseup())
        FPFC::MouseUp();
    FPFC::AddScroll(input.scroll());
    for (auto& key : input.keysdown())
        FPFC::KeyDown(key);
    for (auto& key : input.keysup())
        FPFC::KeyUp(key);
}

void Manager::HandleMessage(PacketWrapper const& packet, void* source) {
    switch (packet.Packet_case()) {
        case PacketWrapper::kVideoSettings:
            HandleVideoSettings(packet.videosettings(), source);
            break;
        case PacketWrapper::kAudioSettings:
            HandleAudioSettings(packet.audiosettings(), source);
            break;
        case PacketWrapper::kCameraSettings:
            HandleCameraSettings(packet.camerasettings(), source);
            break;
        case PacketWrapper::kInput:
            HandleInput(packet.input());
            break;
        default:
            logger.warn("got unexpected packet case {}", (int) packet.Packet_case());
            logger.debug("packet: {}", packet.DebugString());
            break;
    }
}

void Manager::UpdateVideoSettings(void* source) {
    auto& config = getConfig();
    PacketWrapper packet;
    auto& settings = *packet.mutable_videosettings();
    settings.set_horizontal(config.Width.GetValue());
    settings.set_vertical(config.Height.GetValue());
    settings.set_bitrate(config.Bitrate.GetValue());
    logger.debug("sending video settings except to {}", source);
    Socket::Send(packet, source);
    RestartCapture();
}

void Manager::UpdateAudioSettings(void* source) {
    auto& config = getConfig();
    PacketWrapper packet;
    auto& settings = *packet.mutable_audiosettings();
    settings.set_gamevolume(config.GameVolume.GetValue());
    settings.set_mic(config.Mic.GetValue());
    settings.set_micvolume(config.MicVolume.GetValue());
    settings.set_micthreshold(config.MicThreshold.GetValue());
    settings.set_micmix((AudioSettings::MicMix) config.MixMode.GetValue());
    logger.debug("sending audio settings except to {}", source);
    Socket::Send(packet, source);
    UpdateMic();
}

void Manager::UpdateCameraSettings(void* source) {
    auto& config = getConfig();
    PacketWrapper packet;
    auto& settings = *packet.mutable_camerasettings();
    auto& position = *settings.mutable_position();
    position.set_cameramode((CameraSettings::Position::CameraMode) config.CameraMode.GetValue());
    auto& pos = *position.mutable_position();
    auto posVec = config.CameraPos.GetValue();
    pos.set_x(posVec.x);
    pos.set_y(posVec.y);
    pos.set_z(posVec.z);
    auto& rot = *position.mutable_rotation();
    auto rotVec = config.CameraRot.GetValue();
    rot.set_x(rotVec.x);
    rot.set_y(rotVec.y);
    rot.set_z(rotVec.z);
    auto& offset = *position.mutable_offset();
    auto smoothPosVec = config.SmoothCameraPos.GetValue();
    offset.set_x(smoothPosVec.x);
    offset.set_y(smoothPosVec.y);
    offset.set_z(smoothPosVec.z);
    auto& rotOffset = *position.mutable_rotationoffset();
    auto smoothRotVec = config.SmoothCameraRot.GetValue();
    rotOffset.set_x(smoothRotVec.x);
    rotOffset.set_y(smoothRotVec.y);
    rotOffset.set_z(smoothRotVec.z);
    position.set_smoothness(config.Smoothing.GetValue());
    auto& visibility = *settings.mutable_visibility();
    visibility.set_notes((CameraSettings::Visibility::ShowMode) config.NoteVisibility.GetValue());
    visibility.set_walls((CameraSettings::Visibility::ShowMode) config.WallVisibility.GetValue());
    visibility.set_avatar((CameraSettings::Visibility::ShowMode) config.AvatarVisibility.GetValue());
    visibility.set_ui(config.UIVisibility.GetValue());
    visibility.set_cutparticles(config.CutParticleVisibility.GetValue());
    settings.set_fps(config.FPS.GetValue());
    settings.set_fov(config.FOV.GetValue());
    settings.set_orthographic(config.Orthographic.GetValue());
    logger.debug("sending camera settings except to {}", source);
    Socket::Send(packet, source);
    UpdateCamera();
}

void Manager::UpdateAudioConfig() {
    if (channels < 0)
        return;
    PacketWrapper packet;
    auto& audio = *packet.mutable_audioconfig();
    audio.set_samplerate(sampleRate);
    audio.set_channels(channels);
    Socket::Send(packet);
}

bool Manager::IsCapturing() {
    if (waiting)
        return true;
    if (!cameraStream)
        return false;
    return capturing;
}

void Manager::RestartCapture() {
    if (!cameraStream) {
        waiting = true;
        return;
    }
    logger.info("refreshing capture");
    cameraStream->Stop();
    cameraStream->Init(
        getConfig().Width.GetValue(),
        getConfig().Height.GetValue(),
        getConfig().FPS.GetValue(),
        getConfig().Bitrate.GetValue() * 1000,
        getConfig().FOV.GetValue()
    );
    if (!audioStream)
        RefreshAudio();
    FPFC::GetControllers();
    waiting = false;
    capturing = true;
}

void Manager::StopCapture() {
    logger.info("stopping capture");
    if (cameraStream)
        cameraStream->Stop();
    StopAudio();
    FPFC::ReleaseControllers();
    waiting = false;
    capturing = false;
}
