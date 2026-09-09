"""Dynamic, transient and Nyquist probes for the C++ comparison renderer.

Oracle trajectories isolate transformation defects; a separate causal render
uses M2 tracking. Metrics are proxies, never human listening scores.
"""
import argparse
import csv
import json
from pathlib import Path
import subprocess
import tempfile
import numpy as np
from transformation_quality import write_wav, read_wav, ENGINES


def control_file(path, f0, cents, voiced):
    with path.open("w",newline="") as out:
        writer=csv.writer(out); writer.writerow(["sample","sourceHz","ratio","voiced","confidence","amount"])
        for i in range(len(f0)):
            writer.writerow([i,f0[i],2**(cents[i]/1200),int(voiced[i]),1 if voiced[i] else 0,1 if voiced[i] else 0])


def run(args):
    destination=Path(args.output); destination.mkdir(parents=True,exist_ok=True)
    rows=[]
    with tempfile.TemporaryDirectory(prefix="vocalpilot-events-") as temp:
        root=Path(temp)
        case=0
        for rate in (44100,48000,96000):
            t=np.arange(rate*2)/rate
            for name in ("zero_crossing","vibrato","slide","hard_jump","consonants", "fast_slide", "breathy", "attacks"):
                f0=np.full(len(t),220.)
                cents=100*np.sin(2*np.pi*3*t)
                voiced=np.ones(len(t),bool)
                if name=="vibrato":
                    f0*=2**(40*np.sin(2*np.pi*5*t)/1200); cents=1200*np.log2(220/f0)
                elif name in ("slide","fast_slide"):
                    f0=130*2**(np.clip((t-.3)*(1 if name=="slide" else 4),0,1)); cents=1200*np.log2(220/f0)
                elif name=="hard_jump":
                    cents=np.where(t<.5,25,np.where(t<1.,700,-700))
                phase=2*np.pi*np.cumsum(f0)/rate
                audio=.1*np.sin(phase)+.04*np.sin(2*phase)+.015*np.sin(3*phase)
                if name=="breathy":
                    audio+=np.random.default_rng(13).normal(0,.012,len(t))
                if name=="attacks":
                    gate=((t>=.4)&(t<.8))|((t>=1.)&(t<1.4))|(t>=1.6)
                    audio*=gate
                    voiced=gate
                    cents[:]=100
                if name=="consonants":
                    voiced=(t<.5)|((t>=.8)&(t<1.2))|(t>=1.5)
                    noise=np.random.default_rng(77).normal(0,.018,len(t))
                    noise=np.concatenate(([0],np.diff(noise)))
                    audio=np.where(voiced,audio,noise)
                    for onset in (.6,1.3):
                        j=round(onset*rate); audio[j:j+4]=(.2,-.15,.1,-.05)
                    cents[:]=100
                fade=np.minimum(t/.02,1)*np.minimum((2-t)/.02,1)
                audio*=fade
                source=root/f"source-{case}.wav"; trajectory=root/f"control-{case}.csv"
                write_wav(source,rate,audio); control_file(trajectory,f0,cents,voiced)
                output=root/f"render-{case}"; case+=1
                subprocess.run([args.renderer,str(source),str(output),"--trajectory",str(trajectory)],check=True,stdout=subprocess.DEVNULL)
                _,dry=read_wav(output/"dry.wav"); dry=dry[:,0]
                for engine in ENGINES:
                    _,y=read_wav(output/f"{engine}.wav"); y=y[:,0]
                    assert len(y)==len(dry) and np.isfinite(y).all()
                    quiet=(~voiced)&(t>.55)&(t<.79)|( (~voiced)&(t>1.25)&(t<1.49))
                    row=dict(rate=rate,event=name,engine=engine,peak=float(np.max(abs(y))),
                             max_step=float(np.max(abs(np.diff(y)))),input_max_step=float(np.max(abs(np.diff(dry)))),
                             rms_gain_db=float(10*np.log10(np.mean(y*y)/np.mean(dry*dry))),
                             unvoiced_rmse=float(np.sqrt(np.mean((y[quiet]-dry[quiet])**2))) if quiet.any() else None)
                    if name=="consonants":
                        j=round(.6*rate); region=slice(j-round(.003*rate),j+round(.003*rate))
                        row["plosive_peak_offset_ms"]=float((np.argmax(abs(y[region]))-np.argmax(abs(dry[region])))*1000/rate)
                    else: row["plosive_peak_offset_ms"]=None
                    # Independent 40-ms phase-increment fit around expected F0.
                    errors=[]
                    for end in np.arange(.3,1.9,.02):
                        a=round((end-.04)*rate); b=round(end*rate)
                        if not voiced[a:b].all():
                            continue
                        target=np.mean(f0[a:b]*2**(cents[a:b]/1200))
                        frequencies=np.linspace(target*.85,target*1.15,181)
                        w=np.hanning(b-a)*y[a:b]
                        magnitudes=np.abs(np.exp(-2j*np.pi*frequencies[:,None]*np.arange(b-a)/rate)@w)
                        estimate=frequencies[np.argmax(magnitudes)]
                        errors.append(abs(1200*np.log2(estimate/target)))
                    row["dynamic_pitch_p95_cents"]=float(np.percentile(errors,95))
                    rows.append(row)
            # High-frequency single-partial probes. Out-of-band transposed
            # partials should not create strong components at their fold image.
            for fraction in (.34,.43):
                f0=rate/256; high=round(fraction*256)*f0
                audio=.1*np.sin(2*np.pi*high*t)
                source=root/f"alias-{case}.wav"; write_wav(source,rate,audio)
                output=root/f"render-{case}"; case+=1
                subprocess.run([args.renderer,str(source),str(output),"--f0",str(f0),"--cents","1200"],check=True,stdout=subprocess.DEVNULL)
                fold=abs(rate-high*2)
                for engine in ENGINES:
                    _,y=read_wav(output/f"{engine}.wav"); y=y[rate//2:rate*3//2,0]
                    mag=abs(np.fft.rfft(y*np.hanning(len(y))))**2
                    frequencies=np.fft.rfftfreq(len(y),1/rate)
                    folded=np.sum(mag[abs(frequencies-fold)<10])
                    source_power=(.1*len(y)/4)**2*1.5
                    rows.append(dict(rate=rate,event=f"nyquist_probe_{fraction}",engine=engine,fold_image_db=float(10*np.log10(max(folded/source_power,1e-15)))))
        # Retune curves are explicit offline trajectories, not a new plugin
        # parameter. 0 ms steps immediately; positive values are time constants.
        rate=48000; t=np.arange(rate)/rate
        for ms in (0,5,10,20,35,50,100):
            desired=np.where(t<.3,0.,100.)
            if ms:
                desired=np.where(t<.3,0.,100*(1-np.exp(-np.maximum(0,t-.3)/(ms/1000))))
            source=root/f"retune-{ms}.wav"; trajectory=root/f"retune-{ms}.csv"
            write_wav(source,rate,.1*np.sin(2*np.pi*220*t)); control_file(trajectory,np.full(len(t),220.),desired,np.ones(len(t),bool))
            output=root/f"retune-render-{ms}"
            subprocess.run([args.renderer,str(source),str(output),"--trajectory",str(trajectory)],check=True,stdout=subprocess.DEVNULL)
            for engine in ENGINES:
                _,y=read_wav(output/f"{engine}.wav"); y=y[:,0]
                assert np.isfinite(y).all()
                rows.append(dict(rate=rate,event=f"retune_{ms}_ms",engine=engine,peak=float(max(abs(y))),max_step=float(max(abs(np.diff(y))))))
    fields=sorted(set().union(*(row.keys() for row in rows)))
    with (destination/"transformation-events.csv").open("w",newline="") as out:
        writer=csv.DictWriter(out,fieldnames=fields); writer.writeheader(); writer.writerows(rows)
    print(f"Measured {len(rows)} event/engine configurations",flush=True)


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument("renderer"); parser.add_argument("output")
    run(parser.parse_args())
