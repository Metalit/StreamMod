#include "main.hpp"

#include "GlobalNamespace/MainCamera.hpp"
#include "UnityEngine/SpatialTracking/TrackedPoseDriver.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "config.hpp"
#include "hollywood/shared/hollywood.hpp"
#include "manager.hpp"

MAKE_HOOK_MATCH(MainCamera_Awake, &GlobalNamespace::MainCamera::Awake, void, GlobalNamespace::MainCamera* self) {
    MainCamera_Awake(self);

    Manager::SetCamera(self->camera);

    Manager::Init();
}

MAKE_HOOK_MATCH(
    TrackedPoseDriver_Update, &UnityEngine::SpatialTracking::TrackedPoseDriver::Update, void, UnityEngine::SpatialTracking::TrackedPoseDriver* self
) {
    TrackedPoseDriver_Update(self);

    // seems to be the only one? I would have expected head
    if (self->poseSource == UnityEngine::SpatialTracking::TrackedPoseDriver::TrackedPose::Center)
        Manager::SetFollowLocation(self->transform->position, self->transform->rotation);
}

static modloader::ModInfo modInfo = {MOD_ID, VERSION, 0};

extern "C" void setup(CModInfo* info) {
    *info = modInfo.to_c();

    Paper::Logger::RegisterFileContextId(MOD_ID);

    getConfig().Init(modInfo);

    logger.info("Completed setup!");
}

extern "C" void late_load() {
    il2cpp_functions::Init();

    Hollywood::Init();

    custom_types::Register::AutoRegister();

    INSTALL_HOOK(logger, MainCamera_Awake);
    INSTALL_HOOK(logger, TrackedPoseDriver_Update);
}
