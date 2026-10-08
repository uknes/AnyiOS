# Requires Windows 10/11 x64 or Windows 11 ARM64.
# Runs actual Win32 input events -> Dynarmic -> owned ARM64 iPhoneOS guest.
# Does not claim the original danqing/2048 app's UIKit/SpriteKit was launched.
param([switch]$OpenAfterTest)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$guest = Join-Path $root "AnyiOS2048Guest"
if (-not (Test-Path $guest)) { throw "Bundled Apple ARM64 guest binary missing: $guest" }
$platform = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
if ($platform -eq "Arm64" -and (Test-Path (Join-Path $root "anyios-arm64-launcher.exe"))) {
    $exe = Join-Path $root "anyios-arm64-launcher.exe"
    $runtime = "ARM64-native launcher -> Windows x64 emulation -> Dynarmic iOS ARM64"
} else {
    $exe = Join-Path $root "anyios-win-2048.exe"
    $runtime = "Windows x86-64 native -> Dynarmic iOS ARM64"
}
if (-not (Test-Path $exe)) { throw "Windows GUI executable missing: $exe" }
$preview = Join-Path $root "local-ui-test.bmp"
if (Test-Path $preview) { Remove-Item -LiteralPath $preview -Force }
Write-Host "Testing: $runtime"
Write-Host "Original guest: $guest"
$p = Start-Process -FilePath $exe -ArgumentList @("--self-test", """$guest""", """$preview""") -PassThru
if (-not $p.WaitForExit(90000)) {
    $p.Kill()
    throw "Guest GUI event test timed out. Inspect antivirus and Windows x64 emulation settings."
}
if ($p.ExitCode -ne 0) {
    throw "Guest GUI self-test returned code $($p.ExitCode). Look for an AnyiOS error dialog."
}
if (-not (Test-Path $preview) -or (Get-Item $preview).Length -lt 100000) {
    throw "No valid Win32 framebuffer image produced."
}
Add-Type -AssemblyName System.Drawing
$image = [System.Drawing.Image]::FromFile($preview)
try {
    if ($image.Width -ne 600 -or $image.Height -ne 760) {
        throw "Unexpected UI dimensions $($image.Width)x$($image.Height)."
    }
    $png = Join-Path $root "local-ui-test.png"
    $image.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $image.Dispose()
}
Write-Host "PASS: ARM64 guest state initialized, keyboard arrows, drag swipe, restart button, GDI framebuffer and PNG."
Write-Host "Preview: $png"
Write-Host "NOTE: the pictured game is the AnyiOS-owned SDK-free compatibility fixture, not the original SpriteKit iOS 2048 application."
if ($OpenAfterTest) {
    Start-Process -FilePath $exe
}
