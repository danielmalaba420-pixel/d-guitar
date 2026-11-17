import subprocess, numpy as np, sys, os
sys.path.insert(0,'tests')
from analyze import *
env=dict(os.environ,TAP='1')
for gi in range(3):
    subprocess.run(['./tests/render',str(gi),'0','notes','/tmp/out/p.wav','9'],env=env,capture_output=True)
    L,R,sr=load('/tmp/out/p.wav')
    notes=[40,45,52,57,64,69,76]; errs=[]; pk=[]
    for i,n in enumerate(notes):
        t0=0.1+i*1.2+0.3; t1=t0+0.1
        pk.append(np.max(np.abs(L[int(t0*sr):int(t1*sr)])))
        f=pitch_ac(L,sr,t0,t1); tgt=440*2**((n-69)/12); errs.append(1200*np.log2(f/tgt))
    print(gi,'cents:',' '.join(f'{e:+.1f}' for e in errs),'| peaks',' '.join(f'{p:.3f}' for p in pk))
