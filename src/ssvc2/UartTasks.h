#ifndef UARTTASKS_H
#define UARTTASKS_H

#include "freertos/queue.h"

// Функции для управления UART
void ssvc_connect_begin();
void handleUART1MessageTask(void *pvParameters);
void uart1ToUart0Task(void *pvParameters);
void uart0ToUart1Task(void *pvParameters);
void sendCommandToSsvc(const char* message);

#endif // UARTTASKS_H
