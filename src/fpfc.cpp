#include "fpfc.hpp"

#include "GlobalNamespace/FirstPersonFlyingController.hpp"
#include "GlobalNamespace/OculusVRHelper.hpp"
#include "GlobalNamespace/PauseController.hpp"
#include "GlobalNamespace/PauseMenuManager.hpp"
#include "GlobalNamespace/UIKeyboardManager.hpp"
#include "GlobalNamespace/VRCenterAdjust.hpp"
#include "GlobalNamespace/VRController.hpp"
#include "GlobalNamespace/VRPlatformUtils.hpp"
#include "HMUI/UIKeyboard.hpp"
#include "System/Action.hpp"
#include "System/Action_1.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Input.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "VRUIControls/ButtonState.hpp"
#include "VRUIControls/MouseButtonEventData.hpp"
#include "VRUIControls/MouseState.hpp"
#include "VRUIControls/VRInputModule.hpp"
#include "config.hpp"
#include "hooks.hpp"
#include "main.hpp"
#include "manager.hpp"

using namespace GlobalNamespace;
using namespace UnityEngine;

static VRController* controller0 = nullptr;
static VRController* controller1 = nullptr;
static bool click = false;
static bool clickedLastFrame = false;
static float scroll = 0;
static UnityEngine::Vector3 movementF;
static UnityEngine::Vector3 movementB;
static UnityEngine::Quaternion rotation;
static HMUI::UIKeyboard* keyboardOpen;

static inline bool CapturingFPFC() {
    return Manager::IsCapturing() && getConfig().FPFC.GetValue();
}

static GameObject* GetPauseMenu() {
    if (auto ret = GameObject::Find("PauseMenu"))
        return ret;
    else if (auto ret = GameObject::Find("TutorialPauseMenu"))
        return ret;
    else if (auto ret = GameObject::Find("MultiplayerLocalActivePlayerInGameMenuViewController"))
        return ret;
    else
        return nullptr;
}

void FPFC::GetControllers() {
    if (!CapturingFPFC())
        return;

    controller0 = nullptr;
    controller1 = nullptr;

    // in level
    if (auto pauseMenu = GetPauseMenu()) {
        // can't just search for "MenuControllers" because there are two, we need
        // the one that's a child of the pause menu
        auto transform = pauseMenu->transform->Find("MenuControllers");
        controller0 = transform->Find("ControllerLeft")->GetComponent<VRController*>();
        controller1 = transform->Find("ControllerRight")->GetComponent<VRController*>();
        // in main menu
    } else if (auto objectsSource = Object::FindObjectOfType<FirstPersonFlyingController*>()) {
        objectsSource->_centerAdjust->enabled = false;
        for (auto gameObject : objectsSource->_controllerModels) {
            if (gameObject)
                gameObject->active = false;
        }
        controller0 = objectsSource->_controller0;
        controller1 = objectsSource->_controller1;
    }

    if (controller0 && controller1) {
        for (auto& controller : {controller0, controller1}) {
            controller->mouseMode = true;
            controller->enabled = false;
            if (auto pointer = controller->transform->Find("VRLaserPointer(Clone)"))
                pointer->gameObject->active = false;
            if (auto pointer = controller->transform->Find("MenuHandle"))
                pointer->gameObject->active = false;
        }
        logger.info("got menu controllers");
    } else
        logger.warn("failed to get menu controllers");
}

static void ReleaseCurrentControllers() {
    if (!controller0 || !controller1)
        return;
    for (auto& controller : {controller0, controller1}) {
        controller->mouseMode = false;
        controller->enabled = true;
        if (auto pointer = controller->transform->Find("VRLaserPointer(Clone)"))
            pointer->gameObject->active = true;
        if (auto pointer = controller->transform->Find("MenuHandle"))
            pointer->gameObject->active = true;
    }
    logger.info("released menu controllers");
}

void FPFC::ReleaseControllers() {
    logger.info("releasing menu controllers");

    if (auto pauseMenu = GetPauseMenu()) {
        auto transform = pauseMenu->transform->Find("MenuControllers");
        controller0 = transform->Find("ControllerLeft")->GetComponent<VRController*>();
        controller1 = transform->Find("ControllerRight")->GetComponent<VRController*>();
        ReleaseCurrentControllers();
    }
    if (auto objectsSource = Object::FindObjectOfType<FirstPersonFlyingController*>()) {
        objectsSource->_centerAdjust->enabled = true;
        for (auto gameObject : objectsSource->_controllerModels) {
            if (gameObject)
                gameObject->active = true;
        }
        controller0 = objectsSource->_controller0;
        controller1 = objectsSource->_controller1;
        ReleaseCurrentControllers();
    }

    controller0 = nullptr;
    controller1 = nullptr;
}

void FPFC::OnSceneChange() {
    GetControllers();
}

