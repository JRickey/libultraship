#include "ship/controller/controldevice/controller/mapping/sdl/SDLRumbleMapping.h"

#include "ship/config/ConsoleVariable.h"
#include "ship/utils/StringHelper.h"
#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"
#include <array>
#include <unordered_map>
#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace {
#if defined(__SWITCH__)
constexpr uint32_t kRumbleDurationMs = 30000;
constexpr float kSwitchRumbleLowFrequencyHz = 160.0f;
constexpr float kSwitchRumbleHighFrequencyHz = 320.0f;
#else
constexpr uint32_t kRumbleDurationMs = 0;
#endif

void StartGamepadRumble(SDL_GameController* gamepad, uint16_t lowFrequency, uint16_t highFrequency) {
    if (SDL_GameControllerRumble(gamepad, lowFrequency, highFrequency, kRumbleDurationMs) == 0) {
        return;
    }

    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(gamepad);
    if (joystick != nullptr) {
        SDL_JoystickRumble(joystick, lowFrequency, highFrequency, kRumbleDurationMs);
    }
}

void StopGamepadRumble(SDL_GameController* gamepad) {
    if (SDL_GameControllerRumble(gamepad, 0, 0, 0) == 0) {
        return;
    }

    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(gamepad);
    if (joystick != nullptr) {
        SDL_JoystickRumble(joystick, 0, 0, 0);
    }
}

#if defined(__SWITCH__)
HidNpadIdType GetNpadIdForGamepad(SDL_GameController* gamepad, uint8_t portIndex) {
    const int32_t playerIndex = SDL_GameControllerGetPlayerIndex(gamepad);
    if (playerIndex >= 0 && playerIndex <= 7) {
        return static_cast<HidNpadIdType>(HidNpadIdType_No1 + playerIndex);
    }

    // Detached Joy-Cons may not report a player index; fall back to port index.
    // This ensures Port 0 → No1, Port 1 → No2, etc., preventing cross-port bleed.
    if (portIndex <= 7) {
        return static_cast<HidNpadIdType>(HidNpadIdType_No1 + portIndex);
    }

    return HidNpadIdType_No1;
}

struct SwitchVibrationRouting {
    HidNpadStyleTag styleTag;
    int32_t handleCount;
};

struct SwitchVibrationRoutingCandidates {
    std::array<SwitchVibrationRouting, 3> routings;
    size_t count;
};

SwitchVibrationRoutingCandidates MakeRoutingCandidates(const SwitchVibrationRouting& first,
                                                      const SwitchVibrationRouting& second,
                                                      const SwitchVibrationRouting& third) {
    SwitchVibrationRoutingCandidates candidates;
    candidates.routings = { first, second, third };
    candidates.count = 3;
    return candidates;
}

SwitchVibrationRoutingCandidates GetSwitchVibrationRoutingCandidates(SDL_GameController* gamepad) {
    SwitchVibrationRoutingCandidates candidates =
        MakeRoutingCandidates({ HidNpadStyleTag_NpadJoyDual, 2 }, { HidNpadStyleTag_NpadHandheld, 2 },
                              { HidNpadStyleTag_NpadFullKey, 2 });

    const auto type = SDL_GameControllerGetType(gamepad);

    if (type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT) {
        candidates = MakeRoutingCandidates({ HidNpadStyleTag_NpadJoyLeft, 1 }, { HidNpadStyleTag_NpadHandheld, 2 },
                                           { HidNpadStyleTag_NpadFullKey, 2 });
        return candidates;
    }
    if (type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT) {
        candidates = MakeRoutingCandidates({ HidNpadStyleTag_NpadJoyRight, 1 },
                                           { HidNpadStyleTag_NpadHandheld, 2 },
                                           { HidNpadStyleTag_NpadFullKey, 2 });
        return candidates;
    }

    const char* name = SDL_GameControllerName(gamepad);
    if (name != nullptr && SDL_strstr(name, "Combined Joy-Cons") != nullptr) {
        // In handheld mode, combined Joy-Cons can report through handheld style.
        candidates = MakeRoutingCandidates({ HidNpadStyleTag_NpadHandheld, 2 }, { HidNpadStyleTag_NpadJoyDual, 2 },
                                           { HidNpadStyleTag_NpadFullKey, 2 });
    } else if (name != nullptr && SDL_strstr(name, "Switch Controller") != nullptr) {
        // Generic Switch Controller labels are common in handheld mode.
        candidates = MakeRoutingCandidates({ HidNpadStyleTag_NpadHandheld, 2 }, { HidNpadStyleTag_NpadJoyDual, 2 },
                                           { HidNpadStyleTag_NpadFullKey, 2 });
    }

    return candidates;
}

