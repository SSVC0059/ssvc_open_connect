# Подключение модуля

Модули идентичные и отличаются только внешним видом. Подключение у них так же идентичное

## Питание модуля

Питание модуля осуществляется через красно-черный силиконовый провод провод

!!! warning "ОЧЕНЬ ВАЖНАЯ ИНФОРМАЦИЯ!"

    Ни в коем случае не подавайте одновременно питание от разных устройств на пины питания и на USB разъем, а так же не подключайте микроконтроллер к компьютеру при подключенном внешнем питании. Возможен выход из строя как самого микроконтроллера или же USB порта компьютера. Подключение должно быть раздельным

    Для этого рекомендуется добавить дополнительный микровыключатель на питание контроллера

**Контроллер можно запитывать напряжением от 5 до 36 вольт, но рекомендуется не превышать 12В**


!!! warning "Не перепутайте полярность!"

    
    <span style="color: #FF0000;"> КРАСНЫЙ </span> - "+"

    <span style="color: #000000;"> ЧЕРНЫЙ </span> - "-"

<div class="isplay: flex; justify-content: center; gap: 10px; flex-wrap: wrap;">
<img src="/ssvc_open_connect/ready_made/media/connect1.jpg"> 
</div>

## UART

Для подключения к SSVC используется вывод UART


!!! warning "Не перепутайте распиновку!"

    БЕЛЫЙ  - GND 
    
    <span style="color: #004400;"> ЗЕЛЕНЫЙ </span> - к TX SSVC

    <span style="color: #ff0000;"> КРАСНЫЙ </span> - к RX SSVC

<div class="isplay: flex; justify-content: center; gap: 10px; flex-wrap: wrap;">
<img src="/ssvc_open_connect/ready_made/media/uart.jpg"> 
</div>

## DS18B20

Для подключения дополнительных датчиков температур не требуется устанавливать подтягивающий резистор. Он уже установлен на плату.

!!! warning "Не перепутайте полярность!"

    БЕЛЫЙ  - GND 
    
    <span style="color: #004400;"> ЗЕЛЕНЫЙ </span> - DATA

    <span style="color: #ff0000;"> КРАСНЫЙ </span> - к +5v датчика

<div class="isplay: flex; justify-content: center; gap: 10px; flex-wrap: wrap;">
<img src="/ssvc_open_connect/ready_made/media/ds18b20.jpg"> 
</div>


## Светодиод состояния и USB разъем

На модуле для монтажа не din рейку светодиод и USB разъем установлены непосредственно в корпус.

USB разъем может использоваться как для подачи питания так и для прошивки


!!! warning "ОЧЕНЬ ВАЖНАЯ ИНФОРМАЦИЯ!"

    Ни когда не осуществляйте одновременное подключение USB к компьютеру при запитанном контроллере от внешнего блока питания

<div class="isplay: flex; justify-content: center; gap: 10px; flex-wrap: wrap;">
<img src="/ssvc_open_connect/ready_made/media/led.jpg"> 
</div>

на модуле для встраивания в ящик - распаяны на проводах

<div class="isplay: flex; justify-content: center; gap: 10px; flex-wrap: wrap;">
<img src="/ssvc_open_connect/ready_made/media/internal1.jpg"> 
</div>


!!! info
    После размещения корпуса USB разъема на ящике плату необходимо зафиксировать горячим клеем, для иключения выпадания.
