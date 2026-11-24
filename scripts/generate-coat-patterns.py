#!/usr/bin/env python3
"""Canonical approved coat artwork -> native Swift/C++ meshes (stdlib only)."""
import argparse
from pathlib import Path
import math
ROOT=Path(__file__).resolve().parents[1]
MILK=0xF2EEE4; GINGER=0xD8AA73; DARK=0x363536

def curve(a,b,c,d,t):
    u=1-t
    return (u*u*u*a[0]+3*u*u*t*b[0]+3*u*t*t*c[0]+t*t*t*d[0],u*u*u*a[1]+3*u*u*t*b[1]+3*u*t*t*c[1]+t*t*t*d[1])
def shape(marks,head,start,curves,rgb,alpha=1):
    pts=[start]; last=start
    for end,c1,c2 in curves:
        pts += [curve(last,c1,c2,end,i/12) for i in range(1,13)];last=end
    marks.append((head,0,rgb,alpha,pts))
def ellipse(marks,head,rect,rgb):
    x,y,w,h=rect
    pts=[(x+w/2+w/2*math.cos(i*math.tau/64),y+h/2+h/2*math.sin(i*math.tau/64)) for i in range(64)]
    marks.append((head,0,rgb,1,pts))
def rect(marks,head,x,y,w,h,rgb):marks.append((head,0,rgb,1,[(x,y),(x+w,y),(x+w,y+h),(x,y+h)]))
def stroke(marks,head,a,c,b,width,rgb,alpha):
    pts=[((1-t)**2*a[0]+2*(1-t)*t*c[0]+t*t*b[0],(1-t)**2*a[1]+2*(1-t)*t*c[1]+t*t*b[1]) for t in [i/16 for i in range(17)]]
    marks.append((head,width,rgb,alpha,pts))
def skull_ellipse(marks,rect,rgb,alpha=1):
    """Ellipse glued flat to the head (moves with the skull, no 2.5D wrap)."""
    x,y,w,h=rect
    pts=[(x+w/2+w/2*math.cos(i*math.tau/64),y+h/2+h/2*math.sin(i*math.tau/64)) for i in range(64)]
    marks.append((2,0,rgb,alpha,pts))
def muzzle_bib(marks,color):
    """Rounded muzzle flowing into a chest bib (shared by white-chested coats)."""
    shape(marks,True,(0,-24),[
        ((-9,7),(-5,-16),(-4,0)),((-29,20),(-15,13),(-25,14)),
        ((-33,30),(-34,23),(-32,26)),((-70,48),(-37,35),(-58,38)),
        ((70,48),(-30,70),(30,70)),((33,30),(58,38),(37,35)),
        ((29,20),(32,26),(34,23)),((9,7),(25,14),(15,13)),
        ((0,-24),(4,0),(5,-16))],color)
def artwork(silver):
    m=[]
    if silver:
        ellipse(m,False,(-36,-81,72,130),MILK)
        for side in [-1,1]:
            stroke(m,False,(side*49,-41),(side*37,-44),(side*33,-35),3.5,DARK,.75)
            stroke(m,False,(side*49,-20),(side*39,-25),(side*35,-14),3.2,DARK,.75)
    else:
        shape(m,False,(51,-73),[((29,-38),(23,-76),(21,-59)),((47,4),(35,-16),(20,4))],GINGER)
        ellipse(m,False,(-59,-65,28,51),GINGER)
        # Skull-attached orange volume: its edges lie beyond the clipped cat silhouette.
        # The curved nape stays orange when the facial markings turn away.
        shape(m,2,(-100,-100),[
            ((100,-100),(-30,-100),(30,-100)),((100,14),(100,-70),(100,-15)),
            ((-100,14),(45,55),(-45,55)),((-100,-100),(-100,-15),(-100,-70))],GINGER)
    col=DARK if silver else 0xA57543;alpha=.86 if silver else .48
    shape(m,True,(-13,-33),[((-9,-16),(-17,-27),(-7,-23)),((-5,-30),(-11,-22),(-3,-26)),((-13,-33),(-7,-30),(-10,-32))],col,alpha)
    shape(m,True,(0,-34),[((-2,-22),(-5,-29),(3,-25)),((1,-12),(-5,-18),(0,-15)),((5,-24),(5,-18),(7,-20)),((2,-34),(2,-29),(6,-31))],col,alpha)
    shape(m,True,(14,-33),[((11,-18),(18,-27),(9,-23)),((7,-31),(13,-24),(6,-25))],col,alpha)
    for side in [-1,1]:
        shape(m,True,(side*22,-30),[((side*16,-15),(side*24,-23),(side*18,-17)),((side*18,-30),(side*18,-21),(side*20,-24))],col,alpha)
        stroke(m,True,(side*43,3),(side*35,8),(side*29,7),3.2 if silver else 2.1,col,alpha)
        stroke(m,True,(side*43,14),(side*36,16),(side*31,15),2.8 if silver else 1.8,col,alpha)
    if silver:
        top=-16; hw=6
        shape(m,True,(-hw,top),[
            ((-9,7),(-6,top+(5)),(-6,0)),
            ((-30,18),(-15,13),(-24,14)),((-18,34),(-32,27),(-24,33)),
            ((18,34),(-8,35),(8,35)),((30,18),(24,33),(32,27)),((9,7),(24,14),(15,13)),
            ((hw,top),(6,0),(6,top+(5)))],MILK)
    else:
        # The white muzzle becomes a rounded bib extending into the white chest.
        muzzle_bib(m,MILK)
    return m if silver else [mark for mark in m if mark[0]] + [mark for mark in m if not mark[0]]

