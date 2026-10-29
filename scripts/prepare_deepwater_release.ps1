param([string]$Version = "0.8.1")

$ErrorActionPreference = "Stop"
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$Config = [IO.File]::ReadAllText((Join-Path $Root "platformio.ini"))
if (-not $Config.Contains('POSEIDON_VERSION=\"' + $Version + '\"')) {
    throw "Requested release version does not match platformio.ini."
}

$Artifacts = @(
    @{ Source = ".pio\build\cardputer\firmware.factory.bin"; Destination = "release_binaries\poseidon-factory.bin" },
    @{ Source = ".pio\build\cardputer\firmware.factory.bin"; Destination = "release_binaries\poseidon_adv_factory.bin" },
    @{ Source = ".pio\build\cardputer\firmware.bin"; Destination = "release_binaries\poseidon_adv.bin" },
    @{ Source = ".pio\build\cardputer-launcher\firmware.bin"; Destination = "release_binaries\poseidon-launcher.bin" },
    @{ Source = ".pio\build\cardputer-launcher-dual\firmware.bin"; Destination = "release_binaries\poseidon-launcher-dual.bin" }
)

foreach ($Artifact in $Artifacts) {
    $Source = Join-Path $Root $Artifact.Source
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Missing $Source; build all three Cardputer profiles before packaging."
    }
    $Bytes = [IO.File]::ReadAllBytes($Source)
    if ([Text.Encoding]::ASCII.GetString($Bytes).IndexOf($Version, [StringComparison]::Ordinal) -lt 0) {
        throw "$Source does not contain the requested version; rebuild before packaging."
    }
}
$Dual = Get-Item -LiteralPath (Join-Path $Root ".pio\build\cardputer-launcher-dual\firmware.bin")
if ($Dual.Length -gt 0x2D0000) { throw "Dual-app image exceeds the verified POSEIDON slot." }

foreach ($Artifact in $Artifacts) {
    $Source = Join-Path $Root $Artifact.Source
    $Destination = Join-Path $Root $Artifact.Destination
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
    if ((Get-FileHash -LiteralPath $Source).Hash -ne (Get-FileHash -LiteralPath $Destination).Hash) {
        throw "Packaged firmware differs from $Source."
    }
}

$Lines = foreach ($Artifact in $Artifacts | Where-Object { $_.Destination.StartsWith("release_binaries\") }) {
    $Destination = Join-Path $Root $Artifact.Destination
    "{0}  {1}" -f (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash.ToLowerInvariant(),
                  [IO.Path]::GetFileName($Destination)
}
[IO.File]::WriteAllText((Join-Path $Root "release_binaries\SHA256SUMS.txt"),
                       (($Lines -join "`n") + "`n"), [Text.Encoding]::ASCII)
Write-Host "Packaged POSEIDON Advanced $Version for standalone, Launcher and the verified dual-app layout."
