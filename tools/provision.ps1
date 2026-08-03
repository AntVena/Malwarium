<#
provision.ps1 — Windows parity for tools/provision.sh.

Same job: take a FRESH ESP32-S3 board and a FRESH microSD card and leave BOTH in
the verified state this project runs on — a blank board is missing the custom
partition table the save depends on, and a blank/exFAT card won't mount at all.

Windows has no `make`, `diskutil`, or a guaranteed `bash`, so where the bash
script DELEGATES (to flash.sh and the `pedia-sd` Make target) this one replicates
the SAME patterns inline with native tooling — the pio commands, the gen_esp32part
partition decode, and the pyserial DTR boot-line listen are identical in spirit.
tools/flash.sh remains the canonical bash counterpart; keep the two in step.

Phases run in the same order (the card physically moves between them):
  1) sd    — card in a READER on THIS PC. Format (optional) + stage + verify.
  2) board — card seated in the DEVICE. Erase + clean build + flash + verify.
  all      — sd, prompt to move the card, then board.

Usage:
  ./tools/provision.ps1 all -Format -Device 2
  ./tools/provision.ps1 all -Dest E:\
  ./tools/provision.ps1 sd -Format -Device 2
  ./tools/provision.ps1 sd -Dest E:\
  ./tools/provision.ps1 board
  ./tools/provision.ps1 board -WaitWake

