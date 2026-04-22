# Codex Setup Prompt

Paste the prompt below into Codex if you want Codex to perform the full setup for this project.

```text
Set up this Arduino AI Cyberdeck project end-to-end on this machine and on the connected hardware.

Project root:
- <replace with the local path to this cloned repo>

Your job:
- get the project into a working state on fresh or partially configured hardware
- do the setup, config generation, deployment, flashing, and verification work yourself where possible
- minimize manual user work

Important behavior:
- start by checking whether the required config files already exist
- if config is missing, ask the user for the missing values first before editing or flashing anything
- if SSH access to the UNO Q is not configured, ask the user for the SSH host, username, and any required connection details before proceeding
- if the Bluetooth keyboard MAC is not known, ask the user whether to pair manually or provide the MAC
- if the user has not yet installed the UNO Q model or llama-cli runtime, ask before assuming those files exist

Use these files first:
- README.md
- docs/hardware-wiring.md
- docs/first-boot.md
- docs/troubleshooting.md
- config/unoq.env.example
- firmware/giga_ai_terminal/giga_config.example.h

Required setup flow:
1. Inspect the repo and confirm what files already exist.
2. Check whether these files exist:
   - config/unoq.env
   - firmware/giga_ai_terminal/giga_config.h
3. If either file is missing, ask the user for the required values and create them.
4. Verify local Arduino tooling and board packages:
   - arduino-cli
   - arduino:mbed_giga
   - arduino:zephyr
5. Verify how the UNO Q is reachable:
   - SSH target
   - bridge URL
6. Run the UNO Q setup steps from the repo scripts where appropriate.
7. Pair the Bluetooth keyboard if the user wants that and the MAC is known.
8. Flash the GIGA from the repo scripts.
9. Build or flash the UNO Q matrix sketch if requested or needed.
10. Run the verification checks and report exactly what passed and what failed.

When asking the user for missing setup information, ask only for the concrete values you need next, such as:
- Wi-Fi SSID
- Wi-Fi password
- UNO Q hostname or IP
- UNO Q SSH host
- UNO Q SSH username
- Bluetooth keyboard MAC
- preferred GGUF model path

Do not ask broad planning questions. Gather the missing config, write the files, run the scripts, verify the result, and keep going until the setup is complete or a real blocker remains.
```
