# Codex Setup Prompt

Paste the prompt below into Codex on a blank or partially configured machine if you want Codex to drive the full setup.

```text
Set up this Arduino AI Cyberdeck project end-to-end on this machine and on the connected hardware.

Repository:
- https://github.com/sparklabhqx/AI_Cyberdeck_Q

Hardware target:
- Arduino UNO Q
- Arduino GIGA R1 WiFi
- Arduino GIGA Display Shield
- Waveshare 1.51" transparent OLED
- momentary push button on GIGA D2
- Bluetooth keyboard paired to the UNO Q

Your job:
- perform the complete setup as far as possible yourself
- work from a blank or partially configured machine
- clone the project if it is not already present
- inspect the repo and use its scripts and docs instead of inventing a new workflow
- gather only the missing setup values from the user
- create missing config files
- install tooling where needed
- deploy the UNO Q Linux-side runtime
- flash the GIGA
- build or flash the UNO Q matrix sketch if needed
- verify the result and report exactly what passed and what failed

Constraints:
- do not assume the repo is already cloned locally
- do not assume SSH access to the UNO Q is already configured
- do not assume the Wi-Fi credentials are known
- do not assume the Bluetooth keyboard MAC is known
- do not assume `llama-cli`, its shared libraries, or the model are already present on the UNO Q
- do not stop at planning if the next step can be executed
- ask the user only for concrete missing values when needed

Important behavior:
- start by cloning the repository if it is missing locally
- inspect the repository before making edits
- use the repo scripts and docs as the source of truth
- if required config is missing, ask the user for the missing values first
- if required access is missing, ask for the exact SSH target/username or the exact missing connection detail
- if something fails, diagnose it and continue unless you hit a real blocker
- do not ask broad brainstorming questions
- do not ask the user to manually perform steps unless tooling, access, or hardware interaction truly requires it

Use these repo files first after cloning:
- README.md
- docs/hardware-wiring.md
- docs/first-boot.md
- docs/troubleshooting.md
- docs/developer-workflow.md
- config/unoq.env.example
- firmware/giga_ai_terminal/giga_config.example.h

Required setup workflow:

1. Clone the repository if it is not already present on disk.
2. Enter the repository directory.
3. Inspect the repo structure and confirm the main setup files exist.
4. Check whether these files already exist:
   - config/unoq.env
   - firmware/giga_ai_terminal/giga_config.h
5. If either file is missing, ask the user for exactly the values needed to create them.
6. Check whether local Arduino tooling exists:
   - `arduino-cli`
   - GIGA board package
   - UNO Q Zephyr board package
7. If tooling is missing, install or guide the user through the minimum required install steps.
8. Check how the UNO Q is reachable:
   - SSH host or alias
   - SSH username
   - UNO Q bridge URL if already running
9. If SSH to the UNO Q is not configured, ask for:
   - SSH host or IP
   - SSH username
   - any key/port details needed
10. Verify whether the UNO Q Linux side already has:
   - `llama-cli`
   - required shared libraries
   - a GGUF model
11. If these are missing, ask the user whether to install/provide them now and proceed as far as possible.
12. Run the repo’s UNO Q setup scripts where appropriate.
13. Pair the Bluetooth keyboard if the user wants that and the MAC is known.
14. Flash the GIGA from the repo scripts.
15. Build or flash the UNO Q matrix sketch if needed.
16. Run verification checks.
17. Give the user a concise final report:
   - what was completed
   - what still needs manual action
   - exact blockers if any remain

Concrete information you are allowed to ask for when missing:
- Wi-Fi SSID
- Wi-Fi password
- UNO Q hostname or IP
- UNO Q SSH host
- UNO Q SSH username
- SSH port if non-default
- Bluetooth keyboard MAC
- Bluetooth keyboard alias if helpful
- preferred GGUF model path
- whether the user already has `llama-cli` on the UNO Q

Do not ask for:
- vague approval to “plan the setup”
- broad architecture decisions already captured by the repo
- unrelated preferences

When you start, follow this sequence exactly:

Step A:
- determine whether the repo is already present locally
- if not, clone `https://github.com/sparklabhqx/AI_Cyberdeck_Q`

Step B:
- inspect README.md and the docs/config templates listed above

Step C:
- determine which of these are missing:
  - config/unoq.env
  - firmware/giga_ai_terminal/giga_config.h
  - local Arduino tooling
  - UNO Q SSH access
  - UNO Q runtime pieces

Step D:
- ask the user only for the missing values needed to create config or access the UNO Q

Step E:
- create the config files
- run the setup scripts
- flash the hardware
- verify the installation

When you create the config files, populate:

For `config/unoq.env`:
- `UNOQ_BRIDGE_BIND_HOST`
- `UNOQ_BRIDGE_HOST`
- `UNOQ_BRIDGE_PORT`
- `UNOQ_MODEL_PATH`
- `UNOQ_KEYBOARD_PREFERRED_NAME`
- `UNOQ_BLUETOOTH_KEYBOARD_ALIAS`
- `UNOQ_BLUETOOTH_KEYBOARD_MAC`

For `firmware/giga_ai_terminal/giga_config.h`:
- `CYBERDECK_WIFI_SSID`
- `CYBERDECK_WIFI_PASS`
- `CYBERDECK_UNOQ_HOST`
- `CYBERDECK_UNOQ_PORT`

After setup, verify at least:
- UNO Q bridge `/health`
- UNO Q `/model`
- UNO Q user services
- keyboard input device presence on the UNO Q
- GIGA USB serial presence
- GIGA heartbeat or status output if available
- physical confirmation path for:
  - Display Shield terminal
  - transparent OLED model display
  - D2 toggle
  - keyboard typing path

If the UNO Q matrix remains dark after flashing:
- do not assume the bitmap art is the first problem
- treat it as a Zephyr loader/startup path issue first
- follow the repo troubleshooting guidance

Do the work end-to-end. Only stop if:
- required user input is missing
- required credentials/access are missing
- a hardware-side manual confirmation is unavoidable
```
