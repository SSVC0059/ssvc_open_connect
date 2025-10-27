# Прошивка контроллера под OS Windows

## Подключение устройства к компьютеру

Для коммуникации контроллер на борту имеет 2 USB разъема - отличаются они между собой только типом. Прошивать можно через любой из них, но на разных операционных системах может быть не значительные различия. В данной инструкции объясняется прошивка через разъем <span style="color: red;">USB&OTG</span>  . В обоих случаях, на старых операционнызх системах могут потребоваться установка драйвера.

### USB&OTG
При подключении контроллеа через  <span style="color: red;">USB&OTG</span> в диспечере устройств определится как USB JTAG debug unit


<div class="flex">
<img src="/ssvc_open_connect/preparation/media/usb_otg.png" style="height:240px"> 
</div>

### USB to serial
При подключении контроллеа через  <span style="color: red;">USB to serial</span> в диспечере устройств определится как USB-Enhanced-SERIAL CH343
)

<div style="flex">
<img src="/ssvc_open_connect/ssvc_open_connect/preparation/media/usb_serial.png" style="height:240px"> 
</div>

Если драйвер не обнаружен, так же можно почитьать [эту статью](https://myrobot.ru/downloads/driver-ch9102x-esp32.php) по порядку установки