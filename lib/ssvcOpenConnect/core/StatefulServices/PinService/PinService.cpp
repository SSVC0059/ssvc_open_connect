#include "PinService.h"

PinService::PinService() : StatefulService() {
}

void PinService::begin() {
    read([](const PinsState& state) {
        for (const auto& pinState : state) {
            applyPinConfiguration(pinState);
        }
    });
}

void PinService::applyPinConfiguration(const PinState& pinState) {
    switch (pinState.mode) {
        case PinMode::MODE_INPUT:
            pinMode(pinState.pin, INPUT);
            break;
        case PinMode::MODE_OUTPUT:
            pinMode(pinState.pin, OUTPUT);
            digitalWrite(pinState.pin, pinState.value);
            break;
        case PinMode::MODE_INPUT_PULLUP:
            pinMode(pinState.pin, INPUT_PULLUP);
            break;
        case PinMode::MODE_ANALOG:
            // Для ESP32 не требуется вызывать pinMode для аналогового чтения
            break;
        case PinMode::MODE_UNDEFINED:
        default:
            // Ничего не делаем
            break;
    }
}

int PinService::readPin(const int pin) {
    int value = -1;
    read([&](const PinsState& state) {
        for (const auto& pinState : state) {
            if (pinState.pin == pin) {
                if (pinState.mode == PinMode::MODE_ANALOG) {
                    value = analogRead(pinState.pin);
                } else {
                    value = digitalRead(pinState.pin);
                }
                break;
            }
        }
    });

    if (value != -1) {
        update([&](PinsState& state) {
            for (auto& pinState : state) {
                if (pinState.pin == pin) {
                    if (pinState.value != value) {
                        pinState.value = value;
                        return StateUpdateResult::CHANGED;
                    }
                    break;
                }
            }
            return StateUpdateResult::UNCHANGED;
        }, "PinService::readPin");
    }
    return value;
}

void PinService::writePin(const int pin, const int value) {
    update([&](PinsState& state) {
        for (auto& pinState : state) {
            if (pinState.pin == pin & pinState.mode == PinMode::MODE_OUTPUT) {
                if (pinState.value != value) {
                    pinState.value = value;
                    digitalWrite(pinState.pin, value);
                    return StateUpdateResult::CHANGED;
                }
                return StateUpdateResult::UNCHANGED;
            }
        }
        return StateUpdateResult::ERROR; // Пин не найден или не в режиме OUTPUT
    }, "PinService::writePin");
}
