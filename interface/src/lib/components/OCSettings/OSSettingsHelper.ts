import IconIn from '~icons/tabler/login-2';
import IconOut from '~icons/tabler/logout';
import Wind from '~icons/tabler/wind';
import Unknown from '~icons/tabler/help-circle';
import Trash from '~icons/tabler/trash';

export function getZoneDescription(zone: string): string {
	switch (zone) {
		case 'unknown':
			return 'Не назначен';
		case 'inlet_water':
			return 'Входящая вода';
		case 'outlet_water':
			return 'Исходящая вода';
		case 'act':
			return 'Трубка связи с атмосферой';
		case 'deleted':
			return 'удаленные';
		default:
			return `не известная`;
	}
}

// Available zones (you might want to fetch these from API)
export const availableZones = [
	{ value: 'inlet_water', label: 'Входящая вода' },
	{ value: 'outlet_water', label: 'Исходящая вода' },
	{ value: 'act', label: 'Трубка связи с атмосферой' },
	{ value: 'unknown', label: 'Неизвестная зона' }
];

export function getZoneComponent(zone: string) {
	switch (zone) {
		case 'unknown':
			return Unknown;
		case 'inlet_water':
			return IconIn;
		case 'outlet_water':
			return IconOut;
		case 'act':
			return Wind;
		case 'deleted':
			return Trash;
		default:
			return Unknown;
	}
}
