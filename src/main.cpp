#include "main.hpp"

#include "GlobalNamespace/DeactivateMenuControllersOnFocusCaptureOrTrackingLost.hpp"
#include "GlobalNamespace/MainCamera.hpp"
#include "UnityEngine/SceneManagement/LoadSceneMode.hpp"
#include "UnityEngine/SceneManagement/Scene.hpp"
#include "UnityEngine/SceneManagement/SceneManager.hpp"
#include "UnityEngine/SpatialTracking/TrackedPoseDriver.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "bsml/shared/BSML.hpp"
#include "config.hpp"
#include "custom-types/shared/delegate.hpp"
#include "custom-types/shared/register.hpp"
#include "fpfc.hpp"
#include "hollywood/shared/hollywood.hpp"
#include "hooks.hpp"
#include "manager.hpp"

MAKE_AUTO_HOOK_MATCH(MainCamera_Awake, &GlobalNamespace::MainCamera::Awake, void, GlobalNamespace::MainCamera* self) {
    MainCamera_Awake(self);

    Manager::SetCamera(self->camera);

    Manager::Init();
}

MAKE_AUTO_HOOK_MATCH(
    TrackedPoseDriver_Update, &UnityEngine::SpatialTracking::TrackedPoseDriver::Update, void, UnityEngine::SpatialTracking::TrackedPoseDriver* self
) {
    TrackedPoseDriver_Update(self);

    // seems to be the only one? I would have expected head
    if (self->poseSource == UnityEngine::SpatialTracking::TrackedPoseDriver::TrackedPose::Center)
        Manager::SetFollowLocation(self->transform->position, self->transform->rotation);
}

MAKE_AUTO_HOOK_MATCH(
    DeactivateMenuControllersOnFocusCaptureOrTrackingLost_SetActiveMenuController,
    &GlobalNamespace::DeactivateMenuControllersOnFocusCaptureOrTrackingLost::SetActiveMenuController,
    void,
    GlobalNamespace::DeactivateMenuControllersOnFocusCaptureOrTrackingLost* self,
    bool active,
    GlobalNamespace::VRController* controller
) {
    if (Manager::IsCapturing() && getConfig().FPFC.GetValue())
        active = true;

    DeactivateMenuControllersOnFocusCaptureOrTrackingLost_SetActiveMenuController(self, active, controller);
}

static modloader::ModInfo modInfo = {MOD_ID, VERSION, 0};

extern "C" void setup(CModInfo* info) {
    *info = modInfo.to_c();

    Paper::Logger::RegisterFileContextId(MOD_ID);

    getConfig().Init(modInfo);

    try {
        std::stoi(getConfig().Port.GetValue());
    } catch (...) {
        logger.info("Resetting invalid port");
        getConfig().Port.SetValue(getConfig().Port.GetDefaultValue());
    }

    logger.info("Completed setup!");
}

extern "C" void late_load() {
    il2cpp_functions::Init();

    Hollywood::Init();

    custom_types::Register::AutoRegister();

    BSML::Register::RegisterSettingsMenu("Streamer", &Config::CreateMenu, false);
    BSML::Events::onGameDidRestart.addCallback(&Config::Invalidate);
    BSML::Events::onGameDidRestart.addCallback(&Manager::Invalidate);

    namespace Scenes = UnityEngine::SceneManagement;

    Scenes::SceneManager::add_sceneLoaded(BSML::MakeUnityAction<Scenes::Scene, Scenes::LoadSceneMode>(
        std::function([](Scenes::Scene, Scenes::LoadSceneMode) { FPFC::OnSceneChange(); })
    ));

    Hooks::Install();
}
