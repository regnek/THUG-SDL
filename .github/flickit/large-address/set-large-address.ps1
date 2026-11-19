# Sets or clears the large-address flag on THUG-SDL.exe. With the flag, a 32-bit program gets 4 GB of
# address space on a 64-bit Windows instead of 2 GB. It is one bit in the exe's header; nothing else in
# the file changes. The first time it changes the file it keeps a copy as THUG-SDL.exe.before-large-address.
#
#   set-large-address.ps1          set the flag
#   set-large-address.ps1 -Off     clear it
#   -Exe <path>                    another exe than THUG-SDL.exe next to this script
param(
    [string]$Exe = (Join-Path $PSScriptRoot 'THUG-SDL.exe'),
    [switch]$Off
)
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Exe)) {
    Write-Host "There is no $Exe. Put this script next to THUG-SDL.exe in the game folder."
    exit 1
}
$path = (Resolve-Path -LiteralPath $Exe).Path
$bytes = [System.IO.File]::ReadAllBytes($path)
if ($bytes.Length -lt 0x40) { Write-Host "$path is not a Windows program."; exit 1 }
$pe = [BitConverter]::ToInt32($bytes, 0x3C)
if ($pe -lt 0x40 -or $pe + 24 -gt $bytes.Length -or
    $bytes[$pe] -ne 0x50 -or $bytes[$pe + 1] -ne 0x45 -or $bytes[$pe + 2] -ne 0 -or $bytes[$pe + 3] -ne 0) {
    Write-Host "$path is not a Windows program."
    exit 1
}
if ([BitConverter]::ToUInt16($bytes, $pe + 4) -ne 0x14C) {
    Write-Host "$path is not a 32-bit program."
    exit 1
}

$at = $pe + 22                                   # the file header's Characteristics
$flags = [int][BitConverter]::ToUInt16($bytes, $at)
if ($Off) { $new = $flags -band 0xFFDF } else { $new = $flags -bor 0x0020 }
$state = if ($Off) { 'does not have' } else { 'has' }
if ($new -eq $flags) {
    Write-Host "THUG-SDL.exe already $state the large-address flag. Nothing changed."
    exit 0
}

$backup = "$path.before-large-address"
if (-not (Test-Path -LiteralPath $backup)) {
    Copy-Item -LiteralPath $path -Destination $backup
    Write-Host "Kept a copy of the exe as $(Split-Path -Leaf $backup)."
}
$bytes[$at] = [byte]($new -band 0xFF)
$bytes[$at + 1] = [byte](($new -shr 8) -band 0xFF)
try {
    [System.IO.File]::WriteAllBytes($path, $bytes)
} catch {
    Write-Host "Could not write $path. Close the game first. ($($_.Exception.Message))"
    exit 1
}
if ($Off) {
    Write-Host "THUG-SDL.exe no longer has the large-address flag: 2 GB of address space, as before."
} else {
    Write-Host "THUG-SDL.exe now has the large-address flag: 4 GB of address space on a 64-bit Windows."
}
