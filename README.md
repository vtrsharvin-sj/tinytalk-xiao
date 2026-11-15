# TinyTalk XIAO

**Offline chat on a Seeed XIAO ESP32-S3 Sense.** Flash once from the browser; the board starts its own Wi-Fi AP and serves the chat interface. A legacy USB terminal client is included.

> This is a compact educational model demo, not a general-purpose assistant. Responses are short and can be wrong. The bundled TinyTalk v1 model is fine-tuned with locally generated replies. See [model terms](MODEL-LICENSE.md) before redistribution; the Qwen teacher license restricts use of its materials and derivatives to non-commercial purposes.

## Quick start

1. Open the GitHub Pages site in Chrome or Edge on a computer.
2. Connect a XIAO ESP32-S3 Sense by USB and choose **Connect and flash**.
3. Join Wi-Fi **TinyTalk-XIAO**, password **needlechat**.
4. Open **http://192.168.4.1/** to chat with the model running on the board.

The Pages site is used to install firmware. Once installed, chat requests go to the XIAO over its local access point; there is no cloud model or remote API.

## Legacy USB terminal

Install Python 3 and PySerial, then run `py legacy/tinychat.py COM9` (replace the port if needed). Or run `legacy/Chat-TinyTalk2.bat`. Close Arduino Serial Monitor first. Type `/new` to clear the short conversation.

## Build firmware

The model and tokenizer data are included in `firmware/main/`. With ESP-IDF installed, run `idf.py -C firmware build`. The flash mode is DIO for this board. The current prebuilt image is `docs/firmware/factory.bin`; it is also offered by the web flasher.

## Device limits

- XIAO ESP32-S3 Sense, 8 MB flash and octal PSRAM
- TinyTalk v1 quantized Q4_0, embedded in firmware
- 80-token KV context and up to 24 generated tokens per reply
- Two concurrent Wi-Fi clients, one inference at a time
- AP SSID `TinyTalk-XIAO`, passphrase `needlechat` (change it in `firmware/main/main.cpp` before sharing)

## Licenses and attribution

- Firmware source: MIT; see [`LICENSE`](LICENSE). Adapted from [TheREZOR/cardputer-ai](https://github.com/TheREZOR/cardputer-ai).
- TinyTalk base model: CC BY 4.0, attribution TheREZOR, TinyTalk. The fine-tune also uses replies generated with Qwen2.5-3B-Instruct under its non-commercial Qwen Research License; see [model terms](MODEL-LICENSE.md), [NOTICE](NOTICE), and [license text](QWEN-RESEARCH-LICENSE.txt).
- GPT-2 tokenizer data is included in the model artifacts; see the upstream model card and source notices.

## GitHub Pages

Live flasher: https://vtrsharvin-sj.github.io/tinytalk-xiao/

GitHub Pages publishes the `docs/` folder directly from the `main` branch. The flasher and factory image are public downloads. GitHub Pages is public even if repository visibility changes.
