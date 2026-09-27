# ForumCE

ForumCE is an online, text-first forum platform for the **TI-84 Plus CE**.

- Website: https://forumce.com
- Downloads: https://github.com/Zytue67/ForumCE-Releases/releases/latest
- Current version: **ForumCE v1.0.0**

## Before installing ForumCE

Your TI-84 Plus CE needs a few things first:

1. **arTIfiCE** — your calculator must be jailbroken/enabled to run native programs: https://github.com/YvanTT/arTIfiCE
2. **CE Libraries** — install the required CE libraries: https://github.com/CE-Programming/libraries
3. **Cesium** — recommended for launching and managing ForumCE: https://github.com/mateoconlechuga/cesium

Set those up once, then continue with the ForumCE installation below.

## What you need

- A TI-84 Plus CE
- A USB cable
- A ForumCE account from https://forumce.com
- **TI Connect CE** from Texas Instruments
- **ForumCE Connect** for macOS

## Install ForumCE

### 1. Install ForumCE Connect

Download `ForumCE-Connect-v1.0-macOS.dmg` from the latest ForumCE release.

Open the DMG, then drag **ForumCE Connect** into your Applications folder.

If macOS blocks the app the first time, right-click **ForumCE Connect**, choose **Open**, then confirm. If that doesn't work, press the ? at the top-right of the pop-up window, navigate down to the privacy settings link and click it, scroll down to the bottom and press the "allow" button to open the app.

### 2. Install ForumCE on your calculator

Open ForumCE Connect and click **Download / Repair Calculator**.

After the files are verified, click **Open Folder**.

Open TI Connect CE and send **all three** files to the calculator:

- `FORUMCE.8xp`
- `FORUMCE.8xp.0.8xv`
- `FORUMCE.8xp.1.8xv`

If TI Connect CE asks whether to replace existing files, choose **Replace**.

All three files must always be installed together.

### 3. Connect to ForumCE

1. Open **ForumCE Connect** on your Mac.
2. Sign in with your ForumCE account.
3. Connect your TI-84 Plus CE with USB.
4. Start ForumCE on the calculator.
5. Wait for ForumCE Connect to show:
   - **Server — Online**
   - **Calculator — Connected**
   - **Bridge — Running**

You can now use ForumCE from the calculator.

## Updating ForumCE

ForumCE Connect checks for calculator updates automatically.

When an update is available:

1. Click **Download Calculator Update**.
2. Wait for ForumCE Connect to verify the files.
3. Close ForumCE on the calculator.
4. Click **Open Folder**.
5. Open TI Connect CE.
6. Send all three ForumCE files to the calculator.
7. Choose **Replace** if prompted.
8. Start ForumCE again.

## If ForumCE Connect will not open

You can run the ForumCE bridge manually from Terminal instead.

First, make sure the three ForumCE calculator files are already installed with TI Connect CE.

Download `ForumCE-Bridge-macOS-v1.0.zip` from the latest ForumCE release, extract it, then open Terminal and run:

```bash
cd ~/Downloads/ForumCE-Bridge-macOS-v1.0
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
python3 bridge.py
```

The bridge will ask for your ForumCE username and password the first time.

Then:

1. Connect the calculator by USB.
2. Start ForumCE on the calculator.
3. Leave the Terminal window open while using ForumCE.

The manual bridge connects directly to the production ForumCE server at:

`https://api.forumce.com`

## Troubleshooting

**Calculator is not detected**

- Make sure ForumCE is actually running on the calculator.
- Reconnect the USB cable.
- Try another USB port or cable.
- Restart ForumCE Connect.

**ForumCE Connect says the server is offline**

Check https://forumce.com and try again after a moment.

**The calculator files will not install**

Make sure you are sending all three ForumCE files with TI Connect CE and replacing the old copies when asked.

**ForumCE Connect is blocked by macOS**

Right-click the app in Applications and choose **Open**.

## Contact and Support

Visit https://forumce.com/support for ForumCE downloads and support information.
