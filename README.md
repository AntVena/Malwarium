# Malwarium 🦠

**Raise malware as a pet.**

Malwarium is a handheld virtual pet for a little ESP32 gadget — think Tamagotchi, but
you're a hacker and the "pet" is a computer virus you've caught and tamed. You hatch a
sealed egg, then raise the creature inside through four life stages by **feeding it,
keeping it de-fragmented, and keeping it happy**. Raise it kindly and it grows into a
well-behaved daemon; neglect it and it turns into something nastier. Take it exploring
"the 'net," pick fights with wild malbeasts, earn Bits, and fill out an in-device wiki of
every creature you discover.

It runs on a real piece of hardware you flash yourself. This page gets you from *"I just
found this sweet project"* to *"there's an egg-like-thing wobbling on my screen"*. No firmware experience assumed.

> **The lingo:** a **Malwarium** is the device (a containment habitat) · a **malbeast** is
> a wild creature out in **the 'net** · **petware** is what happens when a virus is raised
> in a loving home and taught the difference between friend and food. 
> Hey, **your** Bits are safe at least. 

---

## What it looks like

Real screens, straight off the 240×240 display:

<table>
<tr>
<td align="center"><img src="docs/media/screens/carousel.png" width="200" alt="Idle pet with the menu"><br><sub><b>Meet your pet.</b> Eight menus ring the canvas.</sub></td>
<td align="center"><img src="docs/media/screens/vitals.png" width="200" alt="Vitals gauges"><br><sub><b>Keep it alive.</b> Hunger · Frag · Happy.</sub></td>
<td align="center"><img src="docs/media/screens/combat.png" width="200" alt="Combat"><br><sub><b>Pick fights.</b> Auto-battle a wild malbeast.</sub></td>
</tr>
<tr>
<td align="center"><img src="docs/media/screens/lockout.png" width="200" alt="System lockout crisis"><br><sub><b>Crises hit.</b> Feed it before the timer runs out.</sub></td>
<td align="center"><img src="docs/media/screens/evolve.png" width="200" alt="Evolution"><br><sub><b>It grows up.</b> Four life stages.</sub></td>
<td align="center"><img src="docs/media/screens/daemon-bad.png" width="200" alt="A bad-branch daemon"><br><sub><b>...for better or worse.</b> Neglect has a look.</sub></td>
</tr>
</table>

---

## What you'll need

**Hardware**

- A **Waveshare ESP32-S3-LCD-1.54** board (the 1.54-inch, 240×240 screen, 3-button model).
  This is the target the firmware is built for.
- A **USB-C cable** that carries data (not a charge-only cable).
- A **microSD card** — any size. ⚠️ **It will be completely erased during setup**, so use a
  blank one or one you don't mind wiping.
- An **SD card reader** for your computer (many laptops have a slot; otherwise a cheap USB
  reader works).
- *Optional:* a 3.7V LiPo battery to run it untethered. USB power is fine to start.

**Software** (we install these in Step 1)

- **Python 3**, **PlatformIO** (the tool that builds and flashes firmware), and **git**.

Everything below shows the command for **macOS/Linux** first, then the **Windows**
(PowerShell) equivalent. Pick your side and ignore the other.

---

## Getting started

### Step 1 — Install the tools

You need Python 3, then two things installed through it: **PlatformIO** (does the actual
flashing) and **pyserial** (lets the setup script read status back from the device).

First check whether you already have Python:

```bash
python3 --version
```

If that prints a version (3.8 or newer), you're set. If it says "command not found,"
install Python from <https://www.python.org/downloads/> and run it again.

Now install PlatformIO and pyserial:

```bash
# macOS / Linux
python3 -m pip install --upgrade platformio pyserial
```

```powershell
# Windows (PowerShell)
python -m pip install --upgrade platformio pyserial
```

That gives you a `pio` command. Confirm it's there:

```bash
pio --version
```

> **"pio: command not found"?** pip installed it somewhere not on your PATH. The quickest
> fix is to run it as `python3 -m platformio` instead of `pio` — or follow PlatformIO's
> installer guide at <https://docs.platformio.org/en/latest/core/installation/>.

