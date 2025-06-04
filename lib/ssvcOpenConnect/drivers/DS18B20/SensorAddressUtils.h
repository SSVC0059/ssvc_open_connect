#ifndef SENSORADDRESSUTILS_H
#define SENSORADDRESSUTILS_H

#include <string>
#include <iomanip>
#include <sstream>

class SensorAddressUtils
{
public:
    static std::string addressToString(const uint8_t* address)
    {
        std::stringstream ss;
        ss << std::hex << std::uppercase << std::setfill('0');

        for (int i = 0; i < 8; i++)
        {
            ss << std::setw(2) << static_cast<int>(address[i]);
            if (i < 7)
            {
                ss << "-";
            }
        }

        return ss.str();
    }

    static bool stringToAddress(const std::string& str, uint8_t* address)
    {
        // Проверка формата: XX-XX-XX-XX-XX-XX-XX-XX
        if (str.length() != 23 ||
            str[2] != '-' || str[5] != '-' || str[8] != '-' ||
            str[11] != '-' || str[14] != '-' || str[17] != '-' || str[20] != '-')
        {
            return false;
        }

        for (int i = 0, pos = 0; i < 8; i++, pos += 3)
        {
            // Проверяем что оба символа hex-цифры
            if (!isxdigit(str[pos]) || !isxdigit(str[pos + 1]))
            {
                return false;
            }

            // Конвертируем два символа в байт
            address[i] = static_cast<uint8_t>(
                (hexDigitToValue(str[pos]) << 4) |
                hexDigitToValue(str[pos + 1])
            );
        }

        return true;
    }

    // Вспомогательная функция для преобразования hex-символа в значение
    static uint8_t hexDigitToValue(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        return 0; // Не должно происходить благодаря isxdigit
    }
};

#endif // SENSORADDRESSUTILS_H