void FPFC::KeyDown(std::string const& key) {
    if (keyboardOpen) {
        if (key == "Backspace" || key == "Delete")
            keyboardOpen->deleteButtonWasPressedEvent->Invoke();
        else if (key == "Enter")
            keyboardOpen->okButtonWasPressedEvent->Invoke();
        else if (key.size() == 1)
            keyboardOpen->keyWasPressedEvent->Invoke(key[0]);
        return;
    }
    if (key == "F2") {
        GetControllers();
        return;
    }
    auto upper = toupper(key[0]);
    if (key.size() != 1)
        upper = '\0';
    if (upper == 'P') {
        if (auto pauser = Object::FindObjectOfType<PauseController*>())
            pauser->Pause();
    } else if (upper == 'R') {
        if (auto pauser = Object::FindObjectOfType<PauseMenuManager*>())
            pauser->RestartButtonPressed();
    } else if (upper == 'M') {
        if (auto pauser = Object::FindObjectOfType<PauseMenuManager*>())
            pauser->MenuButtonPressed();
    } else if (upper == 'C') {
        if (auto pauser = Object::FindObjectOfType<PauseMenuManager*>())
            pauser->ContinueButtonPressed();
    }
    float* val = nullptr;
    if (upper == 'W')
        val = &movementF.z;
    else if (upper == 'D')
        val = &movementF.x;
    else if (upper == 'E' || upper == ' ')
        val = &movementF.y;
    else if (upper == 'S')
        val = &movementB.z;
    else if (upper == 'A')
        val = &movementB.x;
    else if (upper == 'Q' || key == "Shift")
        val = &movementB.y;
    if (val)
        *val = 1;
}

void FPFC::KeyUp(std::string const& key) {
    if (keyboardOpen)
        return;
    auto upper = toupper(key[0]);
    if (key.size() != 1)
        upper = '\0';
    float* val = nullptr;
    if (upper == 'W')
        val = &movementF.z;
    else if (upper == 'D')
        val = &movementF.x;
    else if (upper == 'E' || upper == ' ')
        val = &movementF.y;
    else if (upper == 'S')
        val = &movementB.z;
    else if (upper == 'A')
        val = &movementB.x;
    else if (upper == 'Q' || key == "Shift")
        val = &movementB.y;
    if (val)
        *val = 0;
}

void FPFC::MouseMove(Vector2 move) {
    move = Vector2::op_Division(move, 4);
    auto euler = rotation.eulerAngles;
    euler.x -= move.y;
    euler.y += move.x;
    rotation = Quaternion::Euler(euler);
}

void FPFC::MouseDown() {
    click = true;
}

void FPFC::MouseUp() {
    click = false;
}

void FPFC::AddScroll(float amount) {
    scroll += amount;
}

Vector3 FPFC::GetMovement() {
    auto combined = Vector3::op_Subtraction(movementF, movementB);
    return Vector3::op_Multiply(combined, Time::get_deltaTime() * 2);
}

Quaternion FPFC::GetRotation() {
    return rotation;
}

void FPFC::MoveControllers(Transform* target) {
    if (UnityW<VRController>(controller0) && UnityW<VRController>(controller1)) {
        controller0->transform->SetPositionAndRotation(target->position, target->rotation);
        controller1->transform->SetPositionAndRotation(target->position, target->rotation);
    }
}

MAKE_AUTO_HOOK_MATCH(
    VRInputModule_GetMousePointerEventData,
    &VRUIControls::VRInputModule::GetMousePointerEventData,
    VRUIControls::MouseState*,
    VRUIControls::VRInputModule* self,
    int id
) {
    auto ret = VRInputModule_GetMousePointerEventData(self, id);
    if (CapturingFPFC()) {
        auto state = EventSystems::PointerEventData::FramePressState::NotChanged;
        if (click && !clickedLastFrame)
            state = EventSystems::PointerEventData::FramePressState::Pressed;
        if (!click && clickedLastFrame)
            state = EventSystems::PointerEventData::FramePressState::Released;
        ret->GetButtonState(EventSystems::PointerEventData::InputButton::Left)->eventData->buttonState = state;
        clickedLastFrame = click;
    } else
        clickedLastFrame = false;
    return ret;
}

MAKE_AUTO_HOOK_MATCH(
    VRPlatformUtils_GetAnyJoystickMaxAxisDefaultImplementation,
    &VRPlatformUtils::GetAnyJoystickMaxAxisDefaultImplementation,
    Vector2,
    IVRPlatformHelper* self
) {
    auto ret = VRPlatformUtils_GetAnyJoystickMaxAxisDefaultImplementation(self);
    if (CapturingFPFC()) {
        float ret = scroll;
        scroll = 0;
        return {0, ret};
    }
    return ret;
}

MAKE_AUTO_HOOK_MATCH(
    OculusVRHelper_TriggerHapticPulse,
    &OculusVRHelper::TriggerHapticPulse,
    void,
    OculusVRHelper* self,
    XR::XRNode node,
    float duration,
    float strength,
    float frequency
) {
    if (CapturingFPFC())
        return;

    OculusVRHelper_TriggerHapticPulse(self, node, duration, strength, frequency);
}

MAKE_AUTO_HOOK_MATCH(
    UIKeyboardManager_OpenKeyboardFor, &UIKeyboardManager::OpenKeyboardFor, void, UIKeyboardManager* self, HMUI::InputFieldView* input
) {
    UIKeyboardManager_OpenKeyboardFor(self, input);

    keyboardOpen = self->_uiKeyboard;
}

MAKE_AUTO_HOOK_MATCH(UIKeyboardManager_CloseKeyboard, &UIKeyboardManager::CloseKeyboard, void, UIKeyboardManager* self) {

    UIKeyboardManager_CloseKeyboard(self);

    keyboardOpen = nullptr;
}
