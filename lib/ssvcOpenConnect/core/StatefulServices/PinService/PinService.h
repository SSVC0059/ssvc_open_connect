#ifndef PinService_h
#define PinService_h

#include <Arduino.h>
#include <vector>
#include <StatefulService.h>

// Режим работы пина
enum class PinMode {
    MODE_UNDEFINED,
    MODE_INPUT,
    MODE_OUTPUT,
    MODE_INPUT_PULLUP,
    MODE_ANALOG
};

// Состояние пина
struct PinState {
    int pin;
    PinMode mode;
    int value; // Для цифровых (0 или 1) и аналоговых (0-4095) значений
    String name; // Имя пина для удобства
};

// Состояние сервиса - вектор состояний пинов
using PinsState = std::vector<PinState>;

class PinService : public StatefulService<PinsState> {
public:
    PinService();

    // Применяет конфигурацию ко всем пинам из состояния
    void begin();

    // Читает значение с пина
    int readPin(int pin);

    // Записывает значение в пин
    void writePin(int pin, int value);

private:
    // Применяет конфигурацию к одному пину
    static void applyPinConfiguration(const PinState& pinState);
};

#endif // PinService_h