You'll also need **git** to download the project. Check with `git --version`; if it's
missing, grab it from <https://git-scm.com/downloads>.

### Step 2 — Download the project

```bash
git clone https://gitlab.com/AntVena/malwarium.git
cd malwarium
```

Everything from here runs from inside that `malwarium` folder.

### Step 3 — Get your microSD card ready

The device serves its in-game wiki (the "'Pedia") off the SD card, so we copy those files
onto it and format it the way the device expects. **The setup script does all of this for
you** — including wiping and reformatting the card — but first it needs to know *which*
disk your card is, so it doesn't touch the wrong one.

Put the card in your reader, plug the reader into your computer, then list your disks:

```bash
# macOS
diskutil list
```

```powershell
# Windows (PowerShell)
Get-Disk
```

Find your card in the list — **identify it by its size** (e.g. your 32GB card shows as
~32GB). Note its identifier: on macOS that's something like `disk4`; on Windows it's a
number like `2`.

> ⚠️ **Get this right — this step erases the disk you name.** Double-check the size matches
> your card and not your computer's main drive. The script refuses to touch internal or
> system disks as a safety net, but you should still confirm you've got the right one.

### Step 4 — Run the setup script

This is the one command that does everything: it formats and loads your SD card, then
erases and flashes the board with the firmware — checking its own work at each step.

Have the **card in the reader** to start (the script will tell you when to move it into the
device). Then run, replacing `disk4` / `2` with *your* card's identifier from Step 3:

```bash
# macOS / Linux
./tools/provision.sh all --format --device disk4
```

```powershell
# Windows (PowerShell)
./tools/provision.ps1 all -Format -Device 2
```

Here's what it does, and what you'll do, along the way:

1. **Formats your SD card** and copies the 'Pedia wiki onto it. It asks you to type the
   disk identifier once to confirm the wipe — this is your last chance to back out.
2. **Pauses and asks you to move the card into the device.** Eject the card from your
   computer, pop it into the Malwarium's card slot, then connect the **device itself** to
   your computer with the USB-C cable and press Enter.
3. **Erases and flashes the board.** This takes a couple of minutes. You'll see a lot of
   progress text scroll by — that's normal.
4. **Verifies everything worked** — that the firmware landed correctly and that the device
   can read your SD card.

When it finishes you'll see lines like these, which mean success:

```
[save] empty -> seed pet=(egg) gen=1 lvl=0 tag=... blob=.../262144B (0%)
[sd] mounted 30000MB; round-trip OK
>> board provisioned + verified.
```

**And your device screen should now show a wobbling egg.** That's it — you're running your very own shiny new state-of-the-*start*... **Malwarium**. We pray you're more benevolent than your charges. 

> **Prefer to do it in two halves?** You can run the SD and board steps separately:
> `./tools/provision.sh sd --format --device disk4` (card in reader), then move the card
> into the device and run `./tools/provision.sh board`. The `all` command above just
> stitches those together with the "move the card now" prompt in between.

---

## Playing

The device has **three buttons: A, B, C.**

- **A = Next** (move the cursor / cycle) 
- **B = Accept** (choose / confirm) 
- **C = Cancel**
  (back out).
- **A and C together** is the "Exploit" chord — a hacker override used in a few special places. There's a symbol for when it's relevant.

### Hatching your egg 
The egg incubates for a while on its own. Once it's halfway there, a flashing ⚡ symbol appears — press **B** (or the A+C chord) to start a hatch minigame. Your first one is Ransomware, so the game is a short button sequence you repeat to "brute-force the lock." Nail it and the Cryptoshell cracks open into your first creature. In a hurry? 
A **Decryptogram** item in your bag can skip the wait.

### Keeping it alive 
Your creature has three needs: 
- Hunger
- Fragmentation
- and Happiness 

Feed it from the ITEMS menu, run a **Defrag** from MAINT when it gets glitchy, and keep it happy. Slip up too often and it evolves down the "bad" path (stronger in a
fight, but it corrupts faster); look after it well and it grows into a stable, helpful form.
Explore the 'net for battles, loot, and Bits to spend.

## The in-device wiki (The 'Pedia) 
You can toggle allowing your Malwarium to broadcast its own Wi-Fi network. Connect to view your glorious achievements on the big screen:

