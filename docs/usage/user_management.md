# Авторизация и управление пользователями

Начиная с версии прошивки [0.2.5.1](https://github.com/SSVC0059/ssvc_open_connect/releases/tag/v0.2.5.1) сборка прошивки включается так же версия со включенной подсистемой авторизацией.
Она позволяет ограничивать доступ к управлению через указание логина и пароля

<div style="display: flex; justify-content: center;">
    <img src="/ssvc_open_connect/usage/media/p0.png" style="height:480px"> 
</div>

## Данные авторизации по умолчанию

| Имя пользователя | Пароль |
| -------- | -------- |
| admin    | admin    |
| guest    | guest    |


Обязательно измените пароль по умолчанию!

## Переход на версию с авторизацией.

1. Для начала вы должны знать какую версию контроллера вы используете. Либо для контроллера с 8мбт флеш памяти или для 16мбт.  Она доступна на вкладке "Система" -> "Статус системы" -> "Flash Chip" либо на маркировке на самом чипе. Не верная версия сломает работу контроллера и потребует полной перепрошивки с очисткой памяти

2. Переходим на страницу [релизов](https://github.com/SSVC0059/ssvc_open_connect/releases) и скачиваем файл прошивки для своего контроллера. Прошивка со включенной авторизацией заканчивается на security

<div style="display: flex; justify-content: center;">
    <img src="/ssvc_open_connect/usage/media/p2.png" style="height:480px"> 
</div>


3. Переходим на страницу обновления прошивки и указываем ее в меню выбора файла.


<div style="display: flex; justify-content: center;">
    <img src="/ssvc_open_connect/usage/media/p3.png" style="height:380px"> 
</div>

Соглашаемся на установку

<div style="display: flex; justify-content: center;">
    <img src="/ssvc_open_connect/usage/media/p1.png" style="height:280px"> 
</div>

!!!warning "Важно!"
    В связи с особенностями прошивки контроллера, цифры прогресс-бара могут зависать в процессе обновления, хотя оно и будет успешное. Обращайте внимание на появление селеных статусных сообщений в правом нижнем углу и перезагружайте страницу в ручную

<div style="display: flex; justify-content: center;">
    <img src="/ssvc_open_connect/usage/media/p0.png" style="height:480px"> 
</div>

!!!warning "Важно!"
    После обновления прошивки обязательно очистить кеш браузера

## Смена пароля


Переходим на страницу настройки пользователей и нажимаем значок карандаша у выбранного пользователя

<div style="display: flex; justify-content: center;">
    <img src="/ssvc_open_connect/usage/media/p4.png" style="height:480px"> 
</div>

В появившемся окне вводим новый пароль и нажимаем кнопку сохранить

<div style="display: flex; justify-content: center;">
    <img src="/ssvc_open_connect/usage/media/p5.png" style="height:480px"> 
</div>


!!!warning Права администратора
    Функциональность роли Администратор подразумевает ограничение на исполнение ряда действий с контроллером, но на текущий момент не проверена и может работать не стабильно


## Создание нового пользователя

Переходим на страницу настройки пользователей и нажимаем кнопку создания нового пользователя.
<div style="display: flex; justify-content: center;">
    <img src="/ssvc_open_connect/usage/media/p6.png" style="height:480px"> 
</div>
В открывшейся форме вводим имя пользователя и пароль