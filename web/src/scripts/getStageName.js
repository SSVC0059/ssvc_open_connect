export function getStageName(type_message){
    switch (type_message) {
        case "waiting": {
            return "Дежурный режим"
        }
        case "tp1_waiting": {
            return "Ожидание достижения температуры  TP1"
        }
        case "delayed_start": {
            return "Ожидание старта"
        }
        case "heads": {
            return "Отбор голов"
        }
        case "hearts": {
            return "Отбор тела"
        }
        case "tails": {
            return "Отбор хвостов"
        }
        case "response": {
            return "Отправка запроса"
        }
        default: {
            return `Не известный режим: ${type_message}`
        }
    }
}