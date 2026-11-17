import numpy as np, sys, wave
def load(p):
    w=wave.open(p); n=w.getnframes(); d=np.frombuffer(w.readframes(n),dtype=np.int16).reshape(-1,2)/32768.0; return d[:,0],d[:,1],w.getframerate()
def f0(x,sr,t0,t1,lo=60,hi=1500):
    seg=x[int(t0*sr):int(t1*sr)]; seg=seg*np.hanning(len(seg)); N=1<<int(np.ceil(np.log2(len(seg)*8)))
    S=np.abs(np.fft.rfft(seg,N)); fr=np.fft.rfftfreq(N,1/sr); m=(fr>lo)&(fr<hi); i=np.argmax(S[m]); 
    # parabolic
    k=np.where(m)[0][i]; a,b,c=np.log(S[k-1]+1e-12),np.log(S[k]+1e-12),np.log(S[k+1]+1e-12); d=0.5*(a-c)/(a-2*b+c); return fr[k]+d*(fr[1]-fr[0])
if __name__=="__main__":
    L,R,sr=load(sys.argv[1]); print(f0(L,sr,float(sys.argv[2]),float(sys.argv[3])))

def pitch_ac(x,sr,t0,t1,lo=40,hi=1800):
    seg=x[int(t0*sr):int(t1*sr)]; seg=seg-seg.mean()
    if np.max(np.abs(seg))<1e-6: return float('nan')
    N=1<<int(np.ceil(np.log2(len(seg)*2))); S=np.fft.rfft(seg,N); ac=np.fft.irfft(np.abs(S)**2)[:len(seg)]
    lmin=int(sr/hi); lmax=int(sr/lo); ac=ac/ac[0]
    seg2=ac[lmin:lmax]; mx=seg2.max()
    # premier maximum local dépassant 90 % du maximum
    for i in range(1,len(seg2)-1):
        if seg2[i]>=0.9*mx and seg2[i]>=seg2[i-1] and seg2[i]>=seg2[i+1]:
            a,b,c=seg2[i-1],seg2[i],seg2[i+1]; d=0.5*(a-c)/(a-2*b+c); return sr/(lmin+i+d)
    return float('nan')
