
#include <ESP32SvelteKit.h>
#include <PsychicHttpServer.h>
#include <SsvcOpenConnectService.h>

#define SERIAL_BAUD_RATE 115200

PsychicHttpServer server;

ESP32SvelteKit esp32sveltekit(&server, 120);


SsvcOpenConnectService ssvcOpenConnectService = SsvcOpenConnectService(&server,
                                                     esp32sveltekit.getSocket(),
                                                     esp32sveltekit.getSecurityManager(),
                                                     esp32sveltekit.getMqttClient());


void setup()
{

    Serial.begin(SERIAL_BAUD_RATE);

    #ifdef FACTORY_WIFI_HOSTNAME
        esp32sveltekit.setMDNSAppName(FACTORY_WIFI_HOSTNAME);
    #endif

    esp32sveltekit.begin();
    ssvcOpenConnectService.begin();

}

void loop()
{
    vTaskDelete(nullptr);
}
