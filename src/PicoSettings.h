#pragma once

#include <set>
#include <PicoMQTT.h>
#include <Preferences.h>

#define FLOAT_DIGITS 7
#define DOUBLE_DIGITS 12

enum class cb_context : uint8_t {
    CB_INITIAL_SETTING,
    CB_SUBSCRIBE,
    CB_SET,
    CB_ASSIGN
};

using callback_t = std::function<bool(const cb_context)>;

// These methods convert a MQTT message payload string to different types.  The 2nd param must be a non-const reference.
// TODO: Find a way to return the parsed value

static inline void loadFromString(const String & payload, String & value) {
    value = payload;
}

static inline void loadFromString(const String & payload, int & value) {
    value = static_cast<int>(payload.toInt());
}

static inline void loadFromString(const String & payload, double & value) {
    value = payload.toDouble();
}

static inline void loadFromString(const String & payload, float & value) {
    value = payload.toFloat();
}

static inline void loadFromString(const String & payload, bool & value) {
    value = (payload.equalsIgnoreCase("true") || payload == "1");
}

// These methods convert different types to a MQTT payload
static inline String storeToString(const String & value) {
    return value;
}

static inline String storeToString(const int & value) {
    return String(value);
}

static inline String storeToString(const double & value) {
    return String(value, DOUBLE_DIGITS);
}

static inline String storeToString(const float & value) {
    return String(value, FLOAT_DIGITS);
}

static inline String storeToString(const bool & value) {
    return String((int)value);
}

// NVRAM getters
static inline int nvGet(Preferences &prefs, const String &key, int defaultValue) {
    return  prefs.getInt(key.c_str(), defaultValue);
}

static inline String nvGet(Preferences &prefs, const String &key, const String &defaultValue) {
    return prefs.getString(key.c_str(), defaultValue.c_str());
}

static inline double nvGet(Preferences &prefs, const String &key, double defaultValue) {
    return  prefs.getDouble(key.c_str(), defaultValue);
}

static inline float nvGet(Preferences &prefs, const String &key, float defaultValue) {
    return  prefs.getFloat(key.c_str(), defaultValue);
}

static inline bool nvGet(Preferences &prefs, const String &key, bool defaultValue) {
    return  prefs.getBool(key.c_str(), defaultValue);
}

// NVRAM setters
static inline void nvSet(Preferences &prefs, const String &key, int value) {
    prefs.putInt(key.c_str(), value);
}

static inline void nvSet(Preferences &prefs, const String &key, bool value) {
    prefs.putBool(key.c_str(), value);
}

static inline void nvSet(Preferences &prefs, const String &key, float value) {
    prefs.putFloat(key.c_str(), value);
}

static inline void nvSet(Preferences &prefs, const String &key, double value) {
    prefs.putDouble(key.c_str(), value);
}

static inline void nvSet(Preferences &prefs,const String &key, const String &value) {
    prefs.putString(key.c_str(), value.c_str());
}


// TODO: Add more loadFromString and storeToString variants

class PicoSettings {
  public:
    class SettingBase {
      public:
        virtual ~SettingBase() = default;

        // Default constructor (needed for derived classes)
        SettingBase() = default;

        // Non-copyable, non-movable (manages pointers)
        SettingBase(const SettingBase &) = delete;
        SettingBase & operator=(const SettingBase &) = delete;
        SettingBase(SettingBase &&) = delete;
        SettingBase & operator=(SettingBase &&) = delete;
        virtual void load() = 0;
        virtual void begin() = 0;
        virtual void publish() = 0;
        [[nodiscard]] virtual const String &name() const = 0;
    };

    template <typename T>
    class Setting: public SettingBase {
      public:
        Setting(PicoSettings & ns, const String & name, const T & defaultValue, callback_t callback = nullptr):
            _name(name), _ns(ns), _default_value(defaultValue), changeCallback(callback) {
            _value = _default_value;
            // assert(name.len() < NVS_KEY_NAME_MAX_SIZE);
            _ns._settings.insert(this);
        }

        ~Setting() {
            _ns._mqtt.unsubscribe(_ns.prefix() + _ns._name + "/" + _name);
            _ns._settings.erase(this);
        }

