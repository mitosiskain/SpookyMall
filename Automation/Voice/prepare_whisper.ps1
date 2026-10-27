$ErrorActionPreference = 'Stop'
$taskProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskExtraction = Join-Path $taskProject 'ExternalAssets/Voice/Extracted/Whisper-v1.9.5'
$taskRuntime = Join-Path $taskProject 'ThirdParty/Whisper/Win64'
$taskHeaders = Join-Path $taskProject 'ThirdParty/Whisper/include'
$taskModels = Join-Path $taskProject 'ThirdParty/Whisper/Models'
New-Item -ItemType Directory -Force -Path $taskExtraction,$taskRuntime,$taskHeaders,$taskModels | Out-Null

function Get-VerifiedDownload($Uri, $Destination, $Algorithm, $ExpectedHash) {
    if (-not (Test-Path -LiteralPath $Destination)) {
        Invoke-WebRequest -Uri $Uri -OutFile $Destination
    }
    if ((Get-FileHash -LiteralPath $Destination -Algorithm $Algorithm).Hash -ne $ExpectedHash) {
        throw "Hash mismatch: $Destination"
    }
}

$taskArchive = Join-Path $taskExtraction 'whisper-bin-x64.zip'
Get-VerifiedDownload 'https://github.com/ggml-org/whisper.cpp/releases/download/b5454/whisper-bin-x64.zip' $taskArchive 'SHA256' '6ba69e3482d7826214f90a6a9c84ca07782aec1e1d0c6a7c30c994fd5d816ccb'
Expand-Archive -LiteralPath $taskArchive -DestinationPath (Join-Path $taskExtraction 'binary') -Force
Get-ChildItem -LiteralPath (Join-Path $taskExtraction 'binary/Release') -File |
    Where-Object { $_.Name -eq 'whisper.dll' -or $_.Name -eq 'ggml.dll' -or $_.Name -eq 'ggml-base.dll' -or $_.Name -like 'ggml-cpu-*.dll' } |
    Copy-Item -Destination $taskRuntime

$taskRevision = 'd1be6fde11ac6e0407606b4e42fe72d34add8037'
$taskSource = 'https://raw.githubusercontent.com/ggml-org/whisper.cpp/' + $taskRevision + '/'
foreach ($taskRelative in @('include/whisper.h','ggml/include/ggml.h','ggml/include/ggml-cpu.h','ggml/include/ggml-alloc.h','ggml/include/ggml-backend.h')) {
    Invoke-WebRequest -Uri ($taskSource + $taskRelative) -OutFile (Join-Path $taskHeaders ([IO.Path]::GetFileName($taskRelative)))
}
Invoke-WebRequest -Uri ($taskSource + 'LICENSE') -OutFile (Join-Path $taskProject 'ThirdParty/Whisper/whisper.cpp-LICENSE.txt')
Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/openai/whisper/v20250625/LICENSE' -OutFile (Join-Path $taskProject 'ThirdParty/Whisper/Whisper-LICENSE.txt')
Get-VerifiedDownload 'https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin' (Join-Path $taskModels 'ggml-small.en.bin') 'SHA1' 'db8a495a91d927739e50b3fc1cc4c6b8f6c2d022'
Get-ChildItem -LiteralPath (Join-Path $taskExtraction 'binary') -Recurse -File | Select-Object FullName,Length
