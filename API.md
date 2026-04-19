# PicoSettings — API Migration Guide

This document describes breaking API changes. Update consuming code accordingly.

---

## `cb_context` is now a scoped enum

`cb_context` was changed from an unscoped `enum` to a strongly-typed `enum class`. All enum values must now be prefixed with `cb_context::`.

| Before | After |
|--------|-------|
| `CB_INITIAL_SETTING` | `cb_context::CB_INITIAL_SETTING` |
| `CB_SUBSCRIBE` | `cb_context::CB_SUBSCRIBE` |
| `CB_SET` | `cb_context::CB_SET` |
| `CB_ASSIGN` | `cb_context::CB_ASSIGN` |

```cpp
// Before
bool myCallback(cb_context ctx) {
    if (ctx == CB_SUBSCRIBE) { ... }
    return true;
}

// After
bool myCallback(cb_context ctx) {
    if (ctx == cb_context::CB_SUBSCRIBE) { ... }
    return true;
}
```

If you pass `ctx` to a `%d` format argument (e.g. `log_i`), cast it explicitly:

```cpp
log_i("ctx=%d", static_cast<int>(ctx));
```

---

## `Setting::get_default()` renamed to `getDefault()`

```cpp
// Before
float d = fparam.get_default();

// After
float d = fparam.getDefault();
```

---

## `Setting::change_callback` renamed to `changeCallback`

The public callback member has been renamed to follow camelCase convention.

```cpp
// Before
bar.change_callback = [] (cb_context ctx) {
    return true;
};

// After
bar.changeCallback = [] (cb_context ctx) {
    return true;
};
```

---

## Callback function naming convention

User-defined callback functions should now use camelCase. This is a style convention enforced by the project's clang-tidy configuration, not a library requirement.

```cpp
// Before
bool on_fparam_change(cb_context ctx) { ... }
PicoSettings::Setting<float> fparam(settings, "fparam", 2.718, on_fparam_change);

// After
bool onFparamChange(cb_context ctx) { ... }
PicoSettings::Setting<float> fparam(settings, "fparam", 2.718, onFparamChange);
```

---

## Summary of renamed symbols

| Old name | New name | Scope |
|----------|----------|-------|
| `CB_INITIAL_SETTING` | `cb_context::CB_INITIAL_SETTING` | enum value |
| `CB_SUBSCRIBE` | `cb_context::CB_SUBSCRIBE` | enum value |
| `CB_SET` | `cb_context::CB_SET` | enum value |
| `CB_ASSIGN` | `cb_context::CB_ASSIGN` | enum value |
| `Setting::get_default()` | `Setting::getDefault()` | method |
| `Setting::change_callback` | `Setting::changeCallback` | member |
