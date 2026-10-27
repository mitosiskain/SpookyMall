"""Record exact redistributed dependency bytes after prepare_whisper.ps1."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[2]
runtime = root / 'ThirdParty/Whisper'
model = root / 'ThirdParty/Whisper/Models/ggml-small.en.bin'
files = sorted(p for p in runtime.rglob('*') if p.is_file())


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            value.update(block)
    return value.hexdigest()


result = {
    'library': 'whisper.cpp 1.9.5',
    'revision': 'd1be6fde11ac6e0407606b4e42fe72d34add8037',
    'binary_release': 'b5454 (same commit as v1.9.5)',
    'archive_sha256': '6ba69e3482d7826214f90a6a9c84ca07782aec1e1d0c6a7c30c994fd5d816ccb',
    'model': 'Whisper small.en, original F16 GGML conversion',
    'model_sha1': 'db8a495a91d927739e50b3fc1cc4c6b8f6c2d022',
    'license': {'code': 'MIT', 'model': 'MIT'},
    'sources': [
        'https://github.com/ggml-org/whisper.cpp/releases/tag/v1.9.5',
        'https://github.com/ggml-org/whisper.cpp/releases/tag/b5454',
        'https://github.com/ggml-org/whisper.cpp/blob/v1.9.5/models/README.md',
        'https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin',
        'https://github.com/openai/whisper/blob/v20250625/LICENSE',
    ],
    'files': [dict(path=p.relative_to(root).as_posix(), bytes=p.stat().st_size,
                   sha256=digest(p)) for p in files]
}
destination = root / 'ExternalAssets/Voice/whisper_provenance.json'
destination.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(f'Recorded {len(files)} redistributed files.')
