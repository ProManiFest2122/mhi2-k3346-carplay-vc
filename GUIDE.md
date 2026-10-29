# Step-by-step installation guide

[Wersja polska](INSTRUKCJA.md) · [← back to README](README.md)

This guide assumes you **know nothing about this at all**. Do exactly what is written, in order, and
skip nothing. Every step tells you what you should see. If you see something different, **stop** and
go to chapter [6. If something does not work](#6-if-something-does-not-work).

The whole installation takes about 30 minutes, about 15 of them in the car.

---

## 0. Before you start

### 0.1 What it does

After installation the **Virtual Cockpit** (the digital instrument cluster behind the steering wheel)
shows:

- the **CarPlay map** (Apple Maps or Google Maps from your iPhone),
- next to the map, an **arrow** showing where to turn and the **distance** to the turn,
- **iPhone calls**: who is calling, answer and hang up with the steering-wheel button,
- **recent calls** from the iPhone,
- the **album art** of the song that is playing.

Also, a short press of the voice button on the steering wheel starts **Siri**, and the MMI **touchpad**
works as arrow keys in CarPlay.

### 0.2 Does it fit your car

It works **only** in Audi cars with **MHI2 (Harman)** infotainment and firmware
**`MHI2_ER_AUG22_K3346`**, **MU 1438**. Tested in an Audi A4 B9.

You can see the firmware version in the green menu or in M.I.B. (see below). You are looking for two
things: the text **`K3346`** and the number **`1438`**.

- You see `K3346` and `1438` → you can install.
- You see other values → **do not install**. Nothing bad happens if you try: the installer checks the
  version itself and on another unit it **refuses without changing anything**. You just waste your time.

### 0.3 What M.I.B. is and whether you have it

**M.I.B. (More Incredible Bash)** is a free tool on an SD card that lets you run scripts on the MMI.
This mod is installed **through M.I.B.**, so M.I.B. must already work in your car.

How to check whether you have M.I.B.:

1. Switch the ignition on and wait until the MMI has started.
2. Open the **green menu**. On A4 / A5 / Q5 (B9, 2017–2020) hold the **NAV/MAP** and **RADIO** buttons
   (the two next to the MMI knob) together for a few seconds, until a screen with green text appears.
   Other models use other buttons — see the table in the [M.I.B. wiki](https://wiki.mibsolution.one/en/MHI2).
3. In the green menu list, look for the entry **`m.i.b.`**.

- `m.i.b.` is there → you have M.I.B., go on.
- No `m.i.b.` → install M.I.B. first, following the [M.I.B. wiki](https://wiki.mibsolution.one/en/MHI2),
  then come back here. This guide does not cover installing M.I.B. itself.

To leave the green menu, hold the same two buttons again or press the back button.

### 0.4 What to prepare

- [ ] **the SD card with M.I.B.** — the same one M.I.B. runs from in your car,
- [ ] a **computer** (Windows or Mac) with an **SD card reader**,
- [ ] a **battery charger or power supply** — the engine will be off and the ignition on for 15 minutes
      or more; a weak battery during installation is the silliest way to break something,
- [ ] the **iPhone** and the **cable** you normally use in the car,
- [ ] **about 30 minutes** without hurry.

### 0.5 MMI backup (recommended)

Before you change anything, make an MMI backup to the card with M.I.B. (the `m.i.b.` menu has a backup
entry; see the [M.I.B. wiki](https://wiki.mibsolution.one/en/MHI2)). This mod has its own backup and its
own uninstall, but the M.I.B. backup is an extra safety net. After the backup, copy it from the card to
your computer.

---

## 1. Download the file (on the computer)

1. Go to the **[Releases](../../releases/latest)** page of this project.
2. Scroll down to the **"Assets"** section.
3. Click **`mhi2-k3346-carplay-vc-1.0.0-install.zip`**.
   Do **not** click "Source code" — that is code for developers, not for installing.
4. The file is saved in your **Downloads** folder.
5. Also download **`mhi2-k3346-carplay-vc-1.0.0-uninstall.zip`** now and keep it. You will need it to
   uninstall.

**Do not unzip anything yet.** You will do it in step 2, straight onto the card.

---

## 2. Prepare the card (on the computer)

### 2.1 Put the card into the computer

1. Take the M.I.B. card out of the car.
2. Put it into the card reader of your computer.
3. Open File Explorer (Windows) or Finder (Mac). The card appears as a new drive, e.g. **`E:`** or
   **`D:`** on Windows.
4. Open the card. You see the M.I.B. files and folders, including a folder called **`mod`**.
   **Do not delete anything there and do not format the card.**

### 2.2 Unzip onto the card — Windows

1. In **Downloads**, **right**-click `mhi2-k3346-carplay-vc-1.0.0-install.zip`.
2. Choose **"Extract All…"**.
3. In the window that opens, click **"Browse…"**, choose the **card drive** (e.g. `E:`), then
   **"Select Folder"**.
4. **CAREFUL — almost everybody gets this wrong.** Look at the path field. Windows adds a folder name
   at the end by itself, e.g.:

   `E:\mhi2-k3346-carplay-vc-1.0.0-install`

   **Delete** that ending so that only this is left:

   `E:\`

   (with the letter of your card).
5. Click **"Extract"**.
6. If Windows asks whether to replace files (e.g. `mod\command.sh` from another mod), choose
   **"Replace the files in the destination"**.

### 2.2 Unzip onto the card — Mac

On a Mac **do not drag the whole `mod` folder** onto the card: Finder will offer "Replace", which would
delete the M.I.B. files in the card's `mod` folder. Do it like this:

1. In **Downloads**, double-click `mhi2-k3346-carplay-vc-1.0.0-install.zip`. A folder with the same name
   appears. Open it — inside there is a folder **`mod`** and a file **`README_CarPlay_VC.txt`**.
2. Drag **`README_CarPlay_VC.txt`** into the main window of the card.
3. Open the unzipped **`mod`** folder from Downloads and, in a second window, the **`mod` folder on the card**.
4. From the unzipped `mod`, drag **two things** into `mod` on the card: the folder
   **`mib2high-carplay-rgi-v8s-simple`** and the file **`command.sh`**. If Finder asks about `command.sh`,
   choose **"Replace"** (it only concerns that one file).

### 2.3 Check that everything is in place

This is the most important step before going to the car. Open the card and check:

1. In the **main** window of the card (where the M.I.B. folders are) there is now a file
   **`README_CarPlay_VC.txt`**.
2. Open the **`mod`** folder. It contains a folder **`mib2high-carplay-rgi-v8s-simple`** and a file
   **`command.sh`**.
3. Open **`mib2high-carplay-rgi-v8s-simple`**. It contains:
   - a file **`INSTALL_ALL`**,
   - a file **`RELEASE`**,
   - a folder **`payload`**,
   - a folder **`source`**.

**Typical mistake:** the main window of the card has a folder **`mhi2-k3346-carplay-vc-1.0.0-install`**
with `mod` inside it. That means you did not delete the end of the path in step 2.2. Fix: open that
folder, select its contents (`mod` and `README_CarPlay_VC.txt`), drag them into the main window of the card
(Windows: "Replace the files"; Mac: do it as in step 2.2 for Mac), then delete the now empty folder
`mhi2-k3346-carplay-vc-1.0.0-install`.

### 2.4 Eject the card

1. Windows: right-click the card drive → **"Eject"**. Mac: drag the card to the trash (or click the eject
   symbol next to it).
2. Only then take the card out of the reader.

---

## 3. Installation (in the car)

### 3.1 Prepare the car

1. The car is parked, **engine off**.
2. Connect the **charger or power supply** to the battery.
3. **Switch the ignition on** (without starting the engine).
4. Wait until the MMI has started and the screen works normally.

### 3.2 Insert the card

1. Open the glovebox. On A4 / A5 / Q5 B9 there are usually **two SD card slots** there.
2. Insert the card into slot **SD1** (check the label next to the slot). It is the same slot you normally
   start M.I.B. from.
3. Push the card all the way in until it clicks.

### 3.3 Start the installation

1. Open the **green menu**: hold **NAV/MAP + RADIO** together until the green screen appears (as in step 0.3).
2. Turn the MMI knob to **`m.i.b.`** and **press the knob**.
3. Go to **`advanced settings`** and **press the knob**.
4. Go to **`run individual script`** and **press the knob**.

### 3.4 Wait — do not touch anything

Lines of text starting with **`[V8S]`** start to appear on the screen. That is normal. In order:

1. Firmware version check — the line **`VERSION_GATE=PASS`**.
2. Four parts are installed one after another. Each one ends with a line ending in `RESULT=PASS`.
3. At the end, the two lines you are waiting for:

   **`INSTALL_ALL_RESULT=PASS`**

   **`SAFE TO REMOVE SD`**

This takes **a few minutes**. During that time:

- **do not switch the ignition off,**
- **do not take the card out,**
- **do not press anything on the MMI.**

When you see **`SAFE TO REMOVE SD`**, installation is finished. You can take the card out or leave it in
the car, it does not matter.

**If instead** you see a line with **`ERROR`**, the text **`INSTALL_ALL_RESULT=FAIL`** or
**`do not reboot`**, do nothing else and go to chapter [6](#6-if-something-does-not-work).

---

## 4. Restart the MMI and first start

### 4.1 Restart the MMI

Without this the new features do not switch on.

1. On A4 / A5 / Q5 B9 hold together the **big MMI knob** (pressed) **+ NAV/MAP + RADIO** for about
   **10 seconds**, until the MMI screen goes dark.
   (Other models: button table in the [M.I.B. wiki](https://wiki.mibsolution.one/en/MHI2).)
2. Let go. The MMI starts again — this takes **1–2 minutes**. Wait until the screen works normally.

### 4.2 Connect the iPhone

1. Connect the iPhone with the **cable** to the same USB socket as always.
2. Unlock the iPhone. CarPlay starts on the MMI screen as usual.

### 4.3 Start a route

1. In CarPlay on the MMI screen open **Maps** (Apple) or **Google Maps**.
2. Enter any destination and **start navigation**.

### 4.4 Look at the cluster

1. On the cluster switch to the **navigation / map** view — with the steering-wheel buttons, the same way
   as for the factory navigation.
2. **What you should see:**
   - the CarPlay map on the cluster,
   - next to the map, a **tile with an arrow** showing the next maneuver and the distance to it.
3. Also check:
   - the **VIEW** button on the steering wheel switches big / small gauges; the arrow moves with them
     (it disappears for a split second — that is expected),
   - the **left roller** on the steering wheel zooms the map in and out,
   - when somebody calls, the cluster's **Phone** tab shows who is calling, and you can answer with the
     steering-wheel button,
   - a short press of the voice button starts **Siri**.

If all of that works — **done**. Disconnect the charger.

### 4.5 Keep the copy of the original files

During installation a **copy of the car's original files** was saved on the card. Without it the mod
cannot be uninstalled.

1. Take the card to your computer.
2. Copy the **whole `mod` folder** from the card to your computer (e.g. drag it onto the desktop).
3. **Do not delete** the folder `mod\mib2high-carplay-rgi-v8s-simple\backup` on the card and **do not
   format** this card while the mod is installed.

---

## 5. Uninstall (and before every update)

Uninstalling returns the car to its factory state, file for file. **You must use the same card** you
installed from, because it holds the copy of the original files.

1. Put that card into the computer.
2. Unzip **`mhi2-k3346-carplay-vc-1.0.0-uninstall.zip`** onto the card **exactly as in step 2.2**
   (Windows: delete the end of the path, only `E:\` must remain). Mac: unzip it in Downloads and drag
   only the file **`ROLLBACK`** (from `mod/mib2high-carplay-rgi-v8s-simple/`) into the folder
   `mod/mib2high-carplay-rgi-v8s-simple/` **on the card**.
3. Check: the folder `mod\mib2high-carplay-rgi-v8s-simple` now contains a file **`ROLLBACK`**.
4. Eject the card (step 2.4) and put it into the car, slot **SD1**.
5. Charger, ignition, MMI started (step 3.1).
6. Green menu → **`m.i.b.`** → **`advanced settings`** → **`run individual script`** (step 3.3).
7. Wait without touching anything until you see **`ROLLBACK_RESULT=PASS`** and **`SAFE TO REMOVE SD`**.
8. Restart the MMI (step 4.1).

The car is factory again.

**Updating to a new version:** first uninstall the old version (this chapter), then install the new one
(chapters 1–4). If you run the new version's installation over the old one, nothing changes, and the
screen shows `ALREADY_INSTALLED` with a hint to uninstall first.

---

## 6. If something does not work

| What you see | What it means | What to do |
|---|---|---|
| No `m.i.b.` in the green menu | M.I.B. is not installed. | Install M.I.B. following the [M.I.B. wiki](https://wiki.mibsolution.one/en/MHI2). |
| No `[V8S]` lines after `run individual script` | The card has no `mod/command.sh`, or it is not in the slot M.I.B. runs from. | Check the card as in step 2.3 (most often: unzipped one folder too deep) and the slot SD1. |
| `ERROR VERSION_GATE …` | This is not firmware K3346 / MU 1438. | **Nothing was changed.** This mod is not for your car. |
| `INSTALL_ALL_RESULT=FAIL` | Something interrupted the installation. The car is in a **safe state**: parts installed earlier stay, the part that failed was undone automatically. | The reason is on the line with `ERROR`. Take a photo of it. Run the script again (step 3.3) — it finishes the missing parts. If it says `FAIL` again, uninstall (chapter 5) and report the problem. |
| A line with `do not reboot` | The automatic undo did not work. | **Do not restart the MMI.** Uninstall straight away (chapter 5, steps 1–7). |
| No map on the cluster after the restart | Most often: the cluster is not on the navigation view, or no route is running in CarPlay. | Start a route in CarPlay, switch the cluster to navigation. If still nothing — collect logs and report (below). |
| The MMI does not start after the restart (black screen or keeps restarting) | Very unlikely, but possible. | Wait 5 minutes, restart once more (step 4.1). If it still does not work — see "If the MMI does not start" in the [README](README.md#if-the-mmi-does-not-start). |

### Collecting logs and reporting a problem

1. Right after the problem happens, **without restarting the MMI**, run the script again: green menu →
   `m.i.b.` → `advanced settings` → `run individual script`. With nothing to install, the script
   **collects logs**.
2. Wait for **`COLLECT_RESULT=…`** and **`SAFE TO REMOVE SD`**.
3. The logs are on the card in `mod\mib2high-carplay-rgi-v8s-simple\collected\run-NN` (the highest number
   is the newest).
4. Open an issue on GitHub: **Issues** tab → **New issue** → **Bug report**, and fill in the fields.

**Do not attach the logs to the issue** — they contain your personal data (VIN, contact names, phone
numbers, places from navigation). Mention that you have them; you will send them privately if needed.

If the installation **failed**, running the script again retries the installation instead of collecting
logs. In that case see [README → Reporting a problem](README.md#reporting-a-problem)
(the `…-collect-logs.zip` file).
