# Clang-Tidy Findings — PicoSettings.h

**Source:** `src/PicoSettings.h` (288 lines)
**Checks enabled:** `bugprone-*`, `cert-*`, `cppcoreguidelines-*`, `modernize-*`, `readability-*`, `performance-*`, `llvm-include-order`
**Total warnings:** 109

---

## Summary by Category

| Category | Count | Severity | Description |
|----------|-------|----------|-------------|
| `readability-identifier-naming` | 24 | Low | Functions use snake_case; config expects CamelCase |
| `modernize-use-trailing-return-type` | 15 | Low | Prefer trailing return types (`auto -> T`) |
| `cppcoreguidelines-non-private-member-variables-in-classes` | 13 | Low | Member variables are protected instead of private |
| `cppcoreguidelines-avoid-const-or-ref-data-members` | 4 | Medium | Const/ref data members (common in embedded, acceptable) |
| `cppcoreguidelines-special-member-functions` | 4 | Medium | Non-default destructor without copy/move constructors |
| `modernize-use-using` | 2 | Low | Prefer `using` over `typedef` |
| `cppcoreguidelines-explicit-virtual-functions` | 3 | Low | Redundant `virtual` on `override` functions |
| `readability-identifier-length` | 8 | Low | Parameter name `p` too short (≥3 chars expected) |
| `modernize-pass-by-value` | 3 | Low | Pass `String` by value + `std::move` |
| `cppcoreguidelines-pro-type-vararg` | 2 | Low | C-style vararg (`log_i`) usage |
| `bugprone-*` | 3 | Medium | Narrowing conversion, implicit bool conversion, return const ref from parameter |
| `cppcoreguidelines-use-enum-class` | 1 | Low | Unscoped enum `cb_context` |
| `performance-enum-size` | 1 | Low | Enum uses `unsigned int` (4B) instead of `uint8_t` (1B) |
| `modernize-macro-to-enum` | 2 | Low | `FLOAT_DIGITS`, `DOUBLE_DIGITS` could be enums |
| `llvm-include-order` | 1 | Low | Includes not sorted properly |
| `cppcoreguidelines-c-copy-assignment-signature` | 1 | Medium | `operator=` returns `const T&` instead of `Setting&` |
| `bugprone-return-const-ref-from-parameter` | 1 | Medium | Returns const ref to parameter (use-after-free risk) |
| `cppcoreguidelines-pro-type-member-init` | 1 | Low | Constructor doesn't initialize `_settings` |
| `modernize-use-nodiscard` | 2 | Low | `name()` should be marked `[[nodiscard]]` |
| `modernize-use-equals-default` | 1 | Low | Trivial destructor could use `= default` |
| `misc-non-private-member-variables-in-classes` | 1 | Low | `change_callback` is public |

---

## High/Medium Priority Findings (Actionable)

### 1. `operator=` returns const reference — potential use-after-free
**Line 206:** `const T & operator=(const T & other)` returns a const ref to the parameter, which may be a temporary.
```
warning: returning a constant reference parameter may cause use-after-free when the parameter is constructed from a temporary [bugprone-return-const-ref-from-parameter]
warning: operator=() should return 'Setting&' [cppcoreguidelines-c-copy-assignment-signature]
```

### 2. Narrowing conversion in `load_from_string(int&)`
**Line 28:** `payload.toInt()` returns `long`, assigned to `int`.
```
warning: narrowing conversion from 'long' to signed type 'int' is implementation-defined [bugprone-narrowing-conversions,cppcoreguidelines-narrowing-conversions]
```

### 3. Implicit long→bool conversion in `load_from_string(bool&)`
**Line 40:** Same issue as above — `payload.toInt()` → `bool`.
```
warning: implicit conversion 'long' -> 'bool' [readability-implicit-bool-conversion]
```

### 4. Missing copy/move semantics with non-default destructor
**Lines 109, 111, 122:** Classes define destructors but no copy/move constructors or assignment operators.
```
warning: class 'PicoSettings' defines a non-default destructor but does not define a copy constructor... [cppcoreguidelines-special-member-functions]
warning: class 'SettingBase' defines a non-default destructor... [cppcoreguidelines-special-member-functions]
warning: class 'Setting' defines a non-default destructor... [cppcoreguidelines-special-member-functions]
```

---

## Low Priority / Style Findings (Suppress or Address Incrementally)

### Naming conventions (24 warnings)
All functions use `snake_case` but the clang-tidy config expects `CamelCase`. Either:
- Rename all functions to CamelCase, **or**
- Update `.clang-tidy` CheckOptions to accept snake_case for this project.

Recommended `.clang-tidy` addition:
```yaml
CheckOptions:
   - key: readability-identifier-naming.FunctionCase
     value: camelCase
   - key: readability-identifier-naming.VariableCase
     value: camelCase
```

### Redundant `virtual` on `override` (3 warnings)
Lines 136, 148, 163 — `virtual` keyword is redundant when `override` is present. Remove for cleanliness.

### Member visibility (13+ warnings)
All member variables are `protected` instead of `private`. This is intentional for a library with inheritance, but clang-tidy flags it. Suppress via `.clang-tidy`:
```yaml
Checks: '-misc-non-private-member-variables-in-classes'
```

### Const/ref data members (4 warnings)
Common pattern in embedded C++ — suppress or accept as intentional design.

---

## Recommended Next Steps

1. **Fix high/medium issues first** (#1–#4 above) — these are real correctness concerns.
2. **Update `.clang-tidy` naming conventions** to match the project's snake_case style (saves 24 warnings).
3. **Suppress low-priority categories** that don't apply to embedded/library code:
   - `cppcoreguidelines-avoid-const-or-ref-data-members`
   - `misc-non-private-member-variables-in-classes`
   - `cppcoreguidelines-pro-type-vararg` (log_i is unavoidable in Arduino)
4. **Run clang-tidy as a CI gate** once the noise is reduced, to catch new issues.

---

## How to Run

```bash
# Direct invocation
python scripts/run_clang_tidy.py

# Via PlatformIO
platformio run --environment pico-lint
```
