# PicoSettings
This is an example how to use PicoMQTT together with the [ESP32 Preferences](https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html) library for retrieving and changing configuration values backed by non-volatile storage.

## Example
An instance of this class associates an MQTT server with a Preferences namespace,
exporting variables in this namespace to topics like `preferences/<namespace>/<variable>`


````c++

// keep variables under the "testns" namespace:
PicoSettings settings(mqtt, "testns");

// this variable will appear under the topic preferences/testns/bar
PicoSettings::Setting<int> bar(settings, "bar", 42);

````

## Setting a new value via assignment

To permanently change a Settings variable, just assign its value - the new value will be recorded in NVS:

````c++
bar = 4711;
````
On next reboot, `bar` will be set to 4711.

## Setting a value via MQTT
To change the value of the `bar` setting, publish a new value to `preferences/testns/bar`.

The new value will be stored permanently in NVS, hence will retain the new value after a reboot.

## Inspecting values via MQTT
For conveniently making all variables and their values visible via MQTT, periodically call `settings.publish()` .

## Getting a value
A Setting can be used like a variable of the underlying type, for example:

````c++
Serial.printf("bar = %d\n", bar);
````

## Resetting all variables to their compile-time defaults

To reset all variables in a namespace to their compile-time defaults, use the `defaults()` method:

````c++
setting.defaults();
````
After reboot, `bar` will again have the value 42.

## Resetting all variables in a namespace via MQTT
To achieve the same effect via MQTT, publish any value to the special topic `preferences/<namespace>/reset` .

With this example, the reset topic would be `preferences/testns/reset` .

## Value change notification

A value change callback may be associated with a setting, to cause some action in user code.

The callback signature is `bool(cb_context)`, where `cb_context` indicates why the callback was invoked:

- `cb_context::CB_INITIAL_SETTING` — called during `begin()` after loading the value from NVS.
- `cb_context::CB_SUBSCRIBE` — called when a new value arrives via MQTT.
- `cb_context::CB_SET` — called from the `set()` method.
- `cb_context::CB_ASSIGN` — called from `operator=` (before `CB_SET`).

Return `true` to persist the new value to NVS, or `false` to reject it.

Example using a lambda:
````c++
PicoSettings::Setting<bool> flag(settings, "flag", true, [] (cb_context ctx) {
    log_i("flag=%d ctx=%d", flag.get(), ctx);
    return true;
});
````
Example using a callback function:

````c++
bool onFparamChange(cb_context ctx) {
    log_i("fparam changed to %f, default value: %f",
          fparam.get(), fparam.getDefault());
    return true;
}

PicoSettings::Setting<float> fparam(settings, "fparam", 2.718, onFparamChange);
````

A callback can also be assigned or changed at runtime:
````c++
bar.changeCallback = [] (cb_context ctx) {
    log_i("bar changed to %d", bar.get());
    return true;
};
````

## Initialisation

To make the persisted values of PicoSettings available, call the `begin()` method:

````c++
PicoSettings settings(mqtt, "testns");
PicoSettings::Setting<int> bar(settings, "bar", 42);

void setup() {
    settings.begin();
    // print the persisted value of bar
    Serial.printf("bar = %d\n", bar.get());
}
````

This will fire the initial value change callback, subscribe settings for updates via the `preferences/<namespace>/<name>` topic, and register a `preferences/<namespace>/reset` topic.

Writing any value to this `reset` topic will wipe that namespace, and revert all values to compile-time defaults.

## Publishing PicoSettings

To publish all variables in a namespace with their current values, call the `publish()` method.

## Integration Testing

An MQTT-based integration test in `tests/test_settings.py` verifies the full round-trip against a live device running the example firmware.

### Prerequisites

````bash
uv pip install --python .venv/bin/python paho-mqtt pytest
````

The device must be flashed and reachable at `picomqtt.local:1883`.

### What the test does

| Phase | Description |
|-------|-------------|
| 0 | Reset device to compile-time defaults (clean baseline) |
| 1 | Collect and record default values from `preferences/testns/#` |
| 2 | Publish random values for all settings, reboot, verify they persisted |
| 3 | Trigger reset + reboot, verify compile-time defaults are restored |

### Running

````bash
.venv/bin/python -m pytest tests/test_settings.py -v -s
````


