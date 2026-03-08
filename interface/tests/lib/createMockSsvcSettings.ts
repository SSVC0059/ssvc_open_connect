import type { SsvcSettings } from '$lib/types/ssvc';

const defaults: SsvcSettings = {
	heads: [0, 0],
	late_heads: [0, 0],
	hearts: [0, 0],
	tails: [0, 0],
	hyst: 0,
	decrement: 0,
	sound: 0,
	pressure: 0,
	relay_inverted: 0,
	relay_autostart: 0,
	autoresume: 0,
	auto_mode: 0,
	heads_timer: 0,
	late_heads_timer: 0,
	hearts_timer: 0,
	tails_temp: 0,
	start_delay: 0,
	hearts_finish_temp: 0,
	parallel_v3: [],
	parallel_v1: [0, 0],
	parallel: [0, 0],
	hearts_temp_shift: false,
	hearts_pause: false,
	formula: false,
	formula_start_temp: 0,
	tank_mmhg: 0,
	tp2_shift: 0,
	tp_filter: false,
	signal_tp1_control: 0,
	signal_inverted: 0,
	tp1_control_temp: 0,
	tp1_control_start: 0,
	stab_limit_time: 0,
	stab_limit_finish: 0,
	backlight: 'off',
	valve_bw: [0, 0, 0],
	release_speed: 0,
	release_timer: 0,
	heads_final: 0
};

/**
 * Создаёт объект SsvcSettings для тестов с возможностью переопределения полей.
 * По умолчанию release_timer = 0 (см. issue #75).
 */
export function createMockSsvcSettings(
	overrides: Partial<SsvcSettings> = {}
): SsvcSettings {
	return { ...defaults, ...overrides };
}
