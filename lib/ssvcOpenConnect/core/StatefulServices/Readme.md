graph TD
subgraph "Конфигурационный слой"
A[Веб-интерфейс / API<br/>Настройка] --> B
C[config/alarms.json] --> B
end

    subgraph "Сервис порогов"
        B[AlarmThresholdService<br/>StatefulService<br/>- Хранит пороги<br/>- Сохраняет/загружает]
    end

    subgraph "Мониторинг"
        D[OneWireThermalSubsystem<br/>RTOS Task<br/>- Опрашивает датчики<br/>- Вызывает AlarmMonitor] --> E
    end

    subgraph "Диспетчер тревог"
        E[AlarmMonitor<br/>Singleton, 'Сторож'<br/>- Проверяет значения<br/>- Генерирует события] --> F
    end

    subgraph "Подписчики (IAlarmSubscriber)"
        F[IAlarmSubscriber<br/>Интерфейс]
        F --> G[LogSubscriber<br/>- Пишет в лог]
        F --> H[PinTogglerSubscriber<br/>- Управляет GPIO]
        F --> I[NotificationService<br/>- UI уведомления]
    end

    B --> E
    E --> C