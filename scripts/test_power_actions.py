#!/usr/bin/env python3
"""Run Turn Off and Restart end to end in the simulator process.

Each case boots an isolated fs_ with raw shortcut values in the settings file,
triggers the action through scripted HAL input, and follows the simulator
across the reboot the action causes:

- ESP.restart() re-executes the process (SimulatorLifecycle::reboot) with
  CROSSPOINT_SIM_INPUT_SCRIPT_AFTER_REBOOT as its new input script.
- Deep sleep blocks in HalGPIO::startDeepSleep() until any key goes down, then
  re-executes as a Power-button wake (SimulatorLifecycle::rebootAsPowerWake)
  with CROSSPOINT_SIM_INPUT_SCRIPT_AFTER_WAKE. A QUIT while asleep ends the
  process instead, leaving the SD state exactly as the device powered off.

Build the environment first, for example: pio run -e x4-pro-simulator

The simulator cannot show the hold-to-wake rule (wake verification and NVS are
compiled out), the panel power-down or the Picco USB light-sleep hold; check
those on hardware.
"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]

# Persisted shortcut IDs (src/CrossPointSettings.h). Settings files store these
# raw values, so they must never change; check_persisted_ids() pins them.
SLEEP = 1
QUICK_ACTIONS = 27
TURN_OFF = 41
RESTART = 42
CHORD_TURN_OFF = 37
CHORD_RESTART = 38
LONG_MENU_TURN_OFF = 30
LONG_MENU_RESTART = 31
SLEEP_SCREEN_QUICK_RESUME = 9
UI_THEME_LYRA = 1
PERSISTED_IDS = {
    "SLEEP": SLEEP,
    "QUICK_ACTIONS": QUICK_ACTIONS,
    "TURN_OFF": TURN_OFF,
    "RESTART": RESTART,
    "SHORT_PWRBTN_COUNT": 43,
    "CHORD_TURN_OFF": CHORD_TURN_OFF,
    "CHORD_RESTART": CHORD_RESTART,
    "POWER_CHORD_ACTION_COUNT": 39,
    "LONG_MENU_TURN_OFF": LONG_MENU_TURN_OFF,
    "LONG_MENU_RESTART": LONG_MENU_RESTART,
    "LONG_PRESS_MENU_ACTION_COUNT": 32,
    "QUICK_RESUME": SLEEP_SCREEN_QUICK_RESUME,
    "LYRA": UI_THEME_LYRA,
}
# Touch flows need the X4 Pro profile: touch, a frontlight and its top drop-down panel.
TOUCH_ENVS = ("x4-pro-simulator",)
# Top-edge swipe that opens the drop-down panel, then a tap on its header power
# icon (the left 64 px of the header; portrait 480x800, Lyra, default UI size).
OPEN_PANEL = "SWIPE:240,8,240,200,250"
TAP_PANEL_POWER = "TAP:32,66"

# Power shortcuts are ignored for 2 s after boot (main.cpp allowSleepAt).
TRIGGER_MS = 2500
PAGE_TURNS = (1200, 1600)
WAKE_MS = 6000
QUIT_WHILE_OFF_MS = 6000
SAFETY_QUIT_MS = 12000
AFTER_REBOOT_SCRIPT = "4000:QUIT"
CRASH = re.compile(r"Segmentation fault|Assertion failed|std::bad_alloc|uncaught exception|AddressSanitizer")
BOOT_MARKER = "[BOOT] Reset diagnostic:"
SLEEP_FRAME = "sleep_frame.bin"


@dataclass(frozen=True)
class Case:
    name: str
    settings: dict
    trigger: tuple  # (offset ms, scripted HAL input)
    action: str  # "restart", "turn-off" or "sleep" (control)
    context: str = "epub"  # "epub" resumes a book, "home" boots to Home
    after: str = "reboot"  # "reboot" / "wake" follow the new process, "off" quits while asleep
    sleep_screen: int | None = None
    via: str | None = None  # activity that must open before the request, e.g. the drop-down panel
    not_before: int = 0  # the request must not log before this trigger offset (a cancel ran first)
    touch: bool = False  # needs a TOUCH_ENVS profile


CASES = {
    case.name: case
    for case in (
        Case("long-power-restart", {"longPwrBtn": RESTART}, ((0, "POWER:850"),), "restart"),
        Case("long-power-turn-off-wake", {"longPwrBtn": TURN_OFF}, ((0, "POWER:850"),), "turn-off", after="wake"),
        Case("long-power-turn-off-state", {"longPwrBtn": TURN_OFF}, ((0, "POWER:850"),), "turn-off", after="off",
             sleep_screen=SLEEP_SCREEN_QUICK_RESUME),
        # Control: an ordinary Sleep with Quick Resume does write the frame, so
        # its absence after Turn Off is meaningful.
        Case("long-power-sleep-control", {"longPwrBtn": SLEEP}, ((0, "POWER:850"),), "sleep", after="off",
             sleep_screen=SLEEP_SCREEN_QUICK_RESUME),
        Case("short-power-turn-off", {"shortPwrBtn": TURN_OFF}, ((0, "POWER"),), "turn-off", after="off"),
        Case("power-chord-restart", {"powerChordAction": CHORD_RESTART}, ((0, "POWER:350"), (20, "UP:280")),
             "restart"),
        Case("power-chord-turn-off", {"powerChordAction": CHORD_TURN_OFF}, ((0, "POWER:350"), (20, "UP:280")),
             "turn-off", after="off"),
        Case("long-menu-restart", {"longPressMenuAction": LONG_MENU_RESTART}, ((0, "ENTER:850"),), "restart"),
        Case("long-back-turn-off", {"longPressBackAction": LONG_MENU_TURN_OFF}, ((0, "BACK:1100"),), "turn-off",
             after="off"),
        Case("side-long-restart", {"sideButtonUpLong": RESTART}, ((0, "UP:850"),), "restart"),
        Case("quick-actions-turn-off", {"shortPwrBtn": QUICK_ACTIONS, "quickActionSlots": [TURN_OFF, 0, 0, 0, 0]},
             ((0, "POWER"), (450, "ENTER")), "turn-off", after="off"),
        Case("home-long-power-restart", {"longPwrBtn": RESTART}, ((0, "POWER:850"),), "restart", context="home"),
        Case("home-long-power-turn-off-wake", {"longPwrBtn": TURN_OFF}, ((0, "POWER:850"),), "turn-off",
             context="home", after="wake"),
        # The Home Power entry is the last row: Up wraps to it, Confirm opens the
        # chooser (Turn Off preselected), Down moves to Restart.
        Case("home-menu-turn-off", {"uiTheme": UI_THEME_LYRA}, ((0, "UP"), (500, "ENTER"), (1100, "ENTER")),
             "turn-off", context="home", after="off"),
        Case("home-menu-restart", {"uiTheme": UI_THEME_LYRA},
             ((0, "UP"), (500, "ENTER"), (1100, "DOWN"), (1600, "ENTER")), "restart", context="home"),
        # Back cancels without queuing anything; the entry stays selected and reopens.
        Case("home-menu-cancel-then-turn-off", {"uiTheme": UI_THEME_LYRA},
             ((0, "UP"), (500, "ENTER"), (1100, "BACK"), (1700, "ENTER"), (2300, "ENTER")), "turn-off",
             context="home", after="off", not_before=2300),
        Case("panel-turn-off-wake", {"uiTheme": UI_THEME_LYRA},
             ((0, OPEN_PANEL), (800, TAP_PANEL_POWER), (1400, "ENTER")), "turn-off", after="wake",
             via="FrontlightPanel", touch=True),
        Case("panel-restart", {"uiTheme": UI_THEME_LYRA},
             ((0, OPEN_PANEL), (800, TAP_PANEL_POWER), (1400, "DOWN"), (1900, "ENTER")), "restart", context="home",
             via="FrontlightPanel", touch=True),
    )
}


def check_persisted_ids() -> None:
    header = (ROOT / "src" / "CrossPointSettings.h").read_text()
    for name, value in PERSISTED_IDS.items():
        if name.endswith("_COUNT"):
            # Counts are implicit: one past the explicit ID listed just before them.
            found = re.search(rf"=\s*(\d+),\s*\n\s*{name}\b", header)
            actual = int(found.group(1)) + 1 if found else None
        else:
            found = re.search(rf"\b{name}\s*=\s*(\d+)\b", header)
            actual = int(found.group(1)) if found else None
        if actual != value:
            raise AssertionError(f"{name} is {actual} in CrossPointSettings.h; this test expects {value}")
    print(f"PASS persisted IDs: {', '.join(f'{k}={v}' for k, v in PERSISTED_IDS.items())}", flush=True)


def write_epub(path: Path) -> None:
    with zipfile.ZipFile(path, "w") as z:
        z.writestr("mimetype", "application/epub+zip")
        z.writestr("META-INF/container.xml",
                   '<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>'
                   '<rootfile full-path="content.opf" media-type="application/oebps-package+xml"/></rootfiles>'
                   '</container>')
        z.writestr("content.opf",
                   '<package version="2.0" unique-identifier="id" xmlns="http://www.idpf.org/2007/opf"><metadata '
                   'xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">power-actions</dc:identifier>'
                   '<dc:title>Power action test</dc:title><dc:language>en</dc:language></metadata><manifest>'
                   '<item id="c0" href="c0.xhtml" media-type="application/xhtml+xml"/></manifest><spine>'
                   '<itemref idref="c0"/></spine></package>')
        text = "".join(f"<p>Paragraph {n}. Turn Off and Restart must save this reading position before the "
                       f"device powers down or reboots, and the next power-on must reopen it.</p>"
                       for n in range(80))
        z.writestr("c0.xhtml", f'<html xmlns="http://www.w3.org/1999/xhtml"><head><title>One</title></head>'
                               f'<body><h1>Chapter One</h1>{text}</body></html>')


def hold_ms(action: str) -> int:
    """Hold time of a scripted key, e.g. POWER:850; the simulator default is 80 ms."""
    parts = action.split(":")
    return int(parts[1]) if len(parts) > 1 else 80


def boots(output: str) -> list[str]:
    """Split the combined log of every process image at each boot."""
    parts = output.split(BOOT_MARKER)
    return [BOOT_MARKER + part for part in parts[1:]]


def activities(log: str) -> list[str]:
    return re.findall(r"Entering activity: (\w+)", log)


def in_order(log: str, *needles: str) -> bool:
    position = 0
    for needle in needles:
        position = log.find(needle, position)
        if position < 0:
            return False
        position += len(needle)
    return True


def epub_progress(state_dir: Path) -> tuple[int, int] | None:
    files = list(state_dir.glob("epub_*/progress.bin"))
    if len(files) != 1:
        return None
    return struct.unpack_from("<HH", files[0].read_bytes())


def simulator_env(script: str, extra: dict | None = None) -> dict:
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("CROSSPOINT_SIM_", "CROSSINK_SIMULATOR_SMOKE"))}
    env.update(SDL_VIDEODRIVER="dummy", CROSSPOINT_SIM_INPUT_SCRIPT=script)
    env.update(extra or {})
    return env


def run_program(program: Path, work: Path, env: dict) -> str:
    result = subprocess.run([str(program)], cwd=work, env=env, text=True, capture_output=True, timeout=45)
    output = result.stdout + result.stderr
    if result.returncode != 0:
        raise AssertionError(f"exit code {result.returncode}\n{output[-3000:]}")
    if CRASH.search(output):
        raise AssertionError(f"crash pattern in log\n{output[-3000:]}")
    return output


def run_case(env_name: str, program: Path, case: Case, logs: Path | None) -> list[str]:
    checked: list[str] = []
    reader = case.context == "epub"
    with tempfile.TemporaryDirectory(prefix="crossink-power-actions-") as directory:
        work = Path(directory)
        state_dir = work / "fs_" / ".crosspoint"
        state_dir.mkdir(parents=True)
        if reader:
            write_epub(work / "fs_" / "book.epub")
        settings = {"shortPwrBtn": 0, "longPwrBtn": 0, "longPressMenuAction": 0, "longPressBackAction": 0,
                    "powerChordAction": 0, "sideButtonUpLong": 0}
        settings.update(case.settings)
        if case.sleep_screen is not None:
            settings["sleepScreen"] = case.sleep_screen
        settings_path = state_dir / "crossink-settings.json"
        settings_path.write_text(json.dumps(settings))
        (state_dir / "state.json").write_text(json.dumps({
            "openEpubPath": "/book.epub" if reader else "",
            "lastSleepFromReader": reader,
            "showBootScreen": False,
            "releaseNotesSeen": 1,
        }))

        script = [f"{TRIGGER_MS + offset}:{action}" for offset, action in case.trigger]
        if reader:
            script = [f"{at}:RIGHT" for at in PAGE_TURNS] + script
        extra = {}
        if case.after == "wake":
            script += [f"{WAKE_MS}:POWER", f"{SAFETY_QUIT_MS}:QUIT"]
            extra["CROSSPOINT_SIM_INPUT_SCRIPT_AFTER_WAKE"] = AFTER_REBOOT_SCRIPT
        elif case.after == "off":
            script += [f"{QUIT_WHILE_OFF_MS}:QUIT"]
        else:
            script += [f"{SAFETY_QUIT_MS}:QUIT"]
            extra["CROSSPOINT_SIM_INPUT_SCRIPT_AFTER_REBOOT"] = AFTER_REBOOT_SCRIPT
        output = run_program(program, work, simulator_env(";".join(script), extra))
        if logs:
            logs.mkdir(parents=True, exist_ok=True)
            (logs / f"{env_name}-{case.name}.log").write_text(output)

        images = boots(output)
        first = images[0] if images else ""
        start_activity = "EpubReader" if reader else "Home"
        if start_activity not in activities(first):
            raise AssertionError(f"first boot never reached {start_activity}: {activities(first)}")

        expected_boots = 1 if case.after == "off" else 2
        if len(images) != expected_boots:
            raise AssertionError(f"expected {expected_boots} boot(s), saw {len(images)}\n{output[-3000:]}")
        checked.append(f"{len(images)} boot(s)")

        if case.action == "restart":
            if not in_order(first, "[MAIN] Restart requested", "Entering activity: PowerScreen", "[MAIN] Restarting"):
                raise AssertionError(f"missing Restart requested -> PowerScreen -> Restarting\n{first[-3000:]}")
            if "Entering deep sleep" in first or "Turn off requested" in first:
                raise AssertionError("Restart also entered deep sleep or Turn Off")
            checked.append("'Restart requested' -> PowerScreen -> 'Restarting'")
            # A held Power button must be released before the reboot, so the
            # new boot does not receive its release.
            power_release = max((TRIGGER_MS + offset + hold_ms(action) for offset, action in case.trigger
                                 if action.startswith("POWER")), default=None)
            if power_release is not None:
                restarted_at = int(re.search(r"\[(\d+)\] \[INF\] \[MAIN\] Restarting", first).group(1))
                if restarted_at < power_release:
                    raise AssertionError(f"rebooted at {restarted_at} ms with Power held until {power_release} ms")
                checked.append(f"waited for the Power release ({power_release} ms) before rebooting")
        elif case.action == "turn-off":
            if not in_order(first, "[MAIN] Turn off requested", "Entering activity: PowerScreen",
                            "Entering deep sleep (turned off)"):
                raise AssertionError(f"missing Turn off requested -> PowerScreen -> deep sleep (turned off)\n"
                                     f"{first[-3000:]}")
            if "[MAIN] Restarting" in first or "Restart requested" in first:
                raise AssertionError("Turn Off restarted instead")
            if "Entering activity: Sleep" in first:
                raise AssertionError("Turn Off drew the sleep screen")
            checked.append("'Turn off requested' -> PowerScreen -> 'Entering deep sleep (turned off)'")
        else:
            if "Entering deep sleep" not in first or "(turned off)" in first or "PowerScreen" in first:
                raise AssertionError(f"Sleep control did not take the ordinary sleep path\n{first[-3000:]}")
            checked.append("ordinary Sleep path")

        request = "Restart requested" if case.action == "restart" else "Turn off requested"
        if case.via:
            if not in_order(first, f"Entering activity: {case.via}", f"[MAIN] {request}"):
                raise AssertionError(f"{case.via} did not open before '{request}': {activities(first)}")
            checked.append(f"requested from {case.via}")
        if case.not_before and case.action in ("restart", "turn-off"):
            requested_at = int(re.search(rf"\[(\d+)\] \[INF\] \[MAIN\] {request}", first).group(1))
            if requested_at < TRIGGER_MS + case.not_before:
                raise AssertionError(f"'{request}' at {requested_at} ms, before the final selection")
            checked.append(f"nothing queued before the final selection ({requested_at} ms)")

        if case.after in ("reboot", "wake"):
            second = images[1]
            route = "Wake route: PowerButton" if case.after == "wake" else "Wake route: Other"
            if route not in second:
                raise AssertionError(f"second boot did not report {route}\n{second[-3000:]}")
            entered = activities(second)
            if not entered or entered[0] != "Boot":
                raise AssertionError(f"second boot skipped the splash: {entered}")
            if start_activity not in entered:
                raise AssertionError(f"second boot did not resume {start_activity}: {entered}")
            checked.append(f"{route.split(': ')[1]} boot shows the splash and resumes {start_activity}")

        state = json.loads((state_dir / "state.json").read_text())
        if case.after == "off":
            frame = (state_dir / SLEEP_FRAME).exists()
            if case.action == "sleep":
                if not frame or state.get("showBootScreen") is not False:
                    raise AssertionError(f"control Sleep left frame={frame} showBootScreen="
                                         f"{state.get('showBootScreen')}")
                checked.append("control: Quick Resume frame saved, splash skipped next wake")
            else:
                if frame:
                    raise AssertionError("Turn Off saved a Quick Resume frame")
                if state.get("showBootScreen") is not True:
                    raise AssertionError(f"Turn Off did not arm the boot splash: {state}")
                if state.get("lastSleepFromReader") is not reader:
                    raise AssertionError(f"Turn Off saved lastSleepFromReader={state.get('lastSleepFromReader')}")
                checked.append("no Quick Resume frame, splash armed, lastSleepFromReader saved")

        if reader:
            progress = epub_progress(state_dir)
            if progress != (0, len(PAGE_TURNS)):
                raise AssertionError(f"reading position not saved before power action: {progress}")
            checked.append(f"reading position {progress} saved")

        saved = json.loads(settings_path.read_text())
        for key, value in case.settings.items():
            if saved.get(key) != value:
                raise AssertionError(f"{key} did not survive: {saved.get(key)} != {value}")
        checked.append("shortcut setting kept")

        if case.after == "off" and case.action == "turn-off":
            # Power on: the hold itself cannot be simulated, so boot as the Power wake it would cause.
            output = run_program(program, work, simulator_env(AFTER_REBOOT_SCRIPT,
                                                              {"CROSSPOINT_SIM_WAKE_REASON": "power"}))
            if logs:
                (logs / f"{env_name}-{case.name}-power-on.log").write_text(output)
            entered = activities(output)
            if "Wake route: PowerButton" not in output or not entered or entered[0] != "Boot" or \
                    start_activity not in entered:
                raise AssertionError(f"power-on after Turn Off: {entered}\n{output[-3000:]}")
            checked.append(f"power-on shows the splash and resumes {start_activity}")
    return checked


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--env", default="x4-pro-simulator")
    parser.add_argument("--program", type=Path, help="Simulator binary (default: .pio/build/<env>/program)")
    parser.add_argument("--cases", nargs="+", choices=CASES, default=list(CASES))
    parser.add_argument("--logs", type=Path, help="Directory for each case's simulator log")
    parser.add_argument("--jobs", type=int, default=1)
    args = parser.parse_args()
    program = (args.program or ROOT / ".pio" / "build" / args.env / "program").resolve()
    if not program.is_file():
        parser.error(f"Simulator program not found: {program} (run: pio run -e {args.env})")

    check_persisted_ids()
    names = [name for name in args.cases if not CASES[name].touch or args.env in TOUCH_ENVS]
    for name in sorted(set(args.cases) - set(names)):
        print(f"SKIP {args.env} {name}: needs {' or '.join(TOUCH_ENVS)}", flush=True)
    failures = []
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        futures = {pool.submit(run_case, args.env, program, CASES[name], args.logs): name for name in names}
        for future in as_completed(futures):
            name = futures[future]
            try:
                print(f"PASS {args.env} {name}: {'; '.join(future.result())}", flush=True)
            except Exception as error:  # report every case, then fail
                failures.append(name)
                print(f"FAIL {args.env} {name}: {error}", flush=True)
    if failures:
        print(f"{len(failures)} of {len(names)} power action case(s) failed: {', '.join(sorted(failures))}",
              file=sys.stderr)
        return 1
    print(f"All {len(names)} power action cases passed on {args.env}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
