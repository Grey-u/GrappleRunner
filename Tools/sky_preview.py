# =====================================================================================
#  sky_preview.py  -  draws the anime sky OUTSIDE Unreal, to check the look quickly
# =====================================================================================
#  Same maths as the sky part of M_AnimeWorld (Scripts/anime_world.py), written with numpy so
#  it runs anywhere: python Tools/sky_preview.py  ->  writes sky2.png (a 100-degree view).
#  Change the colours or numbers here, look at the picture, then copy the change into
#  anime_world.py. (Used on 14 Nov to match Grey's Genshin sky references.)
#    S(a, b, x) = smoothstep, L(a, b, t) = lerp, fbm = layered smooth noise (soft lumps)
# =====================================================================================
import numpy as np
from PIL import Image
W,H=1200,600
def S(a,b,x):
    t=np.clip((x-a)/(b-a),0,1); return t*t*(3-2*t)
def L(a,b,t): return a+(b-a)*t
fov=np.radians(100); pitch=np.radians(10)
xs=np.linspace(-1,1,W)*np.tan(fov/2); ys=np.linspace(1,-1,H)*np.tan(fov/2)*H/W
X,Y=np.meshgrid(xs,ys)
d=np.stack([np.ones_like(X),X,Y],-1); d/=np.linalg.norm(d,axis=-1,keepdims=True)
dx=d[...,0]*np.cos(pitch)-d[...,2]*np.sin(pitch); up=d[...,0]*np.sin(pitch)+d[...,2]*np.cos(pitch); dy=d[...,1]
def h(ix,iy): return np.modf(np.abs(np.sin(ix*127.1+iy*311.7)*43758.5453))[0]
def fbm(qx,qy,oct=5):
    n=np.zeros_like(qx); amp=0.5
    for o in range(oct):
        ix,iy=np.floor(qx),np.floor(qy); fx,fy=qx-ix,qy-iy; fx=fx*fx*(3-2*fx); fy=fy*fy*(3-2*fy)
        a,b,c,e=h(ix,iy),h(ix+1,iy),h(ix,iy+1),h(ix+1,iy+1)
        n+=amp*L(L(a,b,fx),L(c,e,fx),fy); qx=qx*2.03+11.7; qy=qy*2.03+11.7; amp*=0.5
    return n
Z=np.array([0.12,0.40,0.86]); Hc=np.array([0.80,0.92,0.99])
sky=L(Hc,Z,(np.clip(up*1.3,0,1)**0.55)[...,None])
# ---- clouds above: puffy, crisp, lit from above
k=np.maximum(up+0.12,0.04)
qx=dx/k*0.85; qy=dy/k*0.85
n=fbm(qx,qy); nb=fbm(qx,qy+0.10)       # a little "below" each point (towards the horizon)
m=S(0.535,0.55,n)*S(-0.02,0.10,up)
lit=S(-0.03,0.05,n-nb)
cc=L(np.array([0.60,0.74,0.93]),np.array([1.0,1.0,1.0]),(0.25+0.75*lit)[...,None])
sky=L(sky,cc,(m*0.97)[...,None])
# ---- sea of clouds below the horizon
kd=np.maximum(-up+0.03,0.03)
px=dx/kd*0.35; py=dy/kd*0.35
n2=fbm(px,py); nb2=fbm(px,py-0.12)
m2=S(0.40,0.42,n2)
lit2=S(-0.03,0.06,n2-nb2)
sea=L(np.array([0.50,0.68,0.92]),L(np.array([0.70,0.82,0.96]),np.array([1,1,1]),(0.3+0.7*lit2)[...,None]),m2[...,None])
below=S(0.0,-0.02,up)   # 1 below horizon
sea=L(sea,Hc,(1-S(0.0,0.25,-up))[...,None]*0.75)   # haze near horizon
sky=L(sky,sea,below[...,None])
Image.fromarray((np.clip(sky,0,1)*255).astype(np.uint8)).save("sky2.png")
