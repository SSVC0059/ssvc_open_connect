/**
 * Форматирует количество секунд в читаемую строку (days, hours, minutes, seconds).
 */
export function convertSeconds(seconds: number): string {
	let minutes = Math.floor(seconds / 60);
	let hours = Math.floor(minutes / 60);
	let days = Math.floor(hours / 24);

	hours = hours % 24;
	minutes = minutes % 60;
	seconds = seconds % 60;

	let result = '';
	if (days > 0) {
		result += days + ' day' + (days > 1 ? 's' : '') + ' ';
	}
	if (hours > 0) {
		result += hours + ' hour' + (hours > 1 ? 's' : '') + ' ';
	}
	if (minutes > 0) {
		result += minutes + ' minute' + (minutes > 1 ? 's' : '') + ' ';
	}
	result += seconds + ' second' + (seconds > 1 ? 's' : '');

	return result;
}
