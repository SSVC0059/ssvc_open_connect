export function getStageName(type){
    switch (type) {
        case "waiting": {
            return "Ожидание"
        }
        default: {
            return `Не известный режим: ${type}`
        }
    }
}