bool SendSwitchHidVibration(SDL_GameController* gamepad, uint8_t portIndex, float lowAmplitude, float highAmplitude) {
    HidVibrationDeviceHandle handles[2];
    const HidNpadIdType primaryNpadId = GetNpadIdForGamepad(gamepad, portIndex);
    const auto candidates = GetSwitchVibrationRoutingCandidates(gamepad);

    for (size_t i = 0; i < candidates.count; i++) {
        const SwitchVibrationRouting& routing = candidates.routings[i];
        HidNpadIdType npadCandidates[2] = { primaryNpadId, primaryNpadId };
        size_t npadCandidateCount = 1;
        if (routing.styleTag == HidNpadStyleTag_NpadHandheld && primaryNpadId != HidNpadIdType_Handheld) {
            // In attached handheld mode, vibration can report success on No1..No8
            // without physically driving motors. Try Handheld npad first.
            npadCandidates[0] = HidNpadIdType_Handheld;
            npadCandidates[1] = primaryNpadId;
            npadCandidateCount = 2;
        }

        for (size_t npadAttempt = 0; npadAttempt < npadCandidateCount; npadAttempt++) {
            const HidNpadIdType npadId = npadCandidates[npadAttempt];
            Result rc = hidInitializeVibrationDevices(handles, routing.handleCount, npadId, routing.styleTag);
            if (R_FAILED(rc)) {
                continue;
            }

            HidVibrationValue values[2];
            for (int32_t j = 0; j < routing.handleCount; j++) {
                HidVibrationValue& value = values[j];
                value.amp_low = lowAmplitude;
                value.freq_low = kSwitchRumbleLowFrequencyHz;
                value.amp_high = highAmplitude;
                value.freq_high = kSwitchRumbleHighFrequencyHz;
            }

            rc = hidSendVibrationValues(handles, values, routing.handleCount);
            if (R_FAILED(rc)) {
                continue;
            }

            return true;
        }
    }

    return false;
}

bool StartSwitchGamepadRumble(SDL_GameController* gamepad, uint8_t portIndex, uint16_t lowFrequency, uint16_t highFrequency) {
    const float lowAmplitude = static_cast<float>(lowFrequency) / static_cast<float>(UINT16_MAX);
    const float highAmplitude = static_cast<float>(highFrequency) / static_cast<float>(UINT16_MAX);
    return SendSwitchHidVibration(gamepad, portIndex, lowAmplitude, highAmplitude);
}

bool StopSwitchGamepadRumble(SDL_GameController* gamepad, uint8_t portIndex) {
    return SendSwitchHidVibration(gamepad, portIndex, 0.0f, 0.0f);
}

template <typename RumbleFunc>
void ApplySwitchRumbleForPort(uint8_t portIndex, RumbleFunc&& applyRumble) {
    auto* manager = Ship::Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager().get();
    for (const auto& [instanceId, gamepad] : manager->GetConnectedSDLGamepadsForPort(portIndex)) {
        applyRumble(gamepad);
    }
}
#endif
} // namespace

namespace Ship {
SDLRumbleMapping::SDLRumbleMapping(uint8_t portIndex, uint8_t lowFrequencyIntensityPercentage,
                                   uint8_t highFrequencyIntensityPercentage)
    : ControllerRumbleMapping(PhysicalDeviceType::SDLGamepad, portIndex, lowFrequencyIntensityPercentage,
                              highFrequencyIntensityPercentage) {
    SetLowFrequencyIntensity(lowFrequencyIntensityPercentage);
    SetHighFrequencyIntensity(highFrequencyIntensityPercentage);
}

void SDLRumbleMapping::StartRumble() {
#if defined(__SWITCH__)
    ApplySwitchRumbleForPort(mPortIndex, [this](SDL_GameController* gamepad) {
        if (!StartSwitchGamepadRumble(gamepad, mPortIndex, mLowFrequencyIntensity, mHighFrequencyIntensity)) {
            StartGamepadRumble(gamepad, mLowFrequencyIntensity, mHighFrequencyIntensity);
        }
    });
#else
    for (const auto& [instanceId, gamepad] :
         Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(
             mPortIndex)) {
        StartGamepadRumble(gamepad, mLowFrequencyIntensity, mHighFrequencyIntensity);
    }
#endif
}

void SDLRumbleMapping::StopRumble() {
#if defined(__SWITCH__)
    ApplySwitchRumbleForPort(mPortIndex, [this](SDL_GameController* gamepad) {
        if (!StopSwitchGamepadRumble(gamepad, mPortIndex)) {
            StopGamepadRumble(gamepad);
        }
    });
#else
    for (const auto& [instanceId, gamepad] :
         Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(
             mPortIndex)) {
        StopGamepadRumble(gamepad);
    }
#endif
}

void SDLRumbleMapping::SetLowFrequencyIntensity(uint8_t intensityPercentage) {
    mLowFrequencyIntensityPercentage = intensityPercentage;
    mLowFrequencyIntensity = UINT16_MAX * (intensityPercentage / 100.0f);
}

void SDLRumbleMapping::SetHighFrequencyIntensity(uint8_t intensityPercentage) {
    mHighFrequencyIntensityPercentage = intensityPercentage;
    mHighFrequencyIntensity = UINT16_MAX * (intensityPercentage / 100.0f);
}

std::string SDLRumbleMapping::GetRumbleMappingId() {
    return StringHelper::Sprintf("P%d", mPortIndex);
}

void SDLRumbleMapping::SaveToConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".RumbleMappings." + GetRumbleMappingId();
    Ship::Context::GetInstance()->GetConsoleVariables()->SetString(
        StringHelper::Sprintf("%s.RumbleMappingClass", mappingCvarKey.c_str()).c_str(), "SDLRumbleMapping");
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.LowFrequencyIntensity", mappingCvarKey.c_str()).c_str(),
        mLowFrequencyIntensityPercentage);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.HighFrequencyIntensity", mappingCvarKey.c_str()).c_str(),
        mHighFrequencyIntensityPercentage);
    Ship::Context::GetInstance()->GetConsoleVariables()->Save();
}

void SDLRumbleMapping::EraseFromConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".RumbleMappings." + GetRumbleMappingId();

    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.RumbleMappingClass", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.LowFrequencyIntensity", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.HighFrequencyIntensity", mappingCvarKey.c_str()).c_str());

    Ship::Context::GetInstance()->GetConsoleVariables()->Save();
}

std::string SDLRumbleMapping::GetPhysicalDeviceName() {
    return "SDL Gamepad";
}
} // namespace Ship
