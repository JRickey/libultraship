#include "ship/controller/physicaldevice/ConnectedPhysicalDeviceManager.h"
#include <array>
#include <spdlog/spdlog.h>

#if defined(__SWITCH__)
namespace {
constexpr uint16_t kNintendoVendorId = 0x057E;
constexpr uint16_t kJoyConLeftProductId = 0x2006;
constexpr uint16_t kJoyConRightProductId = 0x2007;

enum class JoyConSide {
    None,
    Left,
    Right,
};

JoyConSide GetJoyConSide(SDL_GameController* gamepad) {
    const auto type = SDL_GameControllerGetType(gamepad);
    if (type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT) {
        return JoyConSide::Left;
    }
    if (type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT) {
        return JoyConSide::Right;
    }

    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(gamepad);
    if (joystick != nullptr) {
        const uint16_t vendor = SDL_JoystickGetVendor(joystick);
        const uint16_t product = SDL_JoystickGetProduct(joystick);

        if (vendor == kNintendoVendorId) {
            if (product == kJoyConLeftProductId) {
                return JoyConSide::Left;
            }
            if (product == kJoyConRightProductId) {
                return JoyConSide::Right;
            }
        }
    }

    const char* name = SDL_GameControllerName(gamepad);
    if (name != nullptr) {
        if (SDL_strstr(name, "Joy-Con (L)") != nullptr || SDL_strstr(name, "Joy-Con Left") != nullptr) {
            return JoyConSide::Left;
        }
        if (SDL_strstr(name, "Joy-Con (R)") != nullptr || SDL_strstr(name, "Joy-Con Right") != nullptr) {
            return JoyConSide::Right;
        }
    }

    return JoyConSide::None;
}
} // namespace
#endif

namespace Ship {
ConnectedPhysicalDeviceManager::ConnectedPhysicalDeviceManager() {
}

ConnectedPhysicalDeviceManager::~ConnectedPhysicalDeviceManager() {
}

std::unordered_map<int32_t, SDL_GameController*>
ConnectedPhysicalDeviceManager::GetConnectedSDLGamepadsForPort(uint8_t portIndex) {
    std::unordered_map<int32_t, SDL_GameController*> result;

    for (const auto& [instanceId, gamepad] : mConnectedSDLGamepads) {
        if (!PortIsIgnoringInstanceId(portIndex, instanceId)) {
            result[instanceId] = gamepad;
        }
    }

    return result;
}

std::unordered_map<int32_t, std::string> ConnectedPhysicalDeviceManager::GetConnectedSDLGamepadNames() {
    return mConnectedSDLGamepadNames;
}

std::unordered_set<int32_t> ConnectedPhysicalDeviceManager::GetIgnoredInstanceIdsForPort(uint8_t portIndex) {
    return mIgnoredInstanceIds[portIndex];
}

bool ConnectedPhysicalDeviceManager::PortIsIgnoringInstanceId(uint8_t portIndex, int32_t instanceId) {
    return GetIgnoredInstanceIdsForPort(portIndex).contains(instanceId);
}

void ConnectedPhysicalDeviceManager::IgnoreInstanceIdForPort(uint8_t portIndex, int32_t instanceId) {
    mIgnoredInstanceIds[portIndex].insert(instanceId);
}

void ConnectedPhysicalDeviceManager::UnignoreInstanceIdForPort(uint8_t portIndex, int32_t instanceId) {
    mIgnoredInstanceIds[portIndex].erase(instanceId);
}

void ConnectedPhysicalDeviceManager::IgnoreVendorIdGlobally(uint16_t vid) {
    if (mIgnoredVendorIds.insert(vid).second) {
        SPDLOG_INFO("ConnectedPhysicalDeviceManager: globally ignoring SDL gamepads with VID 0x{:04x} "
                    "(claimed by another input backend, e.g. Raphnet native)", vid);
    }
}

void ConnectedPhysicalDeviceManager::UnignoreVendorIdGlobally(uint16_t vid) {
    if (mIgnoredVendorIds.erase(vid) > 0) {
        SPDLOG_INFO("ConnectedPhysicalDeviceManager: no longer ignoring SDL gamepads with VID 0x{:04x}", vid);
    }
}

bool ConnectedPhysicalDeviceManager::IsVendorIdIgnoredGlobally(uint16_t vid) const {
    return mIgnoredVendorIds.contains(vid);
}

void ConnectedPhysicalDeviceManager::HandlePhysicalDeviceConnect(int32_t sdlDeviceIndex) {
    RefreshConnectedSDLGamepads();
}

void ConnectedPhysicalDeviceManager::HandlePhysicalDeviceDisconnect(int32_t sdlJoystickInstanceId) {
    RefreshConnectedSDLGamepads();
}

void ConnectedPhysicalDeviceManager::RefreshConnectedSDLGamepads() {
    mConnectedSDLGamepads.clear();
    mConnectedSDLGamepadNames.clear();
    mIgnoredInstanceIds.clear();
    static SDL_JoystickGUID sZeroGuid;
#if defined(__SWITCH__)
    std::array<bool, 4> portAlreadyAssigned = { false, false, false, false };
    std::array<bool, 4> portHasJoyConLeft = { false, false, false, false };
    std::array<bool, 4> portHasJoyConRight = { false, false, false, false };
#endif

    for (int32_t i = 0; i < SDL_NumJoysticks(); i++) {

        SDL_JoystickGUID deviceGUID = SDL_JoystickGetDeviceGUID(i);
        if (SDL_memcmp(&deviceGUID, &sZeroGuid, sizeof(deviceGUID)) == 0) {
            SPDLOG_WARN(
                "Calling SDL JoystickGetDeviceGUID with index ({:d}) returned zero GUID. This is likely due to an "
                "invalid index. Refer to https://wiki.libsdl.org/SDL2/SDL_JoystickGetDeviceGUID for more information.",
                i);
            continue;
        }

        char deviceGuidCStr[33] = "";
        SDL_JoystickGetGUIDString(deviceGUID, deviceGuidCStr, sizeof(deviceGuidCStr));

        if (!SDL_IsGameController(i)) {
            SPDLOG_WARN("SDL Joystick (GUID: {}) not recognized as gamepad."
                        "This is likely due to a missing mapping string in gamecontrollerdb.txt."
                        "Refer to https://github.com/mdqinc/SDL_GameControllerDB for more information.",
                        deviceGuidCStr);
            continue;
        }

        // Globally ignored vendor (e.g. Raphnet adapter already claimed via
        // hidapi by RaphnetPhysicalDeviceManager). Skip BEFORE SDL_GameController-
        // Open so SDL never gets a handle to the device — opening here would
        // compete with our raw-SI commands and on Windows DirectInput tends to
        // grab first, breaking native polling.
        uint16_t devVid = SDL_JoystickGetDeviceVendor(i);
        if (devVid != 0 && IsVendorIdIgnoredGlobally(devVid)) {
            SPDLOG_INFO("ConnectedPhysicalDeviceManager: skipping SDL gamepad index={} VID=0x{:04x} (GUID: {}) "
                        "— globally ignored by another input backend",
                        i, devVid, deviceGuidCStr);
            continue;
        }

        auto gamepad = SDL_GameControllerOpen(i);
        if (gamepad == nullptr) {
            SPDLOG_ERROR("SDL GameControllerOpen error (GUID: {}): {}", deviceGuidCStr, SDL_GetError());
            continue;
        }

        auto instanceId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepad));
        if (instanceId < 0) {
            SPDLOG_ERROR("SDL JoystickInstanceID error (GUID: {}): {}", deviceGuidCStr, SDL_GetError());
            continue;
        }