        void load() override {
            if (!_ns._prefs.isKey(_name.c_str())) {
                nvSet(_ns._prefs, _name, _default_value);
                _value = _default_value;
            } else {
                _value = nvGet(_ns._prefs, _name, _value);
            }
            if (changeCallback) {
                changeCallback(cb_context::CB_INITIAL_SETTING);
            }
        }

        void begin() override {
            load();
            _ns._mqtt.subscribe(_ns.prefix() + _ns._name + "/" + _name, [this](const String & payload) {
                // if there is a callback set, change the value permanently only if the callback returns true
                loadFromString(payload, _value);
                if (changeCallback) {
                    if (changeCallback(cb_context::CB_SUBSCRIBE)) {
                        nvSet(_ns._prefs, _name, _value);
                    }
                } else {
                    nvSet(_ns._prefs, _name, _value);
                }
            });
        }

        void publish() override {
            _ns._mqtt.publish(_ns.prefix() + _ns._name + "/" + _name, storeToString(_value));
        }

        const T & get() const {
            return _value;
        }

        const T & getDefault() const {
            return _default_value;
        }

        void set(const T & newValue) {

            log_i("set %s", _name.c_str());
            if (_value != newValue) {
                _value = newValue;

                if (changeCallback) {
                    if (changeCallback(cb_context::CB_SET)) {
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
        auto operator=(const T & other) -> Setting & {
            if (changeCallback) {
                if (changeCallback(cb_context::CB_ASSIGN)) {
                    set(other);
                 }
               } else {
                set(other);
                }
            return *this;
            }

        [[nodiscard]] const String &name() const override {
            return _name;
        }

        callback_t changeCallback;

      protected:
        const String _name;
        PicoSettings & _ns;
        T _value;
        const T _default_value;

         // Non-copyable, non-movable (has reference member)
        Setting(const Setting &) = delete;
        Setting & operator=(const Setting &) = delete;
        Setting(Setting &&) = delete;
        Setting & operator=(Setting &&) = delete;
      };

    // PicoMQTT::Server  could be replaced by PicoMQTT::Client like so:
    // PicoSettings(PicoMQTT::Client & mqtt, const String name): mqtt(mqtt), name(name) {}
    PicoSettings(PicoMQTT::Server & mqtt, const String &name = "default", const String &prefix = "preferences/"): _mqtt(mqtt), _name(name), _prefix(prefix), _settings{} {}

    ~PicoSettings() {
        _mqtt.unsubscribe(_prefix + _name + "/reset");
        _prefs.end();
    }

    void defaults() {
        log_i("defaults: %s", _name.c_str());
        _prefs.clear();
        _prefs.end();
        reload();
    }

    // init all settings within this namespace
    void begin() {
        reload();
        for (auto & setting: _settings)
            setting->begin();

        // writing any value to preferences/<namespace>/reset will wipe the namespace
        // and set all of its settings to declaration-time defaults
        _mqtt.subscribe(_prefix + _name + "/reset", [this](const String & payload) {
            defaults();
        });
    }

  private:
    // reopen NVS and reload all settings without re-subscribing
    void reload() {
        if (!_prefs.begin(_name.c_str(), false)) {
            log_e("opening namespace '%s' failed", _name.c_str());
        }
        for (auto & setting: _settings)
            setting->load();
    }

  public:
    // publish everything within this namespace
    void publish() {
        for (auto & setting: _settings)
            setting->publish();

        _mqtt.publish(_prefix + _name + "/reset", "0");
    }

    [[nodiscard]] const String &name() const {
        return _name;
    }

    [[nodiscard]] const String &prefix() const {
        return _prefix;
    }

  protected:
    PicoMQTT::Server & _mqtt;
    Preferences _prefs;
    const String _name;
    const String _prefix;

     // Non-copyable, non-movable (has reference members)
    PicoSettings(const PicoSettings &) = delete;
    PicoSettings & operator=(const PicoSettings &) = delete;
    PicoSettings(PicoSettings &&) = delete;
    PicoSettings & operator=(PicoSettings &&) = delete;

      // TODO: Perhaps a different container would work better?...
    std::set<SettingBase *> _settings;
};
