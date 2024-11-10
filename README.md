[![Run Platformio build](https://github.com/SSVC0059/ssvc_open_connect/actions/workflows/build.yml/badge.svg)](https://github.com/SSVC0059/ssvc_open_connect/actions/workflows/build.yml)

# SSVC Open Connect
Open-source hardware-software solution to extend SSVC0059_V2 capabilities through data exchange over UART.


## Разработка и сборка UI

1. Перейти в каталог `web`
2. Выполнить установку зависимостей
```
npm i
```
3. Моки для эмуляции данных от ssvc  размещены в каталоге `web/mock/responce`
Запуск моков осуществляется командой,  где параметром указывается имя мока
```
python mock/server.py responce/waiting.json
```

4. Запуск отладки в браузере выполняется командой
```
npm run dev -- --open
```
5. По окончании разработки запустить сборку дистрибутива веб интрфейса:
```
npm run build 
```
6. Для сборки библиотеки выполняем команду

```
npx svelteesp32 -e async -s ./dist -o ./svelteesp32.h --etag=true
```

Затем копируем собранный файл `svelteesp32.h` Из текущего каталога, в каталог `src/ssvc2`


## Сборка приложения

1.Для борки необходимо установить Platformio core, NodeJs версии не ниже 20.18
2. Запустьть скрипт сборки командой
```
sh build.sh
```

Собрвнная прошивка будет размещена по пути `.pio/build/ssvc2/firmware.bin`


## Подключение к SSVC

1. Контроллер при первоначальном подключении создает точку доступа, в которой нужно будет выбрать точку доступа и пароль
2. После подключения к локальной сети контроллер будет доступен по URL http://svc-open-connect.local