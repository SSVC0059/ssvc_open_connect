# Подключение к SSVC по UART

Для реализации подключения необходим версия модуля [SSVC0059V2](https://smartmodule.ru/portfolio/0059_v2/) от компании SmartModule с выведенными наружу пинами для подключения UART 

<div style="display: flex; justify-content: center; gap: 10px; flex-wrap: wrap;">
<img src="/ssvc_open_connect/usage/interfaces/media/ssvc_uart.jpg" style="height:450px"> 
</div>

## Разъем UART на SSVC

Их можно получить 2мя способами:

### Купить уже с распянными пинами

Продаются [на официальных площадках](https://smartmodule.ru/nashi-partnery/). и выглядит это примерно так

<div style="display: flex; justify-content: center;">
<img src="/ssvc_open_connect/usage/interfaces/media/ssvc_uart_1.png" style="height:450px"> 
</div>

### Распаять самостоятельно. 

Для этого придется вскрыть корпус и припаять проводки к необходимым нам контактам. 

!!! warning "НАРУШЕНИЕ ГАРАНТИИ"

    Производитель заявляет, что самостоятельное вскрытие контроллера снимает гарантию с прибора. При правильных действиях сломать там нечего, но аккуратно паять все же надо. Если нет опыта, то лучше обратиться за помощью в мастерскую или производителю.


<div style="display: flex; justify-content: center;">
    <iframe src="https://vk.com/video_ext.php?oid=-231081277&id=456239019&hash=68d374431e5f8f87" 
            width="853" 
            height="480" 
            allow="autoplay; encrypted-media; fullscreen; picture-in-picture" 
            frameborder="0" 
            allowfullscreen>
    </iframe>
</div>

## Разъем UART на ESP32-S3

В прошивке настроено использвание пинов `GPIO_17-RX SSVC` и `GPIO 18-TX SSVC`

т.е для правильной работы нужно подключить так:

!!! warning "Питание от SSVC"
    Ни в коем случае не используйте питание от контроллера. Даже наличие дополнительного модуля внутри заводского SSVC0059_V2 будет не лучшим решением.
    Запитывайте ESP32 самостоятельно    

<div style="display: flex; justify-content: center;">
<img src="/ssvc_open_connect/usage/interfaces/media/ssvc_uart_2.jpg" style="height:200px"> 
</div>

Вот пример правильного подключения. Обратите внимание на цвет проводов.


<div style="display: flex; justify-content: center;">
    <img src="/ssvc_open_connect/usage/interfaces/media/ssvc_uart_3.jpg" style="height:auto"> 
</div>
