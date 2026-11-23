"""Check the runtime shield/audio files without image or audio dependencies."""
from pathlib import Path
import struct
import wave
import zlib

folder = Path(__file__).resolve().parents[1] / 'Scripts' / 'Assets'
for name in ('empty', 'full', 'left', 'right'):
    path = folder / f'shield-{name}.png'
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', path
    offset, pixels = 8, b''
    while offset < len(data):
        length = struct.unpack_from('>I', data, offset)[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        crc = struct.unpack_from('>I', data, offset + 8 + length)[0]
        assert zlib.crc32(kind + payload) & 0xffffffff == crc, path
        if kind == b'IHDR':
            assert struct.unpack('>IIBBBBB', payload) == (40, 48, 8, 6, 0, 0, 0), path
        if kind == b'IDAT':
            pixels += payload
        offset += 12 + length
    assert len(zlib.decompress(pixels)) == 48 * (40 * 4 + 1), path

for name, duration in (('GuardHit', 0.28), ('GuardBreak', 0.90)):
    path = folder / f'{name}.wav'
    with wave.open(str(path), 'rb') as cue:
        assert (cue.getnchannels(), cue.getsampwidth(), cue.getframerate(), cue.getcomptype()) == (2, 2, 48000, 'NONE'), path
        assert abs(cue.getnframes() / cue.getframerate() - duration) < 0.001, path
        samples = struct.unpack('<' + 'h' * (cue.getnframes() * 2), cue.readframes(cue.getnframes()))
        assert 0.1 < max(abs(value) for value in samples) / 32768 <= 0.95, path
print('Guard assets: four valid RGBA shield textures and two bounded PCM16 stereo cues passed.')
