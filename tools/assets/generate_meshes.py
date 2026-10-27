#!/usr/bin/env python3
"""Deterministic original CC0-independent low-poly mesh compiler for PSP.

Generates indexed 1/64 m quantized positions + 8-byte triangle descriptors.
No external Python dependencies; all authored geometry is original. 3 LODs.
"""
from __future__ import annotations
import math
from pathlib import Path

class Mesh:
    def __init__(self):
        self.vertices=[]; self.indices={}; self.triangles=[]
    def tri(self,a,b,c,material,emissive=False):
        ids=[]
        for point in (a,b,c):
            p=tuple(int(round(float(v)*64)) for v in point)
            if any(abs(v)>32767 for v in p): raise ValueError('mesh out of int16 range')
            if p not in self.indices:
                self.indices[p]=len(self.vertices);self.vertices.append(p)
            ids.append(self.indices[p])
        if len(set(ids))==3:
            self.triangles.append((*ids,material,int(emissive)))
    def quad(self,a,b,c,d,material,emissive=False):
        self.tri(a,b,c,material,emissive);self.tri(a,c,d,material,emissive)
    def box(self,x,y,z,w,h,d,front=0,side=None,roof=None,back=None):
        if side is None:side=front
        if roof is None:roof=front
        if back is None:back=side
        x0,x1=x-w/2,x+w/2;y0,y1=y,y+h;z0,z1=z-d/2,z+d/2
        self.quad((x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1),front)
        self.quad((x1,y0,z0),(x0,y0,z0),(x0,y1,z0),(x1,y1,z0),back)
        self.quad((x0,y0,z0),(x0,y0,z1),(x0,y1,z1),(x0,y1,z0),side)
        self.quad((x1,y0,z1),(x1,y0,z0),(x1,y1,z0),(x1,y1,z1),side)
        self.quad((x0,y1,z1),(x1,y1,z1),(x1,y1,z0),(x0,y1,z0),roof)
    def car_shell(self):
        # Chamfered hood, boot and roofline, non-cubic silhouette.
        self.quad((-0.94,.45,-1.7),(.94,.45,-1.7),(.99,1.04,-1.18),(-.99,1.04,-1.18),16)
        self.quad((-1,.48,-2.16),(1,.48,-2.16),(.94,.82,-1.7),(-.94,.82,-1.7),17)
        self.quad((-1,.47,2.05),(1,.47,2.05),(.94,.90,1.50),(-.94,.90,1.50),16)
        self.box(0,.32,0,1.94,.63,3.65,16,17,16)
        self.quad((-.86,.97,-.98),(.86,.97,-.98),(.73,1.52,-.54),(-.73,1.52,-.54),13)
        self.quad((-.73,1.52,-.54),(.73,1.52,-.54),(.77,1.52,.64),(-.77,1.52,.64),17)
        self.quad((-.77,1.52,.64),(.77,1.52,.64),(.90,.95,1.43),(-.90,.95,1.43),13)
        for side in (-1,1):
            self.quad((side*.87,1.00,-.93),(side*.73,1.52,-.52),(side*.75,1.52,.60),(side*.88,1.0,1.31),13)
        self.box(0,.50,2.07,1.14,.19,.12,22,22,22)
        for side in (-1,1):
            self.box(side*.71,.71,2.11,.46,.20,.09,14,14,14)
            self.box(side*.72,.71,-2.18,.40,.20,.09,15,15,15)
            self.box(side*.96,1.09,.32,.16,.14,.24,7,7,7)
    def wheel(self,x,y,z,r=.39,width=.18,segments=8):
        # Rotation around X axis; separate sidewall and metallic hub.
        for sign in (-1,1):
            xx=x+sign*width/2
            for i in range(segments):
                a=i*2*math.pi/segments;b=(i+1)*2*math.pi/segments
                p=(xx,y+r*math.sin(a),z+r*math.cos(a))
                q=(xx,y+r*math.sin(b),z+r*math.cos(b))
                self.tri((xx,y,z),p,q,11)
                self.tri((xx+sign*.012,y,z),
                         (xx+sign*.012,y+r*.47*math.sin(a),z+r*.47*math.cos(a)),
                         (xx+sign*.012,y+r*.47*math.sin(b),z+r*.47*math.cos(b)),12)
        for i in range(segments):
            a=i*2*math.pi/segments;b=(i+1)*2*math.pi/segments
            self.quad((x-width/2,y+r*math.sin(a),z+r*math.cos(a)),
                      (x+width/2,y+r*math.sin(a),z+r*math.cos(a)),
                      (x+width/2,y+r*math.sin(b),z+r*math.cos(b)),
                      (x-width/2,y+r*math.sin(b),z+r*math.cos(b)),11)
    def car(self,lod):
        if lod==2:
            self.box(0,.34,0,1.95,.74,4.15,16,17,16)
            self.box(0,1.05,0,1.63,.48,2.20,13,13,13)
        elif lod==1:
            self.car_shell()
            for x in (-.94,.94):
                for z in (-1.37,1.35):self.box(x,.24,z,.23,.45,.52,11,11,11)
        else:
            self.car_shell()
            for x in (-.98,.98):
                for z in (-1.36,1.35):self.wheel(x,.39,z)
    def truck(self,lod):
        self.box(0,.70,-1.9,2.45,.42,5.6,22,22,7)
        self.box(0,.94,1.20,2.52,2.55,2.65,16,17,16)
        self.quad((-1.12,2.13,2.56),(1.12,2.13,2.56),(.99,3.18,2.24),(-.99,3.18,2.24),13)
        self.box(0,1.18,2.61,1.35,.67,.12,22,22,22)
        self.box(0,1.80,-6.15,2.55,2.85,11.9,24,24,20)
        for side in (-1,1):
            self.box(side*1.00,1.39,2.65,.43,.28,.13,14,14,14)
            self.box(side*1.03,1.39,-12.15,.42,.22,.12,15,15,15)
        if lod==0:
            for x in (-1.20,1.20):
                for z in (1.5,-1.9,-9.0,-10.6,-11.6):self.wheel(x,.75,z,.67,.24,8)
            for side in (-1,1):
                self.box(side*1.30,2.40,1.18,.10,.68,1.0,13,13,13)
                self.box(side*1.38,2.11,2.25,.23,.18,.32,7,7,7)
            for i in range(8):
                self.box(-1.24+i*.35,2.9,-.65,.08,.12,.08,7,7,7)
        elif lod==1:
            for x in (-1.20,1.20):
                for z in (1.5,-1.9,-9.0,-10.6):self.box(x,.25,z,.31,.93,1.0,11,11,11)
    def building(self,lod):
        self.box(0,0,0,15,18,13,24,25,7)
        self.box(0,17.8,0,15.5,.32,13.5,7,7,7)
        if lod==2:return
        self.box(0,.02,6.62,2.6,3.0,.08,22,22,22)
        self.box(0,2.9,6.78,4.2,.18,.60,20,20,20)
        for floor in range(1 if lod==1 else 5):
            y=3.4+floor*2.8
            for col in range(2 if lod==1 else 5):
                x=(col-2)*2.7 if lod==0 else (col-.5)*5.5
                self.box(x,y,6.58,1.7,1.65,.07,4,4,4)
                if lod==0:
                    self.box(x,y,6.64,.08,1.65,.08,7,7,7)
                    self.box(x,y+.78,6.65,1.7,.07,.09,7,7,7)
        if lod==0:
            for i in range(3):self.box(-5+i*4,18.12,-3,1.7,.85,2.2,7,7,7)
            for i in range(4):self.box(-5+i*3.2,0,6.66,.15,2.8,.11,7,7,7)
    def warehouse(self,lod):
        self.box(0,0,0,27,9,18,24,25,7)
        self.box(0,8.9,0,28,.5,19,7,7,7)
        if lod==2:return
        for x in (-8.5,0,8.5):
            self.box(x,.12,9.08,6.5,5.4,.12,22,22,22)
            self.box(x,5.5,9.15,7.0,.36,.22,21,21,21)
        if lod==0:
            for x in (-9.5,-6.5,-1.5,1.5,6.5,9.5):
                self.box(x,6.7,9.08,1.4,1.0,.09,13,13,13)
            for x in (-10,10):self.box(x,9.35,-4,1.2,.8,4.5,7,7,7)
    def streetlamp(self,lod):
        self.box(0,0,0,.16,7.2,.16,7,7,7)
        self.box(.90,7.02,0,1.95,.15,.16,7,7,7)
        self.box(1.80,6.83,0,.75,.16,.55,23,23,23)
        if lod==0:
            self.box(0,0,0,.40,.38,.40,7,7,7)
            self.box(1.80,6.74,0,.58,.05,.38,23,23,23)
    def tree(self,lod):
        self.box(0,0,0,.55,4.3,.55,10,10,10)
        for k in range(1 if lod==2 else (2 if lod==1 else 3)):
            base=2.0+k*1.3;wide=3.8-k*.65;top=base+3.1
            self.tri((-wide/2,base,wide/2),(wide/2,base,wide/2),(0,top,0),9)
            self.tri((wide/2,base,wide/2),(wide/2,base,-wide/2),(0,top,0),9)
            self.tri((wide/2,base,-wide/2),(-wide/2,base,-wide/2),(0,top,0),9)
            self.tri((-wide/2,base,-wide/2),(-wide/2,base,wide/2),(0,top,0),9)
    def grass(self,lod):
        self.quad((-.5,0,0),(.5,0,0),(.30,.8,0),(-.25,.67,0),8)
        if lod<2:self.quad((0,0,-.5),(0,0,.5),(0,.76,.28),(0,.68,-.27),9)
        if lod==0:self.quad((-.4,0,-.4),(.4,0,.4),(.1,.95,.2),(-.25,.6,-.15),8)
    def bridge(self,lod):
        self.box(0,4,0,15,1,48,6,24,6)
        if lod==2:return
        for side in (-1,1):
            self.box(side*7.3,5,0,.25,1.2,48,7,7,7)
            if lod==0:
                for z in range(-20,21,10):self.box(side*7.3,4,z,.30,3.0,.30,7,7,7)
        for z in (-16,16):self.box(0,-1,z,3,5,2.5,24,25,24)
    def puddle(self,lod):
        # Slightly above asphalt to prevent z-fighting; transparent blending optional.
        self.quad((-1.5,.03,-.6),(1.5,.03,-.7),(1.7,.03,.5),(-1.3,.03,.65),19)
        if lod==0:self.quad((-.7,.04,-.38),(.65,.04,-.42),(.94,.04,-.25),(-.50,.04,-.20),18)

