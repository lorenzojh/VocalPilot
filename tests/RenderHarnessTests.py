"""End-to-end renderer validation, including explicit sample trajectories."""
import csv
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import wave

renderer = str(Path(sys.argv[1]).resolve())
tools = Path(__file__).resolve().parents[1]/"tools"

with tempfile.TemporaryDirectory(prefix="vocalpilot-render-test-") as temp:
    root=Path(temp)
    for rate in (44100,48000,96000):
        source=root/f"source-{rate}.wav"
        with wave.open(str(source),"wb") as out:
            out.setnchannels(2); out.setsampwidth(2); out.setframerate(rate)
            samples=[round(5000*math.sin(2*math.pi*220*i/rate)) for i in range(rate//3)]
            out.writeframes(b"".join(struct.pack("<hh",s,-s) for s in samples))
        destination=root/f"unity-{rate}"
        subprocess.run([renderer,str(source),str(destination),"--f0","220","--cents","0"],check=True,capture_output=True)
        with wave.open(str(destination/"dry.wav"),"rb") as wav: dry=wav.readframes(wav.getnframes())
        for engine in ("legacy","synchronous","spectral"):
            with wave.open(str(destination/f"{engine}.wav"),"rb") as wav:
                assert wav.getnframes()==len(samples) and wav.getframerate()==rate and wav.getnchannels()==2
                assert wav.readframes(wav.getnframes())==dry
        trajectory=destination/"controls.csv"
        replay=root/f"replay-{rate}"
        subprocess.run([renderer,str(source),str(replay),"--trajectory",str(trajectory)],check=True,capture_output=True)
        assert (replay/"synchronous.wav").read_bytes()==(destination/"synchronous.wav").read_bytes()
        for extra in (["--f0","nan"],["--f0","220","--cents","1201"],["--key","1.5"],["--strength","101"],["--cents","0"],["--trajectory",str(trajectory),"--f0","220"]):
            assert subprocess.run([renderer,str(source),str(root/"invalid"),*extra],capture_output=True).returncode!=0
        assert subprocess.run([renderer,str(source),str(destination)],capture_output=True).returncode!=0
    pack=root/"listen"
    subprocess.run([sys.executable,str(tools/"blind_listening.py"),"prepare",str(destination),str(pack),"--seed","42"],check=True,capture_output=True)
    assert set(p.name for p in pack.glob("*.wav"))=={"A.wav","B.wav","C.wav","D.wav","reference-dry.wav"}
    assert subprocess.run([sys.executable,str(tools/"blind_listening.py"),"reveal",str(pack),str(root/"listen-reveal.json")],capture_output=True).returncode!=0
    assert json.loads((root/"listen-reveal.json").read_text()).keys()==set("ABCD")
print("Renderer: three rates, stereo, unity, trajectory replay, invalid options and blind-pack guard passed")
