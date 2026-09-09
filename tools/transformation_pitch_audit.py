"""Wide-search independent output periodicity audit (no target-near search).

FFT autocorrelation implements a normalized difference score over 30..2100 Hz.
Rejects weak periodicity; prefers the shortest near-best period. Like any F0
estimator it can be octave-ambiguous. Retain coverage as well as conditional error.
"""
import argparse
import csv
from pathlib import Path
import subprocess
import tempfile
import numpy as np
from transformation_quality import vowel,write_wav,read_wav,VOWELS,ENGINES


def estimate(audio,rate):
    x=audio[round(.3*rate):round(.85*rate)].astype(float)
    x-=np.mean(x)
    n=len(x); maximum=min(n//2,int(rate/30)); minimum=max(2,int(rate/2100))
    size=1<<(2*n-1).bit_length()
    ac=np.fft.irfft(abs(np.fft.rfft(x,size))**2,size)[:maximum+1]
    energy=np.concatenate(([0.],np.cumsum(x*x)))
    lags=np.arange(maximum+1)
    difference=np.maximum(0,energy[n-lags]+energy[n]-energy[lags]-2*ac)
    score=np.ones(maximum+1)
    score[1:]=difference[1:]*lags[1:]/np.maximum(np.cumsum(difference[1:]),1e-30)
    candidates=np.flatnonzero((score[1:-1]<score[:-2])&(score[1:-1]<=score[2:]))+1
    candidates=candidates[candidates>=minimum]
    if not len(candidates):return 0.,0.
    best=min(score[candidates]); eligible=candidates[score[candidates]<=best+.02]
    p=int(eligible[0])
    if score[p]>.25:return 0.,float(1-score[p])
    a,b,c=score[p-1:p+2]
    fraction=.5*(a-c)/(a-2*b+c)
    return float(rate/(p+np.clip(fraction,-.5,.5))),float(1-b)


def run(args):
    rows=[]
    with tempfile.TemporaryDirectory(prefix="vocalpilot-pitch-audit-") as temp:
        root=Path(temp);case=0
        for rate in (44100,48000,96000):
            for f0 in (90,220,660):
                for name in VOWELS:
                    for cents in (-1200,-700,-100,25,100,700,1200):
                        source=root/"source.wav";write_wav(source,rate,vowel(rate,f0,VOWELS[name]))
                        output=root/f"render-{case}";case+=1
                        subprocess.run([args.renderer,str(source),str(output),"--f0",str(f0),"--cents",str(cents)],check=True,capture_output=True)
                        for engine in ENGINES:
                            _,y=read_wav(output/f"{engine}.wav");hz,confidence=estimate(y[:,0],rate)
                            rows.append(dict(rate=rate,f0=f0,vowel=name,cents=cents,engine=engine,estimated_hz=hz,confidence=confidence,
                                             cents_error=1200*np.log2(hz/(f0*2**(cents/1200))) if hz else None))
                        import shutil
                        shutil.rmtree(output)
    with Path(args.output).open("w",newline="") as out:
        writer=csv.DictWriter(out,fieldnames=rows[0].keys());writer.writeheader();writer.writerows(rows)
    for engine in ENGINES:
        chosen=[r for r in rows if r["engine"]==engine];valid=[abs(r["cents_error"]) for r in chosen if r["cents_error"] is not None]
        print(engine,"valid",len(valid),"/",len(chosen),"median/p95/max cents",np.percentile(valid,[50,95,100]) if valid else "N/A",flush=True)


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("renderer");parser.add_argument("output")
    run(parser.parse_args())
