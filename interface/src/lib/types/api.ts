export type TelegramConfig = {
	token: string;
	chat_id: string;
}

export type SubsystemsState = {
	telegram_bot: boolean;
	thermal: boolean;
}
