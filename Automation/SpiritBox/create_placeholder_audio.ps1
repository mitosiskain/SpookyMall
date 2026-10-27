$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Speech
$spiritRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../SourceAssets/SpiritBox'))
New-Item -ItemType Directory -Path $spiritRoot -Force | Out-Null
$spiritVoice = New-Object System.Speech.Synthesis.SpeechSynthesizer
try {
    $spiritFormat = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(48000, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)
    $spiritVoice.Rate = -3
    $spiritVoice.Volume = 90
    $spiritVoice.SetOutputToWaveFile((Join-Path $spiritRoot 'SW_SpiritBox_Placeholder.wav'), $spiritFormat)
    $spiritVoice.Speak('I am here.')
    $spiritVoice.SetOutputToNull()
    Write-Output ('Created placeholder with installed voice: ' + $spiritVoice.Voice.Name)
} finally {
    $spiritVoice.Dispose()
}