        std::string gamepadName;
        auto name = SDL_GameControllerName(gamepad);
        if (name == nullptr) {
            gamepadName = deviceGuidCStr;
            SPDLOG_WARN("SDL_GameControllerName returned null. Setting name to GUID \"{}\" instead.", gamepadName);
        } else {
            gamepadName = name;
        }

        mConnectedSDLGamepads[instanceId] = gamepad;
        mConnectedSDLGamepadNames[instanceId] = gamepadName;

#if defined(__SWITCH__)
        // Switch has no practical way to do per-port device toggles in the
        // field, so auto-assign each pad to one player slot by default.
        for (uint8_t port = 0; port < 4; port++) {
            mIgnoredInstanceIds[port].insert(instanceId);
        }

        int32_t assignedPort = SDL_GameControllerGetPlayerIndex(gamepad);
        const JoyConSide joyConSide = GetJoyConSide(gamepad);
        const bool assignedPortInRange = assignedPort >= 0 && assignedPort < 4;
        bool allowJoyConPairOnPort = false;

        if (assignedPortInRange && joyConSide != JoyConSide::None &&
            portAlreadyAssigned[static_cast<size_t>(assignedPort)]) {
            if (joyConSide == JoyConSide::Left) {
                allowJoyConPairOnPort = portHasJoyConRight[static_cast<size_t>(assignedPort)] &&
                                        !portHasJoyConLeft[static_cast<size_t>(assignedPort)];
            } else {
                allowJoyConPairOnPort = portHasJoyConLeft[static_cast<size_t>(assignedPort)] &&
                                        !portHasJoyConRight[static_cast<size_t>(assignedPort)];
            }
        }

        const bool playerIndexUsable =
            assignedPortInRange && (!portAlreadyAssigned[static_cast<size_t>(assignedPort)] || allowJoyConPairOnPort);
        if (!playerIndexUsable) {
            assignedPort = -1;
            for (int32_t candidatePort = 0; candidatePort < 4; candidatePort++) {
                if (!portAlreadyAssigned[static_cast<size_t>(candidatePort)]) {
                    assignedPort = candidatePort;
                    break;
                }
            }

            if (assignedPort < 0) {
                assignedPort = static_cast<int32_t>((mConnectedSDLGamepads.size() - 1) % 4);
            }
        }

        mIgnoredInstanceIds[static_cast<uint8_t>(assignedPort)].erase(instanceId);
        portAlreadyAssigned[static_cast<size_t>(assignedPort)] = true;
        if (joyConSide == JoyConSide::Left) {
            portHasJoyConLeft[static_cast<size_t>(assignedPort)] = true;
        } else if (joyConSide == JoyConSide::Right) {
            portHasJoyConRight[static_cast<size_t>(assignedPort)] = true;
        }
#else
        for (uint8_t port = 1; port < 4; port++) {
            mIgnoredInstanceIds[port].insert(instanceId);
        }
#endif
    }
}
} // namespace Ship
