#!/usr/bin/env python3
import os
import re
import struct
import subprocess
import sys
import textwrap
import time
import urllib.parse
import urllib.request


DEVICE = os.environ.get("UNOQ_KEYBOARD_DEVICE", "").strip()
PREFERRED_KEYBOARD_NAME = os.environ.get("UNOQ_KEYBOARD_PREFERRED_NAME", "").strip().lower()
BLUETOOTH_KEYBOARD_ALIAS = os.environ.get(
    "UNOQ_BLUETOOTH_KEYBOARD_ALIAS",
    "Bluetooth Keyboard",
).strip().lower()
BLUETOOTH_KEYBOARD_MAC = os.environ.get(
    "UNOQ_BLUETOOTH_KEYBOARD_MAC",
    "",
).strip()
START_BRIDGE = os.path.expanduser("~/start-giga-wifi-bridge.sh")
LLAMA_CLI = os.path.expanduser("~/bin/llama-cli")
MODEL_GLOB = os.path.expanduser("~/models")
DEFAULT_MODEL = os.path.expanduser(
    os.environ.get(
        "UNOQ_MODEL_PATH",
        "~/models/qwen2.5-0.5b-instruct-q2_k.gguf",
    )
)
SCREEN_WIDTH = 58
MAX_SCREEN_LINES = 18
HISTORY_LINES = MAX_SCREEN_LINES - 6
RENDER_INTERVAL = 0.04
REOPEN_DELAY = 1.0
PROMPT_PREFIX = "Prompt>> "
RESPONSE_PREFIX = "QWEN>> "
RESPONSE_SEPARATOR = "-" * 29
EVENT_FORMAT = "llHHI"
EVENT_SIZE = struct.calcsize(EVENT_FORMAT)
INPUT_DEVICES = "/proc/bus/input/devices"
BLUETOOTHCTL = "bluetoothctl"

BRIDGE_HOST = os.environ.get("UNOQ_BRIDGE_HOST", "127.0.0.1").strip() or "127.0.0.1"
BRIDGE_PORT = int(os.environ.get("UNOQ_BRIDGE_PORT", "8080"))
BRIDGE_PUSH_URL = f"http://{BRIDGE_HOST}:{BRIDGE_PORT}/push?text="
BRIDGE_HEALTH_URL = f"http://{BRIDGE_HOST}:{BRIDGE_PORT}/health"

KEYMAP = {
    2: ("1", "!"),
    3: ("2", "@"),
    4: ("3", "#"),
    5: ("4", "$"),
    6: ("5", "%"),
    7: ("6", "^"),
    8: ("7", "&"),
    9: ("8", "*"),
    10: ("9", "("),
    11: ("0", ")"),
    12: ("-", "_"),
    13: ("=", "+"),
    16: ("q", "Q"),
    17: ("w", "W"),
    18: ("e", "E"),
    19: ("r", "R"),
    20: ("t", "T"),
    21: ("y", "Y"),
    22: ("u", "U"),
    23: ("i", "I"),
    24: ("o", "O"),
    25: ("p", "P"),
    26: ("[", "{"),
    27: ("]", "}"),
    30: ("a", "A"),
    31: ("s", "S"),
    32: ("d", "D"),
    33: ("f", "F"),
    34: ("g", "G"),
    35: ("h", "H"),
    36: ("j", "J"),
    37: ("k", "K"),
    38: ("l", "L"),
    39: (";", ":"),
    40: ("'", '"'),
    41: ("`", "~"),
    43: ("\\", "|"),
    44: ("z", "Z"),
    45: ("x", "X"),
    46: ("c", "C"),
    47: ("v", "V"),
    48: ("b", "B"),
    49: ("n", "N"),
    50: ("m", "M"),
    51: (",", "<"),
    52: (".", ">"),
    53: ("/", "?"),
    57: (" ", " "),
}


def log(message: str) -> None:
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    print(f"[{timestamp}] {message}", flush=True)


def bridge_ok() -> bool:
    try:
        with urllib.request.urlopen(BRIDGE_HEALTH_URL, timeout=1.5) as response:
            return response.read().decode().strip() == "OK"
    except Exception:
        return False