def tuxedo():
    """燕尾服奶牛猫：墨黑被毛 + 白色胸口围兜与鼻梁白斑。"""
    m=[]
    white=0xF4F1EA
    # Slim blaze running from the forehead down between the eyes.
    shape(m,True,(0,-32),[((-6,-4),(-8,-18),(1,-10)),((6,-4),(7,-18),(-1,-10)),((0,-32),(-3,-18),(3,-18))],white)
    muzzle_bib(m,white)
    return m

def calico():
    """三花猫：奶白底色 + 头顶橘/墨两块色斑 + 身体两侧花斑。"""
    m=[]
    ORANGE=0xD99A5B; DARKP=0x413A3A; CREAM=0xF7F3EC
    skull_ellipse(m,(-64,-54,74,60),ORANGE)
    skull_ellipse(m,(-2,-60,72,56),DARKP)
    muzzle_bib(m,CREAM)
    # Flank patches sit on top of the bib so the calico tricolor reads clearly.
    ellipse(m,False,(16,-64,36,48),ORANGE)
    ellipse(m,False,(-52,-46,36,46),DARKP)
    return m

def ginger_tabby():
    """橘猫虎斑：暖橘底色 + 深棕虎斑纹 + 奶油色口鼻胸巾。"""
    m=[]
    RUST=0xA9723C; CREAM=0xF7E8D2
    for side in [-1,1]:
        for y0,cx,cy,y1,w in [(-58,-20,-66,-46,3.6),(-40,-16,-48,-28,4.0),(-22,-14,-30,-12,3.2)]:
            stroke(m,False,(side*36,y0),(side*cx,cy),(side*32,y1),w,RUST,0.58)
        for y in (2,13):
            stroke(m,True,(side*43,y),(side*35,y+5),(side*29,y+4),2.2,RUST,0.72)
        stroke(m,True,(side*22,-32),(side*14,-18),(side*3,-27),2.4,RUST,0.78)
    shape(m,True,(0,-34),[((-1,-23),(-3,-29),(1,-26)),((2,-13),(-1,-19),(1,-16)),((0,-34),(-1,-29),(1,-29))],RUST,0.78)
    muzzle_bib(m,CREAM)
    return m

def british_blue():
    """英短蓝猫：烟蓝灰被毛 + 略浅的口鼻色块（无虎斑）。"""
    m=[]
    ellipse(m,True,(-21,-1,42,27),0xA3AEBA)
    return m

def n(v):
    s=f'{v:.5f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0','') else s

def outputs():
    skins=[('shorthair',artwork(True)),('orangeWhite',artwork(False)),('tuxedo',tuxedo()),
           ('calico',calico()),('gingerTabby',ginger_tabby()),('britishBlue',british_blue())]
    swift='// Generated by scripts/generate-coat-patterns.py; edit the canonical artwork there.\nimport CoreGraphics\nstruct CoatMark {\n    let head: Bool\n    let skull: Bool\n    let width: CGFloat\n    let rgb: Int\n    let alpha: CGFloat\n    let points: [CGPoint]\n}\nenum CoatPatterns {\n'
    cpp='// Generated by scripts/generate-coat-patterns.py; shared by Windows and Linux.\n#pragma once\n#include <vector>\nstruct CoatPoint { float x, y; };\nstruct CoatMark { bool head; bool skull; float width; unsigned int rgb; float alpha; std::vector<CoatPoint> points; };\n'
    for skin,marks in skins:
        swift+=f'    static let {skin}: [CoatMark] = [\n'
        cpp+=f'inline const std::vector<CoatMark>& {skin}Marks() {{\n    static const std::vector<CoatMark> marks = {{\n'
        for head,w,rgb,a,pts in marks:
            swift+=f'        CoatMark(head: {str(bool(head)).lower()}, skull: {str(head == 2).lower()}, width: {n(w)}, rgb: 0x{rgb:06X}, alpha: {n(a)}, points: [\n'
            swift+='            '+', '.join(f'CGPoint(x: {n(x)}, y: {n(y)})' for x,y in pts)+']),\n'
            cpp+=f'        {{{str(bool(head)).lower()}, {str(head == 2).lower()}, {float(w):.5f}f, 0x{rgb:06X}, {float(a):.5f}f, {{\n'
            cpp+='            '+', '.join('{'+f'{float(x):.5f}f, {float(y):.5f}f'+'}' for x,y in pts)+'}},\n'
        swift+='    ]\n';cpp+='    };\n    return marks;\n}\n'
    swift+='    static func marks(for coat: CatCoatTheme) -> [CoatMark] {\n        switch coat {\n        case .shorthair: return shorthair\n        case .orangeWhite: return orangeWhite\n        case .tuxedo: return tuxedo\n        case .calico: return calico\n        case .gingerTabby: return gingerTabby\n        case .britishBlue: return britishBlue\n        default: return []\n        }\n    }\n}\n'
    return {ROOT/'Sources/CoatPatterns.swift':swift,ROOT/'shared/CoatPatterns.hpp':cpp}
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--check',action='store_true');args=p.parse_args()
    for path,text in outputs().items():
        if args.check:
            if not path.exists() or path.read_text()!=text:raise SystemExit(f'Generated artwork is stale: {path}')
        else:path.write_text(text)
    print('Shared coat artwork is consistent' if args.check else 'Generated native coat artwork')
