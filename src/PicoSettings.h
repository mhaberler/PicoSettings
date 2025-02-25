#pragma once

#include <set>
#include <PicoMQTT.h>
#include <Preferences.h>
#include "nvs.h"

#define FLOAT_DIGITS 7
#define DOUBLE_DIGITS 12

typedef enum {
    CB_INITIAL_SETTING,
    CB_SUBSCRIBE,
    CB_SET,
    CB_ASSIGN
} cb_context;

typedef std::function<bool(const cb_context)> callback_t;

// These methods convert a MQTT message payload string to different types.  The 2nd param must be a non-const reference.
// TODO: Find a way to return the parsed value

static inline void load_from_string(const String & payload, String & value) {
    value = payload;
}

static inline void load_from_string(const String & payload, int & value) {
    value = payload.toInt();
}

static inline void load_from_string(const String & payload, double & value) {
    value = payload.toDouble();
}

static inline void load_from_string(const String & payload, float & value) {
    value = payload.toFloat();
}

static inline void load_from_string(const String & payload, bool & value) {
    value = payload.toInt();
}

// These methods convert different types to a MQTT payload
static inline String store_to_string(const String & value) {
    return value;
}

static inline String store_to_string(const int & value) {
    return String(value);
}

static inline String store_to_string(const double & value) {
    return String(value, DOUBLE_DIGITS);
}

static inline String store_to_string(const float & value) {
    return String(value, FLOAT_DIGITS);
}

static inline String store_to_string(const bool & value) {
    return String((int)value);
}

// NVRAM getters
static inline int nvGet(Preferences &p, const String &key, int defaultValue) {
    return  p.getInt(key.c_str(), defaultValue);
}

static inline String nvGet(Preferences &p, const String &key, const String &defaultValue) {
    return p.getString(key.c_str(), defaultValue.c_str());
}

static inline double nvGet(Preferences &p, const String &key, double defaultValue) {
    return  p.getDouble(key.c_str(), defaultValue);
}

static inline float nvGet(Preferences &p, const String &key, float defaultValue) {
    return  p.getFloat(key.c_str(), defaultValue);
}

static inline bool nvGet(Preferences &p, const String &key, bool defaultValue) {
    return  p.getBool(key.c_str(), defaultValue);
}

// NVRAM setters
static inline void nvSet(Preferences &p, const String &key, int value) {
    p.putInt(key.c_str(), value);
}

static inline void nvSet(Preferences &p, const String &key, bool value) {
    p.putBool(key.c_str(), value);
}

static inline void nvSet(Preferences &p, const String &key, float value) {
    p.putFloat(key.c_str(), value);
}

static inline void nvSet(Preferences &p, const String &key, double value) {
    p.putDouble(key.c_str(), value);
}

static inline void nvSet(Preferences &p,const String &key, const String &value) {
    p.putString(key.c_str(), value.c_str());
}


// TODO: Add more load_from_string and store_to_string variants

class PicoSettings {
  public:
    class SettingBase {
      public:
        virtual ~SettingBase() {}

        virtual void begin() = 0;
        virtual void publish() = 0;
        virtual const String &name() = 0;
    };

    template <typename T>
    class Setting: public SettingBase {
      public:
        Setting(PicoSettings & ns, const String & name, const T & default_value, callback_t callback = nullptr):
            _name(name), _ns(ns), _default_value(default_value), change_callback(callback) {
            _value = _default_value;
            // assert(name.len() < NVS_KEY_NAME_MAX_SIZE);
            _ns._settings.insert(this);
        }

        ~Setting() {
            _ns._mqtt.unsubscribe(_ns.prefix() + _ns._name + "/" + _name);
            _ns._settings.erase(this);
        }

        virtual void begin() override {
            if (!_ns._prefs.isKey(_name.c_str())) {
                nvSet(_ns._prefs, _name, _default_value);
                _value = _default_value;
            } else {
                _value = nvGet(_ns._prefs, _name, _value);
            }
            if (change_callback) {
                change_callback(CB_INITIAL_SETTING);
            }
            _ns._mqtt.subscribe(_ns.prefix() + _ns._name + "/" + _name, [this](const String & payload) {
                // if there is a callback set, change the value permanently only if the callback returns true
                load_from_string(payload, _value);
                if (change_callback) {
                    if (change_callback(CB_SUBSCRIBE)) {
                        nvSet(_ns._prefs, _name, _value);
                    }
                } else {
                    nvSet(_ns._prefs, _name, _value);
                }
            });
        }

        virtual void publish() override {
            _ns._mqtt.publish(_ns.prefix() + _ns._name + "/" + _name, store_to_string(_value));
        }

        const T & get() const {
            return _value;
        }

        const T & get_default() const {
            return _default_value;
        }

        void set(const T & new_value) {

            log_i("set %s", _name.c_str());
            if (_value != new_value) {
                _value = new_value;

                if (change_callback) {
                    if (change_callback(CB_SET)) {
                        nvSet(_ns._prefs, _name, _value);
                    }
                } else {
                    nvSet(_ns._prefs, _name, _value);
                }
                publish();
            }
        }

        // This will allow direct conversion to T
        operator T() const {
            return get();
        }

        // This will allow setting from T objects
        const T & operator=(const T & other) {
            if (change_callback) {
                if (change_callback(CB_ASSIGN)) {
                    set(other);
                }
            } else {
                set(other);
            }
            return other;
        }

        const String &name() {
            return _name;
        }

        callback_t change_callback;

      protected:
        const String _name;
        PicoSettings & _ns;
        T _value;
        const T _default_value;
    };


    // PicoMQTT::Server  could be replaced by PicoMQTT::Client like so:
    // PicoSettings(PicoMQTT::Client & mqtt, const String name): mqtt(mqtt), name(name) {}
    PicoSettings(PicoMQTT::Server & mqtt, const String name = "default", const String prefix = "preferences/"): _mqtt(mqtt), _name(name), _prefix(prefix) {}

    ~PicoSettings() {
        _mqtt.unsubscribe(_prefix + _name + "/reset");
        _prefs.end();
    }

    void defaults() {
        log_i("defaults: %s", _name.c_str());
        _prefs.clear();
        _prefs.end();
        begin();
    }

    // init all settings within this namespace
    void begin() {
        if (!_prefs.begin(_name.c_str(), false)) {
            log_e("opening namespace '%s' failed", _name.c_str());
        };
        for (auto & setting: _settings)
            setting->begin();

        // writing any value to preferences/<namespace>/reset will wipe the namespace
        // and set all of its settings to declaration-time defaults
        _mqtt.subscribe(_prefix + _name + "/reset", [this](const String & payload) {
            defaults();
        });
    }

    // publish everything within this namespace
    void publish() {
        for (auto & setting: _settings)
            setting->publish();

        _mqtt.publish(_prefix + _name + "/reset", "0");
    }

    const String &name() {
        return _name;
    }

    const String &prefix() {
        return _prefix;
    }

  protected:
    PicoMQTT::Server & _mqtt;
    Preferences _prefs;
    const String _name;
    const String _prefix;

    // TODO: Perhaps a different container would work better?...
    std::set<SettingBase *> _settings;
};

