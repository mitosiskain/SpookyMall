$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Speech
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'Fixtures/Whisper'))
New-Item -ItemType Directory -Force -Path $taskRoot | Out-Null
$taskVoice = New-Object System.Speech.Synthesis.SpeechSynthesizer
$taskRecords = @()
try {
    $taskFormat = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(16000, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)
    $taskInstalled = @($taskVoice.GetInstalledVoices() | Where-Object { $_.Enabled -and $_.VoiceInfo.Culture.Name -like 'en-*' })
    if ($taskInstalled.Count -eq 0) { throw 'No installed English synthetic voice.' }
    $taskPhrases = [ordered]@{
        'where' = 'Where are you?'
        'who' = 'Who are you?'
        'age' = 'How old are you?'
        'book' = 'Write in the book.'
        'negated' = 'Do not write in the book.'
        'unrelated' = 'The weather is better than yesterday.'
        'similar_noise_words' = 'Better. Then, then, there.'
    }
    foreach ($taskIndex in 0..([Math]::Min(2, $taskInstalled.Count) - 1)) {
        $taskVoice.SelectVoice($taskInstalled[$taskIndex].VoiceInfo.Name)
        $taskVoice.Rate = $taskIndex - 1
        foreach ($taskPhrase in $taskPhrases.GetEnumerator()) {
            $taskFile = $taskPhrase.Key + '_' + $taskIndex + '.wav'
            $taskVoice.SetOutputToWaveFile((Join-Path $taskRoot $taskFile), $taskFormat)
            $taskVoice.Speak($taskPhrase.Value)
            $taskVoice.SetOutputToNull()
            $taskRecords += [ordered]@{file=$taskFile; text=$taskPhrase.Value; voice=$taskVoice.Voice.Name; rate=$taskVoice.Rate}
        }
    }
} finally { $taskVoice.Dispose() }
$taskRecords | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskRoot 'synthetic_sources.json') -Encoding utf8
Write-Output ('Generated ' + $taskRecords.Count + ' synthetic utterances. These do not establish real microphone accuracy.')
