#include <M5Unified.h>
#include <PicoMQTT.h>
#include <PicoWebsocket.h>
#include "PicoSettings.h"

bool on_fparam_change(cb_context ctx);

PicoMQTT::Server mqtt;
PicoSettings settings(mqtt, "testns");

PicoSettings::Setting<String> ssid(settings, "ssid", "some-SSID");
PicoSettings::Setting<String> password(settings, "pass", "secret");


PicoSettings::Setting<int> bar(settings, "bar", 42);
PicoSettings::Setting<String> baz(settings, "baz", "The Answer.");
PicoSettings::Setting<double> dzero(settings, "dzero", 0.0);
PicoSettings::Setting<double> dparam(settings, "dparam", PI);
PicoSettings::Setting<float> fparam(settings, "fparam", 2.71828182845904523536, on_fparam_change);
PicoSettings::Setting<bool> flag(settings, "flag", true, [] (cb_context ctx) {
    log_i("flag=%d", flag.get());
    return true;
});

// value change callback
bool on_fparam_change(cb_context ctx) {
    log_i("fparam changed to %f, default value: %f",
          fparam.get(), fparam.get_default());
    return true;
}

void setup() {

    delay(3000);

    M5.begin();
    Serial.begin(115200);

    // make persisted values available
    settings.begin();
    Serial.printf("stored credentials: %s %s\n", ssid.get().c_str(), password.get().c_str());

    // Connect to WiFi
    Serial.printf("Connecting to WiFi %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
    }
    delay(1000);
    Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());

    mqtt.begin();

    settings.publish();
    bar.change_callback = [] (cb_context ctx){
        log_i("bar changed to %d", bar.get());
        return true;
    };
}

uint32_t last;

void loop() {
    mqtt.loop();

    // periodically publish the namespace
    // and the reset special topic
    if (millis() - last > 3000) {
        settings.publish();
        last = millis();
    }
    yield();
}