def ensure_bridge() -> None:
    if bridge_ok():
        return
    subprocess.Popen(
        [START_BRIDGE],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        stdin=subprocess.DEVNULL,
        start_new_session=True,
    )
    time.sleep(1.0)
    if not bridge_ok():
        raise RuntimeError(
            f"UNO Q bridge is not reachable on {BRIDGE_HOST}:{BRIDGE_PORT}"
        )


def send_payload(text: str) -> None:
    encoded = urllib.parse.quote(text, safe="")
    try:
        with urllib.request.urlopen(BRIDGE_PUSH_URL + encoded, timeout=2.0):
            return
    except Exception:
        ensure_bridge()
        with urllib.request.urlopen(BRIDGE_PUSH_URL + encoded, timeout=2.0):
            return


def model_label(path: str) -> str:
    return os.path.basename(path)


def trim_line(text: str, width: int = SCREEN_WIDTH) -> str:
    if len(text) <= width:
        return text
    if width <= 3:
        return text[:width]
    return text[: width - 3] + "..."


def discover_keyboard_device() -> tuple[str, str]:
    with open(INPUT_DEVICES, "r", encoding="utf-8") as handle:
        blocks = handle.read().strip().split("\n\n")

    candidates: list[tuple[int, str, str]] = []
    for block in blocks:
        name = ""
        handlers = ""
        for line in block.splitlines():
            if line.startswith("N: Name="):
                name = line.split("=", 1)[1].strip().strip('"')
            elif line.startswith("H: Handlers="):
                handlers = line.split("=", 1)[1].strip()

        if not name or not handlers:
            continue

        if "kbd" not in handlers:
            continue

        event_match = re.search(r"\bevent\d+\b", handlers)
        if not event_match:
            continue

        lowered = name.lower()
        if any(
            blocked in lowered
            for blocked in (
                "consumer control",
                "system control",
                "mouse",
                "pwrkey",
                "gpio-keys",
                "headset jack",
                "hph-lout",
                "media",
            )
        ):
            continue

        if "keyboard" not in lowered and "bluetooth" not in lowered:
            continue

        score = 0
        if PREFERRED_KEYBOARD_NAME and PREFERRED_KEYBOARD_NAME in lowered:
            score += 100
        if "bluetooth" in lowered:
            score += 20
        if "keyboard" in lowered:
            score += 5
        if "receiver" in lowered:
            score -= 3
        if "usb" in lowered:
            score -= 2
        if "logitech" in lowered and "bluetooth" not in lowered:
            score -= 1
        candidates.append((score, event_match.group(0), name))

    if not candidates:
        raise FileNotFoundError("No keyboard input device found in /proc/bus/input/devices")

    _, event_name, name = max(candidates, key=lambda item: (item[0], item[1]))
    return f"/dev/input/{event_name}", name


def bluetooth_keyboard_connected() -> bool:
    address = resolve_bluetooth_keyboard_address()
    if not address:
        return False

    try:
        result = subprocess.run(
            [BLUETOOTHCTL, "info", address],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=4,
        )
    except Exception:
        return False

    return "Connected: yes" in result.stdout


def resolve_bluetooth_keyboard_address() -> str:
    if BLUETOOTH_KEYBOARD_MAC:
        return BLUETOOTH_KEYBOARD_MAC

    try:
        result = subprocess.run(
            [BLUETOOTHCTL, "devices"],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=4,
        )
    except Exception:
        return ""

    for line in result.stdout.splitlines():
        match = re.match(r"Device\s+([0-9A-F:]+)\s+(.+)", line.strip(), re.IGNORECASE)
        if not match:
            continue
        address, alias = match.groups()
        if BLUETOOTH_KEYBOARD_ALIAS and BLUETOOTH_KEYBOARD_ALIAS in alias.lower():
            return address
    return ""


def ensure_bluetooth_keyboard_connected() -> None:
    address = resolve_bluetooth_keyboard_address()
    if not address or bluetooth_keyboard_connected():
        return

    commands = (
        [BLUETOOTHCTL, "connect", address],
        [BLUETOOTHCTL, "pair", address],
        [BLUETOOTHCTL, "trust", address],
        [BLUETOOTHCTL, "connect", address],
    )
    for command in commands:
        try:
            subprocess.run(
                command,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=8,
                check=False,
            )
        except Exception:
            continue
        time.sleep(1.0)
        if bluetooth_keyboard_connected():
            return


