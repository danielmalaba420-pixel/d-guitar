# suivi de hauteur (autocorrélation) et de niveau d'un rendu : python3 pitch.py fichier.wav [midi]
import numpy as np, wave, sys
w=wave.open(sys.argv[1]); d=np.frombuffer(w.readframes(w.getnframes()),dtype=np.int16).astype(float).reshape(-1,2)/32768
x=d.mean(1); sr=w.getframerate(); ref=float(sys.argv[2]) if len(sys.argv)>2 else None
hop=int(0.05*sr); N=int(0.06*sr)
out=[]
for k in range(0,len(x)-N,hop):
    s=x[k:k+N]; r=np.sqrt((s**2).mean()); db=20*np.log10(r+1e-9)
    if r<1e-4: out.append((k/sr,db,np.nan)); continue
    s=s-s.mean(); c=np.correlate(s,s,'full')[N-1:]
    lo,hi=int(sr/1200),int(sr/60)
    i=lo+np.argmax(c[lo:hi]);
    if 0<i<len(c)-1:
        a,b,cc=c[i-1],c[i],c[i+1]; i=i+0.5*(a-cc)/(a-2*b+cc)
    f=sr/i; out.append((k/sr,db,69+12*np.log2(f/440)))
for t,db,m in out[::int(sys.argv[3]) if len(sys.argv)>3 else 2]:
    print('%5.2fs %6.1f dB  %s'%(t,db,'%.2f'%m if m==m else '-'))
