export function formatSecondsToHHMMSS(seconds: number): string {
	const hours = Math.floor(seconds / 3600);
	seconds %= 3600;
	const minutes = Math.floor(seconds / 60);
	const remainingSeconds = seconds % 60;

	if (hours > 0) {
		// Если есть часы, возвращаем полный формат ЧЧ:ММ:СС
		return `${hours}:${minutes.toString().padStart(2, '0')}:${remainingSeconds.toString().padStart(2, '0')}`;
	} else {
		// Если часов нет, возвращаем формат ММ:СС
		return `${minutes}:${remainingSeconds.toString().padStart(2, '0')}`;
	}
}

export function getStageDescription(stage: string): string {
	switch (stage) {
		case 'waiting':
			return 'Дежурный режим';
		case 'tp1_waiting':
			return 'Ожидание нагрева колонны';
		case 'delayed_start':
			return 'Стабилизация колонны';
		case 'heads':
			return 'Отбор голов';
		case 'late_heads':
			return 'Отбор подголовников';
		case 'hearts':
			return 'Отбор тела';
		case 'tails':
			return 'Отбор хвостов';
		default:
			return `Ожидание получения данных`;
	}
}

/**
 * Возвращает читаемое описание события ректификации
 * @param event - Строка с идентификатором события
 * @returns Описание события на русском языке
 */
export function getEventDescription(event: string): string {
	switch (event) {
		case "heads_finished":
			return "Завершен этап Головы";
		case "hearts_finished":
			return "Завершен этап Тело";
		case "tails_finished":
			return "Завершен этап Хвосты";
		case "ds_error":
			return "Ошибка датчика температуры";
		case "ds_error_stop":
			return "Выключение оборудования (реле) из-за ошибки датчика";
		case "stabilization_limit":
			return "Превышен лимит времени стабилизации";
		case "remote_stop":
			return "Получена удаленная команда остановки, процесс остановлен";
		case "manually_closed":
			return "Включено ручное управление клапаном текущего этапа, клапан закрыт";
		case "manually_opened":
			return "Включено ручное управление клапаном текущего этапа, клапан открыт";
		default:
			return `Неизвестная команда: ${event}`;
	}
}