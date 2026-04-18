#!/usr/bin/env python3
"""
PicoSettings MQTT integration test.

Verifies:
  1. Device publishes all settings via MQTT
  2. Modified settings persist across reboots
  3. Reset restores compile-time defaults

Target: MQTT broker at picomqtt.local:1883
Requires: paho-mqtt >= 2.0, pytest
"""

import random
import string
import threading
import time

import paho.mqtt.client as mqtt
import pytest

BROKER = "picomqtt.local"
PORT = 1883
PREFIX = "preferences/testns/"
TIMEOUT = 15  # seconds to wait for settings after (re)boot
REBOOT_SETTLE = 2  # minimum wait after triggering reboot

# Settings under test — excludes control topics (reset, reboot)
TESTABLE = {"ssid", "pass", "bar", "baz", "dzero", "dparam", "fparam", "flag"}

# Numeric comparison tolerances (values round-trip through String(val, N))
FLOAT_TOL = 1e-3
DOUBLE_TOL = 1e-8


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _random_values():
    """Generate random value strings appropriate for each setting type."""
    return {
        "ssid": "t-" + "".join(random.choices(string.ascii_lowercase, k=5)),
        "pass": "".join(random.choices(string.ascii_letters + string.digits, k=8)),
        "bar": str(random.randint(100, 9999)),
        "baz": "Rnd-" + "".join(random.choices(string.ascii_letters, k=6)),
        "dzero": f"{random.uniform(-50.0, 50.0):.10f}",
        "dparam": f"{random.uniform(-50.0, 50.0):.10f}",
        "fparam": f"{random.uniform(-50.0, 50.0):.5f}",
        "flag": str(random.randint(0, 1)),
    }


def _collect(timeout=TIMEOUT):
    """
    Connect to the broker (retrying until it's reachable), subscribe to
    PREFIX/#, and return a dict of {name: value_string} once all TESTABLE
    settings have been received.
    """
    received = {}
    connected = threading.Event()
    complete = threading.Event()

    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)

    def on_connect(c, _ud, _fl, rc, _props=None):
        c.subscribe(PREFIX + "#")
        connected.set()

    def on_message(_c, _ud, msg):
        name = msg.topic[len(PREFIX) :]
        if name in TESTABLE:
            received[name] = msg.payload.decode()
            if received.keys() >= TESTABLE:
                complete.set()

    client.on_connect = on_connect
    client.on_message = on_message

    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            client.connect(BROKER, PORT, keepalive=60)
            break
        except (ConnectionRefusedError, OSError, TimeoutError):
            time.sleep(0.5)
    else:
        pytest.fail(f"Cannot connect to {BROKER}:{PORT} within {timeout}s")

    client.loop_start()
    try:
        connected.wait(timeout=max(deadline - time.time(), 0.1))
        remaining = max(deadline - time.time(), 0.1)
        if not complete.wait(timeout=remaining):
            missing = sorted(TESTABLE - received.keys())
            pytest.fail(
                f"Timed out waiting for settings ({timeout}s). "
                f"Missing: {missing}"
            )
        return dict(received)
    finally:
        client.loop_stop()
        client.disconnect()


def _publish(values, extra=None):
    """
    Publish setting values to the broker.

    *values*: dict {setting_name: payload_string}
    *extra*:  list of (topic_suffix, payload) appended after values,
              each preceded by a 0.3s gap to guarantee ordering.
    """
    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
    ready = threading.Event()
    client.on_connect = lambda _c, *_a: ready.set()
    client.connect(BROKER, PORT, keepalive=60)
    client.loop_start()
    ready.wait(5)

    for name, val in values.items():
        client.publish(PREFIX + name, val)
        time.sleep(0.05)

    for suffix, payload in extra or []:
        time.sleep(0.3)
        client.publish(PREFIX + suffix, payload)

    time.sleep(0.5)
    client.loop_stop()
    client.disconnect()


def _assert_match(actual, expected, label):
    """Compare settings with type-appropriate tolerance."""
    for name in sorted(TESTABLE):
        a, e = actual.get(name), expected.get(name)
        assert a is not None, f"{label}: '{name}' missing from device response"
        if name in ("fparam",):
            assert abs(float(a) - float(e)) <= FLOAT_TOL, (
                f"{label}: {name} = {a!r}, expected ~{e!r}"
            )
        elif name in ("dzero", "dparam"):
            assert abs(float(a) - float(e)) <= DOUBLE_TOL, (
                f"{label}: {name} = {a!r}, expected ~{e!r}"
            )
        elif name in ("bar", "flag"):
            assert int(float(a)) == int(float(e)), (
                f"{label}: {name} = {a!r}, expected {e!r}"
            )
        else:
            assert a == e, f"{label}: {name} = {a!r}, expected {e!r}"


# ---------------------------------------------------------------------------
# Test
# ---------------------------------------------------------------------------


def test_settings_persist_and_reset():
    """Settings persist across reboot; reset restores compile-time defaults."""

    # --- Phase 0: reset to compile-time defaults ----------------------------
    print("\n[Phase 0] Resetting to compile-time defaults ...")
    # Verify the device is reachable first
    initial = _collect()
    print(f"  Device reachable, got {len(initial)} settings")
    _publish({}, extra=[("reset", "1"), ("reboot", "1")])
    time.sleep(REBOOT_SETTLE)

    # --- Phase 1: collect known-good defaults -------------------------------
    print("\n[Phase 1] Collecting default values ...")
    defaults = _collect()
    for k in sorted(defaults):
        print(f"  {k:>8s} = {defaults[k]!r}")

    # --- Phase 2: overwrite with random values, reboot, verify --------------
    randoms = _random_values()
    print("\n[Phase 2] Publishing random values ...")
    for k in sorted(randoms):
        print(f"  {k:>8s} <- {randoms[k]!r}")
    _publish(randoms)

    print("  Triggering reboot ...")
    _publish({}, extra=[("reboot", "1")])
    time.sleep(REBOOT_SETTLE)

    print("  Waiting for device to come back ...")
    persisted = _collect()
    for k in sorted(persisted):
        print(f"  {k:>8s} = {persisted[k]!r}")
    _assert_match(persisted, randoms, "persistence")
    print("  OK: Random values persisted across reboot")

    # --- Phase 3: reset + reboot, verify defaults restored ------------------
    print("\n[Phase 3] Triggering reset + reboot ...")
    _publish({}, extra=[("reset", "1"), ("reboot", "1")])
    time.sleep(REBOOT_SETTLE)

    print("  Waiting for device to come back ...")
    restored = _collect()
    for k in sorted(restored):
        print(f"  {k:>8s} = {restored[k]!r}")
    _assert_match(restored, defaults, "reset")
    print("  OK: Default values restored after reset")