def wrapped_lines(text: str) -> list[str]:
    lines: list[str] = []
    for raw in text.splitlines():
        clean = raw.rstrip()
        if not clean:
            lines.append("")
            continue
        lines.extend(textwrap.wrap(clean, width=SCREEN_WIDTH) or [""])
    return lines


def wrapped_prefixed_lines(prefix: str, text: str, width: int = SCREEN_WIDTH) -> list[str]:
    lines: list[str] = []
    for raw in text.splitlines():
        clean = raw.rstrip()
        if not clean:
            if prefix:
                lines.append(prefix.rstrip())
                prefix = ""
            else:
                lines.append("")
            continue

        if prefix:
            first_width = max(1, width - len(prefix))
            first_chunk = clean[:first_width]
            lines.append(prefix + first_chunk)
            prefix = ""
            clean = clean[first_width:]

        while clean:
            chunk = clean[:width]
            lines.append(chunk)
            clean = clean[width:]

    if not lines:
        return [prefix.rstrip() if prefix else ""]
    return lines


def clean_llm_output(text: str) -> str:
    text = text.replace("\r", "")
    text = re.sub(r"\x08.\x08", "", text)
    text = re.sub(r"[\x00-\x08\x0b-\x1f]", "", text)
    cleaned: list[str] = []
    capture = False
    for line in text.splitlines():
        if line.startswith("> "):
            if not capture:
                capture = True
                continue
        if not capture:
            continue
        if line.startswith("[ Prompt:"):
            continue
        if line in {"Exiting...", ">", ""}:
            continue
        cleaned.append(line)
    return "\n".join(cleaned).strip()


def first_model() -> str:
    if os.path.isfile(DEFAULT_MODEL):
        return DEFAULT_MODEL
    for entry in sorted(os.listdir(MODEL_GLOB)):
        if entry.endswith(".gguf"):
            return os.path.join(MODEL_GLOB, entry)
    raise FileNotFoundError("No GGUF model found in ~/models")


def run_llm(prompt: str) -> str:
    model = first_model()
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = f"{os.path.expanduser('~/lib')}:{env.get('LD_LIBRARY_PATH', '')}"
    result = subprocess.run(
        [
            LLAMA_CLI,
            "-m",
            model,
            "-c",
            "64",
            "-n",
            "32",
            "-t",
            "4",
            "-p",
            prompt,
            "--simple-io",
            "--no-display-prompt",
            "--no-perf",
            "--conversation",
            "--single-turn",
        ],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        timeout=90,
        env=env,
    )
    output = clean_llm_output(result.stdout)
    return output or "ERROR: LLM returned no printable output."