NAMES=['car','truck','building','warehouse','streetlamp','tree','grass','bridge','puddle']

def generate(out):
    chunks=['/* GENERATED by tools/assets/generate_meshes.py; DO NOT EDIT. */',
            '/* Coordinates in 1/64 metre units, 3 LODs per asset. */']
    metadata=[]
    for name in NAMES:
        for lod in range(3):
            mesh=Mesh();getattr(mesh,name)(lod)
            key=f'rh_{name}_lod{lod}'
            chunks.append(f'static const RHAssetVertexQ {key}_v[] = {{')
            for x,y,z in mesh.vertices:chunks.append(f' {{{x},{y},{z}}},')
            chunks.append('};')
            chunks.append(f'static const RHAssetFaceQ {key}_f[] = {{')
            for a,b,c,mat,em in mesh.triangles:chunks.append(f' {{{a},{b},{c},{mat},{em}}},')
            chunks.append('};')
            metadata.append((key,len(mesh.vertices),len(mesh.triangles)))
    chunks.append('static const RHAssetMeshQ rh_asset_meshes[RH_ASSET_COUNT][3] = {')
    for name in NAMES:
        chunks.append(' {')
        for lod in range(3):
            key=f'rh_{name}_lod{lod}'
            v,t=next((v,t) for k,v,t in metadata if k==key)
            chunks.append(f'  {{{key}_v,{key}_f,{v},{t}}},')
        chunks.append(' },')
    chunks.append('};')
    data='\n'.join(chunks)+'\n'
    Path(out).write_text(data,encoding='utf-8')
    print('generated',len(data),'bytes',sum(x[1] for x in metadata),'vertices',sum(x[2] for x in metadata),'triangles')
    for name in NAMES:
        print(name,[(v,t) for key,v,t in metadata if key.startswith(f'rh_{name}_')])
if __name__=='__main__':
    generate(Path(__file__).resolve().parents[2]/'src'/'asset_mesh_data.inc')
