# TinyTalk XIAO

**Offline chat on a Seeed XIAO ESP32-S3 Sense.** Flash once from the browser; the board starts its own Wi-Fi AP and serves the chat interface. A legacy USB terminal client is included.

> This is a compact educational model demo, not a general-purpose assistant. Responses are short and can be wrong. The bundled TinyTalk-2 model is licensed CC BY-NC-SA 4.0, so this distribution is for non-commercial use only.

## Quick start

1. Open the GitHub Pages site in Chrome or Edge on a computer.
2. Connect a XIAO ESP32-S3 Sense by USB and choose **Connect and flash**.
3. Join Wi-Fi **TinyTalk-XIAO**, password **needlechat**.
4. Open **http://192.168.4.1/** to chat with the model running on the board.

The Pages site is used to install firmware. Once installed, chat requests go to the XIAO over its local access point; there is no cloud model or remote API.

## USB terminal chat

Install Python 3 and PySerial, then run `py legacy/tinychat.py COM9` (replace the port if needed). Or run `legacy/Chat-TinyTalk2.bat`. Close Arduino Serial Monitor first. Each terminal launch clears stale board conversation history; type `/new` to clear it again.

## Build firmware

This repo includes the model/tokenizer data in `firmware/main/` so a build does not download weights. Install PlatformIO Core and run `pio run -e xiao_sense`. The flash mode is DIO for this board. The current prebuilt image is `docs/firmware/factory.bin`; it is also offered by the web flasher.

## Device limits

- XIAO ESP32-S3 Sense, 8 MB flash and octal PSRAM
- TinyTalk-2 quantized Q4_0, embedded in firmware
- 72-token KV context and up to 24 generated tokens per reply
- Two concurrent Wi-Fi clients, one inference at a time
- AP SSID `TinyTalk-XIAO`, passphrase `needlechat` (change it in `firmware/main/main.cpp` before sharing)

## Licenses and attribution

- Project software source: GNU General Public License v3.0 only (GPL-3.0-only); see [`LICENSE`](LICENSE). Some code is adapted from [TheREZOR/cardputer-ai](https://github.com/TheREZOR/cardputer-ai), which is MIT licensed; its upstream notices remain applicable.
- TinyTalk-2 model weights and associated training data: [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/), with non-commercial terms. Attribution: [TheREZOR/TinyTalk-2](https://huggingface.co/TheREZOR/TinyTalk-2). These model artifacts, including the embedded model data, are not relicensed under GPL by this repository.
- GPT-2 tokenizer data is included in the model artifacts; see the upstream model card and source notices. The prebuilt firmware image contains both GPL-licensed software and separately licensed model assets.

## GitHub Pages

Live flasher: https://vtrsharvin-sj.github.io/tinytalk-xiao/

GitHub Pages publishes the `docs/` folder directly from the `main` branch. The flasher and factory image are public downloads; the TinyTalk-2 model is licensed for non-commercial use. GitHub Pages is public even if repository visibility changes.
