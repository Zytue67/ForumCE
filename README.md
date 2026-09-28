# ForumCE

ForumCE is an online, text-first forum platform for the **TI-84 Plus CE**.

- Website: https://forumce.com
- Downloads: https://github.com/Zytue67/ForumCE-Releases/releases/latest
- Current version: **ForumCE v1.1.0**
- Desktop support: **macOS (Apple Silicon + Intel) and Windows 10/11 x64**

## Before installing ForumCE

Your TI-84 Plus CE needs a few things first:

1. **arTIfiCE** — enables native programs on supported calculator OS versions: https://github.com/YvanTT/arTIfiCE
2. **CE Libraries** — install the required CE libraries using the official instructions: https://github.com/CE-Programming/libraries
3. **Cesium** — recommended for launching and managing ForumCE: https://github.com/mateoconlechuga/cesium

## What you need

- A TI-84 Plus CE
- A USB data cable
- A ForumCE account from https://forumce.com
- **TI Connect CE** from Texas Instruments
- **ForumCE Connect** for macOS or Windows

## Install ForumCE

### 1. Install ForumCE Connect

Open the [latest ForumCE release](https://github.com/Zytue67/ForumCE-Releases/releases/latest) and download the build for your computer:

- `ForumCE-Connect-v1.1-macOS-Apple-Silicon.dmg`
- `ForumCE-Connect-v1.1-macOS-Intel.dmg`
- `ForumCE-Connect-v1.1.0-Windows-x64.exe`

**macOS:** open the DMG and drag **ForumCE Connect** into Applications. Because the app is currently ad-hoc signed, macOS may require you to right-click the app and choose **Open** on first launch.

**Windows:** run the `.exe`. Because the app is not yet code-signed, Windows SmartScreen may show an unknown/uncommon app warning. Use **More info → Run anyway** only if you downloaded ForumCE Connect from the official ForumCE release.

Packaged builds include their Python runtime and dependencies. A separate Python installation is not required.

### 2. Install ForumCE on your calculator

Open ForumCE Connect and click **Download / Repair Calculator**, or download these three files directly from the release:

- `FORUMCE.8xp`
- `FORUMCE.8xp.0.8xv`
- `FORUMCE.8xp.1.8xv`

Use TI Connect CE to send **all three** files to the calculator. If TI Connect CE asks whether to replace existing files, choose **Replace**.

All three ForumCE files must be installed together.

### 3. Connect to ForumCE

1. Open **ForumCE Connect**.
2. Sign in with your ForumCE account.
3. Close TI Connect CE after transferring files so it does not compete for the calculator connection.
4. Connect your TI-84 Plus CE by USB.
5. Start ForumCE on the calculator.
6. Wait for ForumCE Connect to show:
   - **Server — Online**
   - **Calculator — Connected**
   - **Bridge — Running**

You can now use ForumCE from the calculator.

## ForumCE v1.1.0

v1.1.0 adds official **Windows 10/11 x64 support** to ForumCE Connect while keeping the existing macOS support. The calculator client and ForumCE server protocol are unchanged from v1.0.0.

Windows support was tested with a physical TI-84 Plus CE, including forum loading, posts/replies, direct messages, reactions/notifications, and USB reconnecting.

## Updating ForumCE

ForumCE Connect checks the official release channel for calculator updates and verifies downloaded calculator files before installation.

## macOS manual bridge fallback

If the macOS desktop app cannot be used, the standalone macOS bridge is available from the release page. See the release repository documentation for the manual Terminal setup.

## Troubleshooting

**Calculator is not detected**

- Make sure ForumCE is running on the calculator.
- Close TI Connect CE after file transfer.
- Reconnect the USB cable.
- Try another USB data port/cable.
- Restart ForumCE Connect.

**ForumCE Connect says the server is offline**

Check https://forumce.com and your internet connection, then try again.

**ForumCE reports a missing CE library**

Follow the official CE Libraries installation instructions and make sure the required libraries were actually transferred to the calculator.

**Windows SmartScreen blocks ForumCE Connect**

Make sure the file came from the official ForumCE release, then choose **More info → Run anyway**. ForumCE Connect is not yet commercially code-signed.

## Contact and Support

- Website: https://forumce.com
- Support: https://forumce.com/support
- Email: forumce.dev@gmail.com
