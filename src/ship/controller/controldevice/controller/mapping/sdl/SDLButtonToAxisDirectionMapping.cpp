#include "ship/controller/controldevice/controller/mapping/sdl/SDLButtonToAxisDirectionMapping.h"
#include <spdlog/spdlog.h>
#include "ship/utils/StringHelper.h"
#include "ship/window/gui/IconsFontAwesome4.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"

#define MAX_SDL_RANGE (float)INT16_MAX

namespace {
#if defined(__SWITCH__)
constexpr uint16_t kNintendoVendorId = 0x057E;
constexpr uint16_t kJoyConLeftProductId = 0x2006;
constexpr uint16_t kJoyConRightProductId = 0x2007;

bool ShouldUseSingleJoyConButtonFallback(SDL_GameController* gamepad) {
    const auto type = SDL_GameControllerGetType(gamepad);
    if (type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT ||
        type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT) {
        return true;
    }

    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(gamepad);
    if (joystick != nullptr) {
        const uint16_t vendor = SDL_JoystickGetVendor(joystick);
        const uint16_t product = SDL_JoystickGetProduct(joystick);
        if (vendor == kNintendoVendorId && (product == kJoyConLeftProductId || product == kJoyConRightProductId)) {
            return true;
        }
    }

    const char* name = SDL_GameControllerName(gamepad);
    return name != nullptr && SDL_strstr(name, "Joy-Con") != nullptr;
}
#endif
} // namespace

namespace Ship {
SDLButtonToAxisDirectionMapping::SDLButtonToAxisDirectionMapping(uint8_t portIndex, StickIndex stickIndex,
                                                                 Direction direction, int32_t sdlControllerButton)
    : ControllerInputMapping(PhysicalDeviceType::SDLGamepad),
      ControllerAxisDirectionMapping(PhysicalDeviceType::SDLGamepad, portIndex, stickIndex, direction),
      SDLButtonToAnyMapping(sdlControllerButton) {
}

float SDLButtonToAxisDirectionMapping::GetNormalizedAxisDirectionValue() {
    if (Context::GetInstance()->GetControlDeck()->GamepadGameInputBlocked()) {
        return 0.0f;
    }

    for (const auto& [instanceId, gamepad] :
         Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(
             mPortIndex)) {
#if defined(__SWITCH__)
        if (!ShouldUseSingleJoyConButtonFallback(gamepad)) {
            continue;
        }
#endif

        if (SDL_GameControllerGetButton(gamepad, mControllerButton)) {
            return MAX_AXIS_RANGE;
        }
    }

    return 0.0f;
}

std::string SDLButtonToAxisDirectionMapping::GetAxisDirectionMappingId() {
    return StringHelper::Sprintf("P%d-S%d-D%d-SDLB%d", mPortIndex, mStickIndex, mDirection, mControllerButton);
}

void SDLButtonToAxisDirectionMapping::SaveToConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".AxisDirectionMappings." + GetAxisDirectionMappingId();
    Ship::Context::GetInstance()->GetConsoleVariables()->SetString(
        StringHelper::Sprintf("%s.AxisDirectionMappingClass", mappingCvarKey.c_str()).c_str(),
        "SDLButtonToAxisDirectionMapping");
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.Stick", mappingCvarKey.c_str()).c_str(), mStickIndex);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.Direction", mappingCvarKey.c_str()).c_str(), mDirection);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.SDLControllerButton", mappingCvarKey.c_str()).c_str(), mControllerButton);
    Ship::Context::GetInstance()->GetConsoleVariables()->Save();
}

void SDLButtonToAxisDirectionMapping::EraseFromConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".AxisDirectionMappings." + GetAxisDirectionMappingId();
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.Stick", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.Direction", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.AxisDirectionMappingClass", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.SDLControllerButton", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->Save();
}

int8_t SDLButtonToAxisDirectionMapping::GetMappingType() {
    return MAPPING_TYPE_GAMEPAD;
}

std::string SDLButtonToAxisDirectionMapping::GetPhysicalDeviceName() {
    return SDLButtonToAnyMapping::GetPhysicalDeviceName();
}

std::string SDLButtonToAxisDirectionMapping::GetPhysicalInputName() {
    return SDLButtonToAnyMapping::GetPhysicalInputName();
}
} // namespace Ship
