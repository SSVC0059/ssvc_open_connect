let previousTp1 = null;  // Переменная для хранения предыдущего значения tp1

function updateData(data) {
    
    if (data.type == 'response' && data.request == "GET_SETTINGS") {
        update_setting_ssvc(data)
    } else {
        update_telemetry(data)
    }
 
};

async function update_setting_ssvc(data) {
    // Отображаем JSON в текстовом формате
    // document.getElementById('settings_ssvc_data').innerText = JSON.stringify(data, null, 2);
           // Обновляем отображение значений
    console.info(JSON.stringify(data, null, 2))
    
    document.getElementById('head_speed_1_value').innerText = data.settings.heads[0];
    document.getElementById('head_speed_2_value').innerText = data.settings.heads[1];
    
    document.getElementById('heart_speed_1_value').innerText = data.settings.hearts[0];
    document.getElementById('heart_speed_2_value').innerText = data.settings.hearts[0];
            
    document.getElementById('tails_speed_1_value').innerText = data.settings.tails[0];
    document.getElementById('tails_speed_2_value').innerText = data.settings.tails[0];
    
    document.getElementById('hyst').innerText = data.settings.hyst;
    document.getElementById('decrement').innerText = data.settings.decrement;
   
    // Звук. Значение 1 - включено, 0 - выключено.
    if (data.settings.sound == 1) {
        document.getElementById('sound').innerText = "Включен"
    } else {
        document.getElementById('sound').innerText = "Выключен"
    }

    if (data.settings.pressure == 1) {
        document.getElementById('pressure').innerText = "Включен"
    } else {
        document.getElementById('pressure').innerText = "Выключен"
    }

    if (data.settings.relay_inverted == 1) {
        document.getElementById('relay_inverted').innerText = "Да"
    } else {
        document.getElementById('relay_inverted').innerText = "Нет"
    }

    if (data.settings.relay_autostart == 1) {
        document.getElementById('relay_autostart').innerText = "Да"
    } else {
        document.getElementById('relay_autostart').innerText = "Нет"
    }
    
    if (data.settings.autoresume == 1) {
        document.getElementById('autoresume').innerText = "Да"
    } else {
        document.getElementById('autoresume').innerText = "Нет"
    }
};


async function update_telemetry(data) {
       // Обновляем заголовок страницы в зависимости от типа
       const header = document.getElementById('header');
       if (data.type == 'waiting') {
           header.innerText = "Бездействие";
       } else if (data.type == 'tp1_waiting') {
           header.innerText = "Ожидание нагрева куба:";
       } else if (data.type == 'tp2_waiting') {
           header.innerText = "Ожидание нагрева колонны"; 
       } else if (data.type == 'delayed_start') {
           header.innerText = "Отсроченный старт"; 
       } else if (data.type == 'heads') {
           header.innerText = "Отбор голов"; 
       } else if (data.type == 'hearts') {
           header.innerText = "Отбор тела"; 
       } else if (data.type == 'tails') {
           header.innerText = "Отбор хвостов";
       } else if (data.type == 'message') {
            header.innerText = data.text;  
       } else {
           header.innerText = "Неизвестный тип: " + data.type;
       }
   
       // Обновляем значение tp1 и применяем стили
       const tp1Element = document.getElementById('tp1');
       tp1Element.innerText = data.common.tp1;
   
       // Применяем цвет в зависимости от изменения значения tp1
       if (previousTp1 !== null) {
           if (data.common.tp1 > previousTp1) {
               tp1Element.style.color = 'red';  // Если tp1 растет, красный
           } else if (data.common.tp1 < previousTp1) {
               tp1Element.style.color = 'green';  // Если tp1 снижается, зеленый
           }
       }
   
        // Обновляем отображение значений
        document.getElementById('tp1_target').innerText = data.tp1_target;
        document.getElementById('mmhg').innerText = data.common.mmhg;
        document.getElementById('tp1').innerText = data.common.tp1;
        document.getElementById('tp2').innerText = data.common.tp2;
        if (data.common.relay == 1) {
            document.getElementById('relay').innerText = "Включено"
        } else {
            document.getElementById('relay').innerText = "Отключен"
        }
        if (data.common.relay == 1) {
            document.getElementById('signal').innerText = "Включено"
        } else {
            document.getElementById('signal').innerText = "Отключен"
        }
   
       // Отображаем JSON в текстовом формате
    //    document.getElementById('jsonData').innerText = JSON.stringify(data, null, 2);
   
       // Сохраняем текущее значение tp1 для следующего сравнения
       previousTp1 = data.common.tp1;
};


async function click_menu(tab_name) {
    console.info("tab_name:" + tab_name)
    // Удаляем класс "active" у всех вкладок
    document.querySelectorAll('.tab').forEach(tab => tab.classList.remove('active'));

    // Добавляем класс "active" к выбранной вкладке
    document.getElementById(tab_name + '_tab').classList.add('active');

    // Скрываем все контейнеры
    document.getElementById("data_container").style.display = "none";
    document.getElementById("settings_ssvc_container").style.display = "none";
    document.getElementById("open_connect_container").style.display = "none";
    document.getElementById("about_container").style.display = "none";

    if (tab_name == "main") {
        document.getElementById("data_container").style.display = "block";
    } else if (tab_name == "settings_ssvc") {
        document.getElementById("settings_ssvc_container").style.display = "block";

        // Вызов метода для получения настроек (пример вызова)
        sendGetSettings(); 

    } else if (tab_name == "open_connect") {
        document.getElementById("open_connect_container").style.display = "block";
    }
}

function sendGetSettings() {

    const response = fetch('/get_setting_ssvc');
    
    // Проверка на успешный ответ
    if (!response.ok) {
        console.log(response.ok);
    }

}

// Устанавливаем подключение через Server-Sent Events
const eventSource = new EventSource('/events');

eventSource.addEventListener('telemetry', function(event) {
    const data = JSON.parse(event.data);
    updateData(data);
}, false);


eventSource.onerror = function() {
    console.error('Ошибка при подключении к Server-Sent Events');
};