class TerminalState:
    def __init__(self) -> None:
        self.history = []
        self.current = ""
        self.shift = False
        self.caps = False
        self.bridge_state = "BOOT"
        self.keyboard_path = "detecting"
        self.keyboard_name = "waiting"
        self.model_path = first_model()
        self.last_render = ""
        self.last_render_monotonic = 0.0
        self.notice = "Keyboard active. Enter sends to local LLM."

    def append_history(self, text: str) -> None:
        self.history.extend(wrapped_lines(text))
        self.history = self.history[-HISTORY_LINES:]

    def set_keyboard(self, path: str, name: str) -> None:
        self.keyboard_path = path
        self.keyboard_name = name
        self.notice = f"Keyboard {os.path.basename(path)}: {name}"

    def set_bridge_state(self, state: str) -> None:
        self.bridge_state = state

    def status_lines(self) -> list[str]:
        top = trim_line(f"UNO Q USB TERMINAL  BRIDGE:{self.bridge_state}")
        bottom = trim_line(
            f"KB:{os.path.basename(self.keyboard_path)}  MODEL:{model_label(self.model_path)}"
        )
        return [top, bottom]

    def screen_text(self) -> str:
        visible = self.history[-HISTORY_LINES:]
        prompt_line = f"{PROMPT_PREFIX}{self.current}_"
        lines = ["/clear", *visible, prompt_line]
        return "\n".join(lines)

    def append_prompt_line(self, prompt: str) -> None:
        self.append_history(f"{PROMPT_PREFIX}{prompt}")

    def append_llm_response(self, text: str) -> None:
        self.append_history(RESPONSE_SEPARATOR)
        self.history.extend(wrapped_prefixed_lines(RESPONSE_PREFIX, text))
        self.history = self.history[-HISTORY_LINES:]

    def render(self, force: bool = False) -> None:
        screen = self.screen_text()
        now = time.monotonic()
        if not force and screen == self.last_render:
            return
        if not force and now - self.last_render_monotonic < RENDER_INTERVAL:
            return
        send_payload(screen)
        self.last_render = screen
        self.last_render_monotonic = now

    def run_local_command(self, prompt: str) -> str | None:
        if prompt == "/help":
            return "Commands: /help /clear /divider /model"
        if prompt == "/clear":
            self.history.clear()
            self.notice = "Screen cleared."
            return None
        if prompt == "/divider":
            self.append_history("-" * SCREEN_WIDTH)
            self.notice = "Divider inserted."
            return None
        if prompt == "/model":
            return f"MODEL: {self.model_path}"
        return None

    def submit(self) -> None:
        prompt = self.current.strip()
        if not prompt:
            return
        command_output = self.run_local_command(prompt)
        self.append_prompt_line(prompt)
        self.current = ""
        if command_output is not None:
            self.append_history(command_output)
            self.append_history("")
            self.render(force=True)
            return
        if prompt in {"/clear", "/divider"}:
            self.append_history("")
            self.render(force=True)
            return
        self.append_history(RESPONSE_SEPARATOR)
        self.history.extend(wrapped_prefixed_lines(RESPONSE_PREFIX, "THINKING..."))
        self.history = self.history[-HISTORY_LINES:]
        self.notice = "Running local model on UNO Q..."
        self.render(force=True)

        if self.history:
            while self.history and self.history[-1] != RESPONSE_SEPARATOR:
                self.history.pop()
            if self.history and self.history[-1] == RESPONSE_SEPARATOR:
                self.history.pop()

        try:
            self.model_path = first_model()
            output = run_llm(prompt)
            self.notice = "Model response received."
        except subprocess.TimeoutExpired:
            output = "ERROR: LLM timed out."
            self.notice = "LLM timeout."
        except Exception as exc:
            output = f"ERROR: {exc}"
            self.notice = "LLM error."

        self.append_llm_response(output)
        self.append_history("")
        self.render(force=True)

    def key_to_text(self, code: int) -> str | None:
        if code not in KEYMAP:
            return None
        base, shifted = KEYMAP[code]
        if base.isalpha():
            return shifted if self.shift ^ self.caps else base
        return shifted if self.shift else base

    def handle_key(self, code: int, value: int) -> None:
        if code in (42, 54):
            self.shift = value != 0
            return
        if value not in (1, 2):
            return
        if code == 58 and value == 1:
            self.caps = not self.caps
            return
        if code == 14:
            self.current = self.current[:-1]
            self.render()
            return
        if code == 28:
            self.submit()
            return
        text = self.key_to_text(code)
        if text is None:
            return
        self.current += text
        self.render()


def open_keyboard_device() -> tuple[object, str, str]:
    if DEVICE:
        return open(DEVICE, "rb", buffering=0), DEVICE, "env override"

    ensure_bluetooth_keyboard_connected()
    path, name = discover_keyboard_device()
    return open(path, "rb", buffering=0), path, name


def main() -> None:
    ensure_bridge()
    state = TerminalState()
    state.set_bridge_state("OK")
    state.render(force=True)

    while True:
        try:
            ensure_bridge()
            state.set_bridge_state("OK")
            device, path, name = open_keyboard_device()
            state.set_keyboard(path, name)
            state.render(force=True)
            log(f"Listening for keyboard input on {path} ({name})")

            with device:
                while True:
                    data = device.read(EVENT_SIZE)
                    if len(data) != EVENT_SIZE:
                        continue
                    _, _, event_type, code, value = struct.unpack(EVENT_FORMAT, data)
                    if event_type != 1:
                        continue
                    state.handle_key(code, value)
        except KeyboardInterrupt:
            log("Stopping usb_giga_terminal.py")
            sys.exit(0)
        except Exception as exc:
            state.set_bridge_state("WAIT")
            state.notice = f"Recovering after error: {exc}"
            try:
                state.append_history(f"STATUS: reconnecting after {exc}")
                state.render(force=True)
            except Exception:
                pass
            log(f"Terminal loop error: {exc}")
            time.sleep(REOPEN_DELAY)


if __name__ == "__main__":
    main()
