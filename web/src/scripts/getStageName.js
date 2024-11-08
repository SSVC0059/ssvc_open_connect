export function getStageName(type_message){
    switch (type_message) {
        case "waiting": {
            return "Бездействие"
        }
        case "delayed_start": {
            return "Ожидание старта"
        }
        case "hearts": {
            return "Отбор тела"
        }
        default: {
            return `Не известный режим: ${type_message}`
        }
    }
}