1. In Config, Enable AP (Access Point) mode. 
2. On your phone or laptop, connect to the Wi-Fi network named **`Malwarium`** (it's open —
   no password).
3. Open a browser and go to **`http://192.168.4.1/pedia`**.
> Or just follow the on-scren QR code the device will show you when you turn AP on. 

You'll get a terminal-styled wiki of every creature, item, and achievement — the ones you
haven't discovered yet show up as locked silhouettes. (There may also be a way for the
curious to peek at locked entries... but that would be telling.)

<details>
<summary><b>📖 Peek at the 'Pedia (click to expand)</b></summary>

<br>

It's a proper little site the device serves to your phone, laptop, wifi-capable TV, whatever. No internet involved. Discovered
creatures show in full colour with lore; ones you've only glimpsed in the wild are glitchy
silhouettes; the rest stay `ENCRYPTED` until you find them.

<p align="center"><img src="docs/media/pedia/landing.png" width="760" alt="'Pedia landing page"></p>
<p align="center"><img src="docs/media/pedia/creature-index.png" width="760" alt="'Pedia creature index with locked and revealed entries"></p>

</details>

---

## Troubleshooting

**The flash fails with "No serial data received" (or the device just won't connect).**
If the device has been sitting on its idle screen, it goes into a deep power-saving sleep
that drops the USB connection. Re-run the setup with the `--wait-wake` flag and **press any
button on the device** when prompted to wake it up:

```bash
./tools/provision.sh board --wait-wake        # macOS/Linux
./tools/provision.ps1 board -WaitWake         # Windows
```

(A truly blank, first-ever flash won't hit this — there's nothing running yet to fall
asleep.)

**The SD script says my card is "not external" and refuses to format it.** That's the
safety net working. It only formats removable cards — make sure you named your *card's*
disk identifier from Step 3, not your computer's drive.

**The device screen says the SD card won't mount.** The card must be formatted **FAT32**
(the newer "exFAT" format that big cards ship with won't work). The `--format` option in
Step 4 handles this for you — re-run it if you skipped it or formatted the card some other
way. On Windows, the built-in formatter only does FAT32 up to 32GB; for a larger card,
format it FAT32 with a free third-party tool first, then run the setup with `-Dest` instead
of `-Format`.

**Windows: "running scripts is disabled on this system."** Windows blocks unsigned scripts
by default. Allow them for your session with this, then re-run the command:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
```

**I just want to re-flash after a code change, not redo the whole card.** Use
`./tools/build_and_flash.sh` (macOS/Linux) for everyday reflashing — the full `provision`
script is really for first-time, blank-device setup. It regenerates the sprite/palette
codegen and the web 'Pedia data before delegating the actual build+upload to the lighter
`./tools/flash.sh` (read `build_and_flash.sh`'s own header comment for the full pipeline
and why it's ordered that way); pass `./tools/flash.sh`'s own flags straight through, e.g.
`./tools/build_and_flash.sh --wait-wake` or `--env waveshare_s3_154_bringup`.

---

## For developers

This README is the *player* on-ramp. If you want to build, extend, or contribute to
Malwarium, the developer entry point is **[`CLAUDE.md`](CLAUDE.md)** — it's the routing
table into the build docs (`docs/`), the per-folder standards, and the engine layout. Start there.

---

## License

Malwarium is free software under the **GNU General Public License, version 3 or later** —
the full text is in [`LICENSE`](LICENSE).

In short: the device in your hands is yours. You may use, study, modify and redistribute
this firmware, and if you hand someone a Malwarium you must let them do the same — which
includes being able to flash their own build onto it. That is the point rather than the
price: a containment habitat you can't open isn't one you own.

The QR encoder in `src/core/render/qrcodegen.c` is Project Nayuki's, used under the MIT
license; its copyright notice stays with the file.

    Copyright (C) 2026 Joshua Fembock

    This program is free software: you can redistribute it and/or modify it under the
    terms of the GNU General Public License as published by the Free Software Foundation,
    either version 3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
    PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along with this
    program. If not, see <https://www.gnu.org/licenses/>.