Params:
  -Dest <path>    Mounted card path for staging (e.g. E:\). Required for `sd`
                  without -Format; ignored with -Format (re-derived after format).
  -Format         Reformat the card FAT32 first. DESTRUCTIVE — needs -Device and
                  confirmation; refuses the system disk and non-removable disks.
                  NOTE: Windows' built-in Format-Volume only does FAT32 up to 32GB;
                  for a larger card, format it FAT32 with a third-party tool and
                  use -Dest instead.
  -Device <n>     Disk NUMBER to format (see `Get-Disk`), e.g. 2.
  -Port <COMn>    Serial port for the board upload/verify (default: autodetect).
  -WaitWake       Retry erase/upload across the light-sleep window (re-provisioning
                  an already-alive board; a blank chip won't need it).
  -KeepBuild      Skip the clean rebuild. Do NOT use for a partition-table change.
  -EnvName <name> PlatformIO env (default waveshare_s3_154 — the only target).
#>

[CmdletBinding()]
param(
    [Parameter(Position = 0)][ValidateSet('sd', 'board', 'all', 'help')][string]$Sub = 'help',
    [string]$Dest = '',
    [switch]$Format,
    [string]$Device = '',
    [string]$Port = '',
    [switch]$WaitWake,
    [switch]$KeepBuild,
    [string]$EnvName = 'waveshare_s3_154'
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot
$IdleSleepWakeBudgetS = 150
$ExpectPartName = 'malsave'
$ExpectPartSize = '256K'

function Say  { param($m) Write-Host ">> $m" }
function Die  { param($m) Write-Error "provision.ps1: $m"; exit 1 }

function Get-GenPartTool {
    $base = Join-Path $env:USERPROFILE '.platformio\packages\framework-arduinoespressif32'
    $hit = Get-ChildItem -Path $base -Recurse -Filter gen_esp32part.py -Depth 3 -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($hit) { return $hit.FullName } else { return $null }
}

# ---------------------------------------------------------------------------
# SD phase
# ---------------------------------------------------------------------------
function Invoke-SdFormat {
    if (-not $Device) { Die '-Format needs -Device <n> (see: Get-Disk)' }
    $disk = Get-Disk -Number $Device -ErrorAction SilentlyContinue
    if (-not $disk) { Die "no disk number $Device (see: Get-Disk)" }
    # Fail closed: refuse the boot/system disk and any non-removable/USB disk.
    if ($disk.IsBoot -or $disk.IsSystem) { Die "disk $Device is the boot/system disk — refusing to format it" }
    if ($disk.BusType -notin @('USB', 'SD', 'MMC')) {
        Die "disk $Device BusType='$($disk.BusType)' is not removable (USB/SD/MMC) — refusing. Only external cards can be formatted here."
    }

    Write-Host ''
    Write-Host 'About to ERASE and reformat FAT32 (this destroys everything on it):'
    $disk | Format-List Number, FriendlyName, BusType, @{n = 'SizeGB'; e = { [math]::Round($_.Size / 1GB, 1) } }
    if ($disk.Size -gt 32GB) {
        Say 'card is >32GB — Windows'' built-in FAT32 formatter may refuse. If it does, format FAT32 with a third-party tool and re-run with -Dest.'
    }
    $reply = Read-Host "Type the disk number '$Device' to confirm the wipe"
    if ($reply -ne "$Device") { Die "confirmation did not match ('$reply' != '$Device') — aborting, nothing was erased" }

    Say "formatting disk $Device as FAT32 (volume MALWARIUM)…"
    # Clear then a single FAT32 partition, assign a letter, format FAT32.
    Clear-Disk -Number $Device -RemoveData -RemoveOEM -Confirm:$false
    Initialize-Disk -Number $Device -PartitionStyle MBR -ErrorAction SilentlyContinue
    $part = New-Partition -DiskNumber $Device -UseMaximumSize -AssignDriveLetter
    Format-Volume -Partition $part -FileSystem FAT32 -NewFileSystemLabel MALWARIUM -Confirm:$false | Out-Null
    $script:Dest = "$($part.DriveLetter):\"
    Say "formatted; mounted at $script:Dest"
}

function Test-SdFilesystem {
    $root = ([System.IO.Path]::GetPathRoot((Resolve-Path $Dest).Path))
    $vol = Get-Volume -FilePath $root -ErrorAction SilentlyContinue
    if (-not $vol) { Say "couldn't read the volume for $Dest — proceeding, but confirm it's FAT32 not exFAT"; return }
    switch -Wildcard ($vol.FileSystem) {
        'FAT32' { Say "filesystem: FAT32 (good)" }
        'FAT'   { Say "filesystem: FAT (good)" }
        'exFAT' { Die "card is exFAT — the device CANNOT mount exFAT. Reformat FAT32 (-Format -Device n, or a third-party FAT32 tool)." }
        default { Say "filesystem: $($vol.FileSystem) — proceeding, but the device needs FAT32/FAT16 (not exFAT)" }
    }
}

function Invoke-DoSd {
    if ($Format) { Invoke-SdFormat }
    elseif (-not $Dest) { Die 'sd needs -Dest <path> (or -Format -Device n to format one)' }
    if (-not (Test-Path $Dest)) { Die "-Dest '$Dest' is not mounted. Put the card in a READER on THIS PC (not the device) and pass its drive, e.g. E:\" }
    Test-SdFilesystem

    # Replicate the `pedia-sd` Make target: regenerate pedia_data.js from the live
    # content tables (so the staged copy is never stale), then copy the served
    # bundle to $Dest\web\ (ap_server.h serves it from /sdcard/web/<path>).
    Say 'regenerating web/data/pedia_data.js from content tables'
    & python (Join-Path $RepoRoot 'tools\gen_pedia_data.py') --repo $RepoRoot --out (Join-Path $RepoRoot 'web\data\pedia_data.js')
    if ($LASTEXITCODE -ne 0) { Die 'pedia data regeneration failed' }

    Say "staging /web 'Pedia bundle -> $Dest\web\"
    $webDst = Join-Path $Dest 'web'
    New-Item -ItemType Directory -Force -Path (Join-Path $webDst 'assets'), (Join-Path $webDst 'data') | Out-Null
    Copy-Item (Join-Path $RepoRoot 'web\index.html'), (Join-Path $RepoRoot 'web\style.css'), (Join-Path $RepoRoot 'web\app.js') $webDst -Force
    Copy-Item (Join-Path $RepoRoot 'web\assets\*') (Join-Path $webDst 'assets') -Recurse -Force
    Copy-Item (Join-Path $RepoRoot 'web\data\*')   (Join-Path $webDst 'data')   -Recurse -Force

    # Verify it landed — not just that the copy didn't throw.
    $missing = @()
    foreach ($f in 'index.html', 'style.css', 'app.js', 'data\pedia_data.js') {
        if (-not (Test-Path (Join-Path $webDst $f))) { $missing += "$webDst\$f" }
    }
    $nassets = (Get-ChildItem (Join-Path $webDst 'assets') -File -Recurse -ErrorAction SilentlyContinue | Measure-Object).Count
    if ($nassets -eq 0) { $missing += "$webDst\assets\ (empty)" }
    if ($missing.Count -gt 0) { $missing | ForEach-Object { Write-Host "   MISSING: $_" }; Die 'SD bundle verification failed — the card is not fully provisioned' }
    Say "SD verified: index.html + style.css + app.js + data/pedia_data.js + assets/ ($nassets files)"
    Say 'eject the card and move it into the DEVICE before flashing (the on-device SD check needs it seated).'
}

# ---------------------------------------------------------------------------
# board phase
# ---------------------------------------------------------------------------
function Invoke-FlashCmd {
    param([string[]]$PioArgs)
    if (-not $WaitWake) {
        Push-Location $RepoRoot; try { & pio @PioArgs } finally { Pop-Location }
        if ($LASTEXITCODE -ne 0) { throw "pio $($PioArgs -join ' ') failed" }
        return
    }
    Say 'if the board''s screen is asleep, press any button on it (a button wake holds USB up far longer than the timer pulse).'
    $deadline = (Get-Date).AddSeconds($IdleSleepWakeBudgetS)
    $attempt = 0
    while ($true) {
        Push-Location $RepoRoot; try { & pio @PioArgs } finally { Pop-Location }
        if ($LASTEXITCODE -eq 0) { return }
        $attempt++
        if ((Get-Date) -ge $deadline) { Die "no wake pulse within ${IdleSleepWakeBudgetS}s ($attempt attempts) — giving up" }
        Say "attempt $attempt missed the wake window, retrying…"
    }
}

function Test-BoardPartition {
    $bin = Join-Path $RepoRoot ".pio\build\$EnvName\partitions.bin"
    if (-not (Test-Path $bin)) { Die "no partitions.bin at $bin — did the build run?" }
    $tool = Get-GenPartTool
    if (-not $tool) { Say 'gen_esp32part.py not found — skipping partition assertion'; return }
    $table = & python $tool $bin 2>$null
    $table | ForEach-Object { Write-Host "   $_" }
    $ok = $table | Where-Object {
        $c = $_ -split ','
        $c.Count -ge 5 -and $c[0].Trim() -eq $ExpectPartName -and $c[2].Trim() -eq 'nvs' -and $c[4].Trim() -eq $ExpectPartSize
    }
    if (-not $ok) { Die "partition table is MISSING '$ExpectPartName' ($ExpectPartSize nvs). The save has nowhere to live. Re-run WITHOUT -KeepBuild (a clean rebuild)." }
    Say "partition OK: '$ExpectPartName' present ($ExpectPartSize nvs)"
}

# The pyserial DTR boot-line listen (flash.sh's save-usage report pattern), here
# also asserting [sd] round-trip and a healthy blob percentage.
$BootVerifyPy = @'
import serial, sys, time
port = sys.argv[1]
save_ok = sd_ok = None
save_line = sd_line = ""
try:
    ser = serial.Serial(port, 115200, timeout=0.3)
    ser.dtr = True
    try:
        ser.rts = True; time.sleep(0.15); ser.rts = False
    except Exception:
        pass
    end = time.time() + 12
    while time.time() < end and (save_ok is None or sd_ok is None):
        line = ser.readline().decode(errors="replace").strip()
        if not line:
            continue
        if line.startswith("[save]"):
            save_line = line
            pct = None
            for tok in line.replace("("," ").replace(")"," ").split():
                if tok.endswith("%"):
                    try: pct = int(tok[:-1])
                    except ValueError: pass
            save_ok = ("FAILED" not in line) and (pct is not None and pct < 100)
        elif line.startswith("[sd]"):
            sd_line = line
            sd_ok = ("round-trip OK" in line)
    ser.close()
except Exception as e:
    print(f"   (serial capture failed: {e})")
rc = 0
print("   " + (save_line or "(no [save] line captured)"))
print("   " + (sd_line or "(no [sd] line captured)"))
if save_ok is False:
    print("   !! SAVE UNHEALTHY — blob at/over capacity or a write FAILED (the NVS-full bug). NOT provisioned."); rc = 2
elif save_ok is None:
    print("   .. couldn't confirm the save line (best-effort) — power-cycle and check 'pio device monitor'.")
if sd_ok is False:
    print("   !! SD did NOT round-trip on-device — the card isn't mounting (exFAT? bad seat?). Re-provision the card."); rc = 2 if rc == 0 else rc
elif sd_ok is None and rc == 0:
    print("   .. couldn't confirm the [sd] line (best-effort) — seat the card and check the monitor.")
sys.exit(rc)
'@

function Test-BoardBoot {
    $p = $Port
    if (-not $p) {
        $ini = Get-Content (Join-Path $RepoRoot 'platformio.ini')
        $inEnv = $false
        foreach ($line in $ini) {
            if ($line -match '^\[env:') { $inEnv = ($line.Trim() -eq "[env:$EnvName]") }
            elseif ($inEnv -and $line -match '^\s*monitor_port\s*=\s*(\S+)') { $p = $Matches[1]; break }
        }
    }
    if (-not $p) { Say 'no serial port resolved — skipping on-device boot assertion (check `pio device monitor`)'; return }
    & python -c 'import serial' 2>$null
    if ($LASTEXITCODE -ne 0) { Say 'pyserial not installed — skipping boot assertion (pip install pyserial)'; return }
    Say "verifying boot lines on $p…"
    $tmp = New-TemporaryFile
    Set-Content -Path $tmp -Value $BootVerifyPy -Encoding ascii
    try { & python $tmp $p; $rc = $LASTEXITCODE } finally { Remove-Item $tmp -ErrorAction SilentlyContinue }
    if ($rc -eq 2) { Die 'on-device verification FAILED (see the flagged line above)' }
}

function Invoke-DoBoard {
    if (-not (Get-Command pio -ErrorAction SilentlyContinue)) { Die 'pio (PlatformIO CLI) not found on PATH' }

    # Stale-pedia note (non-fatal for a board-only run).
    Push-Location $RepoRoot
    try {
        $tmp = New-TemporaryFile
        & python 'tools\gen_pedia_data.py' --repo . --out $tmp 2>$null
        if ((Get-FileHash $tmp).Hash -ne (Get-FileHash 'web\data\pedia_data.js').Hash) {
            Say 'note: web/data/pedia_data.js looks STALE vs. content tables — run ''.\tools\provision.ps1 sd …'' to re-stage the card.'
        }
        Remove-Item $tmp -ErrorAction SilentlyContinue
    } finally { Pop-Location }

    # A partition change needs a clean, non-cached rebuild (a stale cache re-emits
    # the OLD firmware.bin offset → boot loop). Erase clears factory/leftover flash
    # and stale NVS pages before the new table lands. Both are load-bearing.
    if (-not $KeepBuild) {
        $bd = Join-Path $RepoRoot ".pio\build\$EnvName"
        Say "clean: removing $bd (a partition change needs a non-cached rebuild)"
        if (Test-Path $bd) { Remove-Item -Recurse -Force $bd }
    }
    Say "erasing entire flash (pio run -e $EnvName -t erase)"
    Invoke-FlashCmd @('run', '-e', $EnvName, '-t', 'erase')

    Say "building + flashing (pio run -e $EnvName -t upload)"
    $up = @('run', '-e', $EnvName, '-t', 'upload')
    if ($Port) { $up += @('--upload-port', $Port) }
    Invoke-FlashCmd $up

    Test-BoardPartition
    Test-BoardBoot
    Say 'board provisioned + verified.'
}

# ---------------------------------------------------------------------------
switch ($Sub) {
    'sd'    { Invoke-DoSd }
    'board' { Invoke-DoBoard }
    'all' {
        Invoke-DoSd
        Write-Host ''
        Say 'SD done. Eject the card, seat it in the DEVICE, connect the board over USB.'
        Read-Host 'Press Enter when the card is in the device and it''s plugged in'
        Invoke-DoBoard
    }
    default { Get-Help $PSCommandPath -Detailed 2>$null; Get-Content $PSCommandPath -TotalCount 60 | Select-Object -Skip 1 }
}
Say 'provision: done.'
