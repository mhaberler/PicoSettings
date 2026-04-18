#include <M5Unified.h>
#include <PicoMQTT.h>
#include <PicoWebsocket.h>
#include "PicoSettings.h"
#include <ESPmDNS.h>

String macAddress;
static const char *hostname = HOSTNAME;

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

void getMacAddress(String &macStr) {
    // Get MAC address
    uint8_t mac[6];
    Network.macAddress(mac);
    macStr = String(mac[0], HEX) + String(mac[1], HEX) + String(mac[2], HEX) + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
    macStr.toUpperCase();
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


    getMacAddress(macAddress);
    log_i("MAC address=%s", macAddress.c_str());

    if (MDNS.begin(hostname)) {
        MDNS.enableWorkstation();
        MDNS.addService("mqtt", "tcp", MQTT_PORT);
        MDNS.addService("mqtt-ws", "tcp", MQTTWS_PORT);
        MDNS.addServiceTxt("mqtt-ws", "tcp", "path", "/mqtt");
        mdns_service_instance_name_set("_mqtt", "_tcp", ("PicoMQTT-TCP-" + macAddress).c_str());
        mdns_service_instance_name_set("_mqtt-ws", "_tcp",
                                       ("PicoMQTT-WS-" + macAddress).c_str());
    }
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
