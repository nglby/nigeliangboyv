# -*- coding: utf-8 -*-
"""
销售数据管理系统 —— OBSIDIAN DASHBOARD
全 PIL 渲染：玻璃态卡片 + 手绘表格 + 高端图表
"""
import tkinter as tk
from tkinter import messagebox
from order import Order, PRODUCTS, SALESPERSONS
from manager import OrderManager
import data_handler
import math
import random
import time as _time
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageTk

# ==================== 设计系统 ====================
BG   = "#0D1117"
SURF = "#161B22"
CARD = "#1C2128"
CARD2= "#21262D"
BDR  = "#30363D"
WT   = "#E6EDF3"
WT2  = "#C9D1D9"
GR   = "#7D8590"
BL   = "#58A6FF"
GN   = "#3FB950"
AM   = "#D29922"
RD   = "#F85149"
PU   = "#BC8CFF"
CY   = "#39D2C0"
PK   = "#F778BA"
OG   = "#F0883E"

# 预计算混合色
def _m(fg, alpha):  # alpha 0.0-1.0
    def h2i(s): return int(s[1:3],16),int(s[3:5],16),int(s[5:7],16)
    fr,fgv,fb = h2i(fg); br,bgv,bb = h2i(BG)
    return f"#{int(fr*alpha+br*(1-alpha)):02x}{int(fgv*alpha+bgv*(1-alpha)):02x}{int(fb*alpha+bb*(1-alpha)):02x}"

BL05 = _m(BL,0.05); BL10 = _m(BL,0.10); BL15 = _m(BL,0.15); BL20 = _m(BL,0.20)
BL30 = _m(BL,0.30); BL40 = _m(BL,0.40); BL60 = _m(BL,0.60)
GN05 = _m(GN,0.05); GN10 = _m(GN,0.10); GN20 = _m(GN,0.20)
AM05 = _m(AM,0.05); AM10 = _m(AM,0.10); AM20 = _m(AM,0.20)
PU05 = _m(PU,0.05); PU10 = _m(PU,0.10); PU20 = _m(PU,0.20)
RD05 = _m(RD,0.05); RD10 = _m(RD,0.10); RD20 = _m(RD,0.20)
CY05 = _m(CY,0.05); CY10 = _m(CY,0.10); CY20 = _m(CY,0.20)

ACC = [BL, GN, AM, PU, CY, RD, PK, OG]
SALESPERSON_AVATAR = {
    "哪吒":"\U0001F525","奥特曼":"\u26A1","悟空":"\U0001F435",
    "葫芦娃":"\U0001F383","柯南":"\U0001F50D","鸣人":"\U0001F365",
    "路飞":"\U0001F3F4\u200D\u2620\uFE0F","皮卡丘":"\u26A1",
    "多啦A梦":"\U0001F514","樱桃小丸子":"\U0001F338"
}

# ==================== PIL 渲染引擎 ====================
def _pil_to_tk(img):
    return ImageTk.PhotoImage(img)

def render_glass_card(w, h, accent=None, radius=12):
    """渲染玻璃态卡片"""
    img = Image.new("RGBA", (w, h), (0,0,0,0))
    draw = ImageDraw.Draw(img)
    r = radius
    # 底层填充
    draw.rounded_rectangle([0, 0, w-1, h-1], r, fill=(28,33,40,220))
    # 边框
    draw.rounded_rectangle([0, 0, w-1, h-1], r, outline=(48,54,61,120), width=1)
    # 顶部高光
    draw.rounded_rectangle([1, 1, w-2, h//2+10], r, fill=(255,255,255,3))
    # accent 左边条
    if accent:
        ac = accent.replace("#","")
        ar,ag,ab = int(ac[0:2],16),int(ac[2:4],16),int(ac[4:6],16)
        draw.rectangle([0, r, 3, h-r], fill=(ar,ag,ab,180))
        draw.rectangle([0, r, 3, h-r], fill=(ar,ag,ab,60))
    return _pil_to_tk(img)

def render_gradient_hdr(w, h, c1, c2):
    """渲染渐变条"""
    img = Image.new("RGBA", (w, h), (0,0,0,0))
    draw = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(h-1, 1)
        def blend(a,b): return int(a[0]*t+b[0]*(1-t)),int(a[1]*t+b[1]*(1-t)),int(a[2]*t+b[2]*(1-t))
        a1 = (int(c1[1:3],16),int(c1[3:5],16),int(c1[5:7],16))
        a2 = (int(c2[1:3],16),int(c2[3:5],16),int(c2[5:7],16))
        r,g,b = blend(a1,a2)
        draw.line([(0,y),(w,y)], fill=(r,g,b,255))
    return _pil_to_tk(img)

def render_bar_chart(data, colors, w, h, pad=(45,15,25,28)):
    """渲染专业柱状图(PIL抗锯齿)"""
    img = Image.new("RGBA", (w, h), (0,0,0,0))
    draw = ImageDraw.Draw(img)
    if not data: return _pil_to_tk(img)
    max_v = max(v for _,v in data) if data else 1
    pl,pt,pr,pb = pad
    cw,ch = w-pl-pr, h-pt-pb
    # 网格线
    for i in range(5):
        y = int(pt + ch*i/4)
        draw.line([(pl,y),(pl+cw,y)], fill=(48,54,61,60), width=1)
    # 柱状体
    n = len(data)
    bar_w = int(cw / n * 0.65) if n>1 else int(cw*0.4)
    gap = (cw - bar_w*n) // (n+1) if n>1 else (cw-bar_w)//2
    for i,(label,val) in enumerate(data):
        x = pl + gap + i*(bar_w+gap)
        bh = int((val/max(max_v,1))*ch)
        y = pt + ch - bh
        # 柱体渐变
        ac = colors[i%len(colors)]
        aci = (int(ac[1:3],16),int(ac[3:5],16),int(ac[5:7],16))
        dark = tuple(int(c*0.25) for c in aci)
        mid  = tuple(int(c*0.5) for c in aci)
        lite = tuple(int(c*0.75) for c in aci)
        draw.rectangle([x,y,x+bar_w,pt+ch], fill=dark)
        draw.rectangle([x+1,y,x+bar_w-1,pt+ch], fill=mid)
        draw.rectangle([x+2,y,x+bar_w//2+x,pt+ch], fill=lite)
        draw.rectangle([x,y,y+2], (x+bar_w,y+2), fill=aci+(180,))
        draw.rectangle([x,y,x+bar_w,pt+ch], outline=aci+(40,), width=1)
        # 标签
        try:
            fnt = ImageFont.truetype("segoeui.ttf", 10)
        except:
            fnt = ImageFont.load_default()
        ll = label[:8] if len(label)>8 else label
        bbox = draw.textbbox((0,0), ll, font=fnt)
        tw = bbox[2]-bbox[0]
        draw.text((x+bar_w//2-tw//2, pt+ch+4), ll, fill=(125,133,144,200), font=fnt)
        # 数值
        vt = f"\xa5{val:,.0f}"
        bbox2 = draw.textbbox((0,0), vt, font=fnt)
        tw2 = bbox2[2]-bbox2[0]
        draw.text((x+bar_w//2-tw2//2, y-14), vt, fill=aci+(220,), font=fnt)
    return _pil_to_tk(img)

def render_area_chart(data, color, w, h, pad=(40,10,15,25)):
    """渲染面积图"""
    img = Image.new("RGBA", (w, h), (0,0,0,0))
    draw = ImageDraw.Draw(img)
    if not data: return _pil_to_tk(img)
    max_v = max(v for _,v in data) if data else 1
    pl,pt,pr,pb = pad
    cw,ch = w-pl-pr, h-pt-pb
    # 网格
    for i in range(5):
        y = int(pt + ch*i/4)
        draw.line([(pl,y),(pl+cw,y)], fill=(48,54,61,60), width=1)
    # 坐标点
    n = len(data)
    points = []
    for i,(_,val) in enumerate(data):
        x = pl + int(cw * i / max(n-1,1)) if n>1 else pl + cw//2
        y = pt + ch - int((val/max(max_v,1))*ch)
        points.append((x,y))
    # 面积填充
    if n>=2:
        poly_fill = points + [(points[-1][0], pt+ch), (points[0][0], pt+ch)]
        ac = color.replace("#","")
        aci = (int(ac[0:2],16),int(ac[2:4],16),int(ac[4:6],16))
        draw.polygon(poly_fill, fill=aci+(40,))
        draw.polygon(poly_fill, fill=aci+(20,))
    # 折线
    if n>=2:
        ac = color.replace("#","")
        aci = (int(ac[0:2],16),int(ac[2:4],16),int(ac[4:6],16))
        draw.line(points, fill=aci+(255,), width=2)
        for px,py in points:
            draw.ellipse([px-3,py-3,px+3,py+3], fill=aci+(255,))
    # 月份标签
    try:
        fnt = ImageFont.truetype("segoeui.ttf", 9)
    except:
        fnt = ImageFont.load_default()
    for i,(month,_) in enumerate(data):
        x = pl + int(cw * i / max(n-1,1)) if n>1 else pl + cw//2
        ml = month[-2:] if len(month)>3 else month
        bbox = draw.textbbox((0,0), ml, font=fnt)
        tw = bbox[2]-bbox[0]
        draw.text((x-tw//2, pt+ch+4), ml, fill=(125,133,144,180), font=fnt)
    return _pil_to_tk(img)

def render_hbar_chart(data, colors, w, h):
    """水平柱状图 PIL 渲染"""
    img = Image.new("RGBA", (w, h), (0,0,0,0))
    draw = ImageDraw.Draw(img)
    if not data: return _pil_to_tk(img)
    max_v = max(v for _,v in data) if data else 1
    n = len(data)
    bar_h = max(14, min(22, (h-8)//n-4))
    bar_w = w - 20
    try:
        fnt = ImageFont.truetype("segoeui.ttf", 9)
    except:
        fnt = ImageFont.load_default()
    for i,(name,val) in enumerate(data):
        y = 4 + i*(bar_h+4)
        bw = max(30, int((val/max(max_v,1))*bar_w))
        ac = colors[i%len(colors)]
        aci = (int(ac[1:3],16),int(ac[3:5],16),int(ac[5:7],16))
        # 背景
        draw.rectangle([12,y,12+bar_w,y+bar_h], fill=(22,27,34,200))
        # 填充条
        draw.rectangle([12,y,12+bw,y+bar_h], fill=tuple(int(c*0.35) for c in aci))
        draw.rectangle([12,y,12+int(bw*0.7),y+bar_h], fill=tuple(int(c*0.55) for c in aci))
        draw.rectangle([12,y,12+int(bw*0.35),y+bar_h], fill=tuple(int(c*0.75) for c in aci))
        draw.rectangle([12,y,12+bw,y+bar_h], outline=aci+(50,), width=1)
        draw.line([(12,y+1),(12+bw,y+1)], fill=aci+(180,), width=1)
        # 标签
        ll = name[:10]
        draw.text((16, y+bar_h//2-5), ll, fill=(230,237,243,230), font=fnt)
        # 数值
        vt = f"\xa5{val:,.0f}"
        bbox = draw.textbbox((0,0), vt, font=fnt)
        tw = bbox[2]-bbox[0]
        draw.text((12+bar_w-tw-2, y+bar_h//2-5), vt, fill=aci+(230,), font=fnt)
    return _pil_to_tk(img)


# ==================== 粒子浮层（保留并增强）====================
class ParticleOverlay(tk.Toplevel):
    def __init__(self, master, w=1400, h=820):
        super().__init__(master)
        self.master = master
        self.overrideredirect(True)
        self.wm_attributes("-topmost", True)
        self.wm_attributes("-transparentcolor", BG)
        self.wm_attributes("-disabled", True)
        self._ok = True
        self._fw, self._fh = w, h
        self._meteors, self._stars, self._particles, self._pulses = [],[],[],[]
        self._sync()
        self.geometry(f"{w}x{h}")
        self.cv = tk.Canvas(self, width=w, height=h, bg=BG, highlightthickness=0,bd=0)
        self.cv.pack(fill="both",expand=True)

    def init_draw(self):
        self.cv.delete("all")
        self._meteors.clear(); self._stars.clear()
        self._particles.clear(); self._pulses.clear()
        # 星空 240 颗
        for _ in range(240):
            x,y = random.randint(0,self._fw),random.randint(0,self._fh)
            r = random.uniform(0.3,2.0)
            c = random.choice(["#FFFFFF"]*3+["#A5D8FF","#C4B5FD","#FDE68A","#93C5FD","#A7F3D0"])
            s = self.cv.create_oval(x,y,x+r,y+r,fill=c,outline="")
            self._stars.append({"id":s,"x":x,"y":y,"r":r,"tw":random.uniform(0,6.28),"sp":random.uniform(0.015,0.05)})
        # 流星 8 颗
        for _ in range(8): self._spawn_meteor()
        # 浮动粒子 100
        for _ in range(100):
            self._spawn_particle(random.randint(0,self._fw),random.randint(0,self._fh))

    def _spawn_meteor(self):
        x1 = random.randint(-200,self._fw+200); y1 = random.randint(-300,-20)
        length = random.randint(70,220); ang = math.radians(random.randint(20,50))
        x2 = x1+length*math.cos(ang); y2 = y1+length*math.sin(ang)
        sp = random.uniform(4.0,9.0)
        color = random.choice([BL,GN,PU,CY,PK,OG,"#FFFFFF"])
        m = {"x":x1,"y":y1,"x2":x2,"y2":y2,"sp":sp,"color":color,"tails":[],"head":None,"glow":None}
        for i in range(10):
            r = i/10; a = int(6+(255-6)*(1-r)*0.6)
            tx=x1+(x2-x1)*r*0.5; ty=y1+(y2-y1)*r*0.5; tx2=x1+(x2-x1)*r; ty2=y1+(y2-y1)*r
            c = _m(color,a/255); tid = self.cv.create_line(tx,ty,tx2,ty2,fill=c,width=2.2-r*1.1,smooth=True)
            m["tails"].append(tid)
        hid = self.cv.create_oval(x1-2,y1-2,x1+3,y1+3,fill="#FFFFFF",outline="")
        gid = self.cv.create_oval(x1-5,y1-5,x1+6,y1+6,outline=_m(color,0.3),width=1)
        m["head"]=hid; m["glow"]=gid
        self._meteors.append(m)

    def _spawn_particle(self,x,y):
        w = random.uniform(1.0,3.5); h = random.uniform(0.5,1.5)
        sp = random.uniform(0.2,1.6); dr = random.uniform(-0.25,0.25)
        color = random.choice([BL,GN,PU,CY])
        pid = self.cv.create_rectangle(x,y,x+w,y+h,fill=_m(color,random.randint(40,160)/255),outline="")
        self._particles.append({"id":pid,"x":x,"y":y,"w":w,"h":h,"sp":sp,"dr":dr,"color":color,"ph":random.uniform(0,6.28)})

    def _spawn_pulse(self):
        cx,cy = random.randint(50,self._fw-50),random.randint(50,self._fh-50)
        color = random.choice([BL,GN,PU,CY])
        pid = self.cv.create_oval(cx,cy,cx+2,cy+2,outline=_m(color,0.4),width=1.5)
        self._pulses.append({"id":pid,"x":cx,"y":cy,"r":2,"color":color,"life":0,"max_life":random.randint(25,45)})

    def tick(self):
        if not self._ok: return
        t = _time.time()
        for s in self._stars:
            tw = 0.25+0.75*math.sin(t*s["sp"]*16+s["tw"])
            r = s["r"]*(0.35+0.65*tw); self.cv.coords(s["id"],s["x"]-r,s["y"]-r,s["x"]+r,s["y"]+r)
        for m in self._meteors[:]:
            m["x"]+=m["sp"]*math.cos(math.radians(36))
            m["y"]+=m["sp"]*math.sin(math.radians(36))
            if m["x"]>self._fw+300 or m["y"]>self._fh+200:
                for tid in m["tails"]: self.cv.delete(tid)
                if m["head"]: self.cv.delete(m["head"])
                if m["glow"]: self.cv.delete(m["glow"])
                self._meteors.remove(m); self._spawn_meteor()
            else:
                dx,dy = m["x2"]-m["x"],m["y2"]-m["y"]
                for i,tid in enumerate(m["tails"]):
                    r=i/len(m["tails"]); tx=m["x"]+dx*r*0.5; ty=m["y"]+dy*r*0.5
                    tx2=m["x"]+dx*r; ty2=m["y"]+dy*r
                    self.cv.coords(tid,tx,ty,tx2,ty2)
                if m["head"]: self.cv.coords(m["head"],m["x"]-2,m["y"]-2,m["x"]+3,m["y"]+3)
                if m["glow"]: self.cv.coords(m["glow"],m["x"]-5,m["y"]-5,m["x"]+6,m["y"]+6)
        if random.random()<0.04 and len(self._meteors)<10: self._spawn_meteor()
        for p in self._particles[:]:
            fl = 0.45+0.55*math.sin(t*3.5+p["ph"])
            p["y"]-=p["sp"]; p["x"]+=p["dr"]
            if p["y"]<-20: self.cv.delete(p["id"]); self._particles.remove(p); self._spawn_particle(random.randint(0,self._fw),random.randint(self._fh,self._fh+100))
            else:
                a = int(140*fl)
                self.cv.itemconfig(p["id"],fill=_m(p["color"],a/255))
                self.cv.coords(p["id"],p["x"],p["y"],p["x"]+p["w"],p["y"]+p["h"])
        if random.random()<0.01 and len(self._particles)<115: self._spawn_particle(random.randint(0,self._fw),random.randint(self._fh,self._fh+100))
        for p in self._pulses[:]:
            p["life"]+=1; p["r"]+=2.5; rat = p["life"]/p["max_life"]
            alpha = int((1-rat)*100)
            self.cv.itemconfig(p["id"],outline=_m(p["color"] if rat<0.5 else BG,alpha/255))
            rr=p["r"]; self.cv.coords(p["id"],p["x"]-rr,p["y"]-rr,p["x"]+rr,p["y"]+rr)
            if p["life"]>=p["max_life"]: self.cv.delete(p["id"]); self._pulses.remove(p)
        if random.random()<0.006: self._spawn_pulse()
        try: self.update_idletasks()
        except: pass

    def _sync(self):
        try:
            x,y = self.master.winfo_rootx(),self.master.winfo_rooty()
            w,h = self.master.winfo_width(),self.master.winfo_height()
            if w>10 and h>10: self._fw,self._fh=w,h; self.geometry(f"{w}x{h}+{x}+{y}")
        except: pass

    def destroy(self):
        self._ok = False
        try: super().destroy()
        except: pass


# ==================== Canvas 手绘表格 ====================
class CanvasTable(tk.Canvas):
    """完全手绘的数据表格——支持排序、滚动、悬浮高亮"""
    def __init__(self, parent, columns, **kw):
        super().__init__(parent, bg=SURF, highlightthickness=1, highlightbackground=BDR, bd=0, **kw)
        self.columns = columns  # [(key, title, width, align), ...]
        self._data = []
        self._row_h = 34
        self._hdr_h = 40
        self._hover_row = -1
        self._selected_row = -1
        self._scroll_offset = 0
        self._sort_col = None
        self._sort_asc = True
        self._visible_rows = 0
        self._total_h = 0

        self.bind("<Configure>", self._on_resize)
        self.bind("<Motion>", self._on_motion)
        self.bind("<Leave>", lambda e: self._set_hover(-1))
        self.bind("<Button-1>", self._on_click)
        self.bind("<MouseWheel>", self._on_scroll)
        self.bind("<Double-1>", self._on_dbl)
        self._dbl_cb = None

    def bind_dbl_click(self, cb):
        self._dbl_cb = cb

    def set_data(self, rows):
        """rows: list of dicts [{col_key: value}, ...]"""
        self._data = rows
        self._scroll_offset = 0
        self._draw()

    def _on_resize(self, e=None):
        self._draw()

    def _on_scroll(self, e):
        self._scroll_offset = max(0, min(self._scroll_offset - (1 if e.delta>0 else -1),
                                         max(0, len(self._data)-self._visible_rows)))
        self._draw()

    def _on_motion(self, e):
        if e.y < self._hdr_h:
            self._set_hover(-1)
            return
        row = self._scroll_offset + (e.y - self._hdr_h) // self._row_h
        if 0 <= row < len(self._data):
            self._set_hover(row)
        else:
            self._set_hover(-1)

    def _set_hover(self, row):
        if row != self._hover_row:
            self._hover_row = row
            self._draw()

    def _on_click(self, e):
        if e.y < self._hdr_h:
            # 点击表头 = 排序
            x = 0
            for key,title,width,align in self.columns:
                if x <= e.x < x+width:
                    self._sort(key)
                    return
                x += width
            return
        row = self._scroll_offset + (e.y - self._hdr_h) // self._row_h
        if 0 <= row < len(self._data):
            self._selected_row = row
            self._draw()
        else:
            self._selected_row = -1
            self._draw()

    def _on_dbl(self, e):
        row = self._scroll_offset + (e.y - self._hdr_h) // self._row_h
        if 0 <= row < len(self._data) and self._dbl_cb:
            self._dbl_cb(self._data[row])

    def _sort(self, key):
        if self._sort_col == key:
            self._sort_asc = not self._sort_asc
        else:
            self._sort_col = key
            self._sort_asc = True
        self._data.sort(key=lambda r: r.get(key,""), reverse=not self._sort_asc)
        self._draw()

    def selected_data(self):
        if 0 <= self._selected_row < len(self._data):
            return self._data[self._selected_row]
        return None

    def _draw(self):
        self.delete("all")
        w = self.winfo_width()
        if w < 10: return
        h = self.winfo_height()
        self._visible_rows = max(1, (h - self._hdr_h) // self._row_h)

        # 计算列总宽
        fixed_w = sum(c[2] for c in self.columns)
        col_widths = [c[2] for c in self.columns]

        # ── 表头 ──
        x = 0
        for i,(key,title,width,align) in enumerate(self.columns):
            cw = col_widths[i]
            # Header bg
            self.create_rectangle(x, 0, x+cw, self._hdr_h,
                                 fill="#0D1117", outline=BDR, width=1)
            # Title
            txt = title
            if self._sort_col == key:
                txt = ("\u25B2 " if self._sort_asc else "\u25BC ") + title.strip()
            self.create_text(x+cw//2, self._hdr_h//2, text=txt,
                           fill=WT if self._sort_col==key else GR,
                           font=("Segoe UI", 9, "bold"))
            # Accent bottom line
            self.create_line(x, self._hdr_h-1, x+cw, self._hdr_h-1,
                           fill=BL if self._sort_col==key else BDR, width=2)
            x += cw

        # ── 数据行 ──
        y0 = self._hdr_h
        for i in range(self._visible_rows):
            di = self._scroll_offset + i
            if di >= len(self._data):
                break
            row = self._data[di]
            y = y0 + i*self._row_h
            # Row background
            is_hover = (di == self._hover_row)
            is_sel = (di == self._selected_row)
            if is_sel:
                bg = BL15
            elif is_hover:
                bg = "#1C2633"
            elif di % 2 == 0:
                bg = "#161B22"
            else:
                bg = "#1A1F28"
            self.create_rectangle(0, y, w, y+self._row_h, fill=bg, outline="")
            # 底部细线
            self.create_line(0, y+self._row_h-1, w, y+self._row_h-1, fill="#21262D", width=1)
            # 左边选中指示条
            if is_sel:
                self.create_rectangle(0, y+2, 3, y+self._row_h-2, fill=BL, outline="")

            # Cells
            x = 0
            for j,(key,title,width,align) in enumerate(self.columns):
                cw = col_widths[j]
                val = str(row.get(key, ""))
                # Truncate
                # Determine text position
                if align == "e":
                    tx = x + cw - 12
                    anchor = "e"
                elif align == "center":
                    tx = x + cw//2
                    anchor = "center"
                else:
                    tx = x + 12
                    anchor = "w"
                # Color based on column
                fg = WT if not is_sel else WT
                if key == "total" or key == "unit_price":
                    fg = GN
                elif key == "order_id":
                    fg = BL
                elif key == "salesperson":
                    fg = PU
                # Truncate text if needed
                self.create_text(tx, y+self._row_h//2, text=val,
                               fill=fg, font=("Segoe UI", 9),
                               anchor=anchor)
                x += cw

        # ── 滚动条指示器 ──
        if len(self._data) > self._visible_rows:
            total = len(self._data)
            thumb_h = max(20, self._visible_rows/total * (h-self._hdr_h))
            thumb_y = self._hdr_h + (self._scroll_offset/total)*(h-self._hdr_h)
            self.create_rectangle(w-4, thumb_y, w-1, thumb_y+thumb_h,
                                 fill="#484F58", outline="")


# ==================== 订单弹窗（PIL 背景）====================
class AddOrderDialog(tk.Toplevel):
    def __init__(self, parent, on_success):
        super().__init__(parent)
        self.title(""); self.resizable(False,False)
        self.transient(parent); self.overrideredirect(True)
        self.focus_force()
        self.on_success = on_success
        self.configure(bg=BG)
        self.var_id = tk.StringVar()
        self.var_date = tk.StringVar(value="2025-01-15")
        self.var_prod = tk.StringVar()
        self.var_qty = tk.StringVar()
        self.var_price = tk.StringVar()
        self.var_sp = tk.StringVar()
        self._build(parent)

    def _build(self, parent):
        w,h = 440, 460
        px = parent.winfo_rootx()+(parent.winfo_width()-w)//2
        py = parent.winfo_rooty()+(parent.winfo_height()-h)//2
        self.geometry(f"{w}x{h}+{px}+{py}")

        # PIL 渲染玻璃态背景
        bg_img = render_glass_card(w, h, None, 14)
        tk.Label(self, image=bg_img, bg=BG).place(x=0,y=0)
        self._bg_img = bg_img

        # 标题
        tk.Label(self, text="New Mission Order",
                font=("Segoe UI", 15, "bold"), bg=CARD, fg=WT).place(x=30, y=18)

        # 关闭按钮
        cb = tk.Label(self, text="\u2715", font=("Segoe UI", 16),
                     bg=CARD, fg=GR, cursor="hand2")
        cb.place(x=w-35, y=12)
        def cbe(e): cb.config(fg=RD)
        def cbl(e): cb.config(fg=GR)
        cb.bind("<Enter>", cbe); cb.bind("<Leave>", cbl)
        cb.bind("<Button-1>", lambda e: self.destroy())

        # 表单
        rows = [
            ("Order ID", self.var_id, "entry", 80),
            ("Date", self.var_date, "entry", 80),
            ("Product", self.var_prod, "combo", 220),
            ("Quantity", self.var_qty, "entry", 80),
            ("Unit Price", self.var_price, "entry", 100),
            ("Agent", self.var_sp, "combo", 220),
        ]
        for i,(label,var,kind,sw) in enumerate(rows):
            y = 58 + i*48
            sl = tk.Label(self, text=label.upper(),
                         font=("Segoe UI", 9, "bold"),
                         bg=CARD, fg=GR)
            sl.place(x=28, y=y)
            if kind == "combo":
                vals = PRODUCTS if label=="Product" else SALESPERSONS
                from tkinter import ttk
                cm = ttk.Combobox(self, textvariable=var, values=vals,
                                 font=("Segoe UI", 10), width=28, state="readonly")
                cm.place(x=28, y=y+20, width=sw, height=30)
            else:
                e = tk.Entry(self, textvariable=var,
                           font=("Segoe UI", 10), bg="#0D1117", fg=WT,
                           insertbackground=BL, relief="flat", bd=0,
                           highlightthickness=1, highlightbackground=BDR,
                           highlightcolor=BL)
                e.place(x=28, y=y+20, width=sw, height=30)

        # 按钮
        self._btn_ok = tk.Label(self, text="  Confirm  ",
                               font=("Segoe UI", 11, "bold"),
                               bg=GN, fg="#0D1117", cursor="hand2",
                               padx=24, pady=8)
        self._btn_ok.place(x=50, y=395)
        self._btn_ok.bind("<Button-1>", lambda e: self._on_ok())
        self._btn_ok.bind("<Enter>", lambda e: self._btn_ok.config(bg="#2EA043"))
        self._btn_ok.bind("<Leave>", lambda e: self._btn_ok.config(bg=GN))

        self._btn_no = tk.Label(self, text="  Cancel  ",
                               font=("Segoe UI", 11),
                               bg=CARD2, fg=GR, cursor="hand2",
                               padx=24, pady=8,
                               highlightthickness=1, highlightbackground=BDR)
        self._btn_no.place(x=230, y=395)
        self._btn_no.bind("<Button-1>", lambda e: self.destroy())
        self._btn_no.bind("<Enter>", lambda e: self._btn_no.config(bg="#30363D",fg=WT))
        self._btn_no.bind("<Leave>", lambda e: self._btn_no.config(bg=CARD2,fg=GR))

    def _on_ok(self):
        try:
            qty = int(self.var_qty.get())
            price = float(self.var_price.get())
            o = Order(self.var_id.get().strip(), self.var_date.get().strip(),
                     self.var_prod.get().strip(), qty, price, self.var_sp.get().strip())
        except ValueError as e:
            messagebox.showerror("Error", str(e))
            return
        self.on_success(o)
        self.destroy()


# ==================== 主应用 ====================
class SalesApp:
    def __init__(self, root):
        self.root = root
        self.manager = OrderManager()
        self._anim_tick = 0
        self._card_imgs = {}  # 缓存 PIL 渲染的图表图片
        self._setup_root()
        self._build_layout()
        self.root.update_idletasks()
        self._anim_loop()
        self.load_sample()

    def _setup_root(self):
        self.root.title("Obsidian // Sales Command Dashboard")
        self.root.configure(bg=BG)
        self.root.minsize(1200, 700)
        sw, sh = self.root.winfo_screenwidth(), self.root.winfo_screenheight()
        w, h = 1500, 860
        self.root.geometry(f"{w}x{h}+{(sw-w)//2}+{(sh-h)//2}")

        # 粒子浮层
        self.overlay = ParticleOverlay(self.root, w, h)
        self.overlay.init_draw()
        self.root.bind("<Configure>", lambda e: self._on_root_move())
        self.root.bind("<Destroy>", lambda e: self._on_root_destroy())
        self.root.focus_force()

    def _build_layout(self):
        # ── 头部 ──
        self._build_header()

        # ── 主内容：左侧表格 + 右侧面板 ──
        body = tk.Frame(self.root, bg=BG)
        body.pack(fill="both", expand=True, padx=12, pady=(6,0))

        # Toolbar
        self._build_toolbar(body)

        # Content area
        content = tk.Frame(body, bg=BG)
        content.pack(fill="both", expand=True, pady=(8,0))

        # 左侧表格
        self._build_table(content)

        # 右侧面板
        self._build_right_panel(content)

        # ── 状态栏 ──
        self._build_statusbar()

    def _build_header(self):
        hdr = tk.Frame(self.root, bg=CARD, height=64,
                      highlightthickness=1, highlightbackground=BDR)
        hdr.pack(fill="x")
        hdr.pack_propagate(False)

        # Logo 图标 (Canvas绘制)
        logo = tk.Canvas(hdr, width=64, height=64, bg=CARD,
                        highlightthickness=0, bd=0)
        logo.pack(side="left", padx=(12,8))
        # 六边形
        cx,cy,r = 28,32,14
        pts = []
        for i in range(6):
            a = math.pi/3*i - math.pi/6
            pts.extend([cx+r*math.cos(a), cy+r*math.sin(a)])
        logo.create_polygon(pts, outline=BL30, fill="", width=2)
        logo.create_polygon(pts, outline=BL, fill=BL10, width=1.5)
        logo.create_text(cx, cy, text="S", fill=WT, font=("Segoe UI", 12, "bold"))

        # 标题
        tk.Label(hdr, text="Sales Command", font=("Segoe UI", 18, "bold"),
                bg=CARD, fg=WT).pack(side="left", pady=(8,0))
        tk.Label(hdr, text="Order Management System",
                font=("Segoe UI", 9), bg=CARD, fg=GR).pack(side="left", padx=(10,0), pady=(12,0))

        # 右侧状态指示
        sf = tk.Frame(hdr, bg=CARD)
        sf.pack(side="right", padx=16)
        self._led = tk.Canvas(sf, width=10, height=10, bg=CARD,
                             highlightthickness=0,bd=0)
        self._led.pack(side="left", padx=(0,6))
        self._led.create_oval(1,1,9,9,fill=GN,outline="")
        tk.Label(sf, text="ONLINE", font=("Segoe UI", 9, "bold"),
                bg=CARD, fg=GN).pack(side="left")
        tk.Label(sf, text="  |  v5.0", font=("Segoe UI", 9),
                bg=CARD, fg=GR).pack(side="left", padx=(6,0))

    def _build_toolbar(self, parent):
        # 仪表卡片区（单独一行，有足够高度）
        self._build_dash_cards(parent)

        # 工具栏行（搜索 + 按钮）
        tb = tk.Frame(parent, bg=BG, height=46)
        tb.pack(fill="x", pady=(8,0))
        tb.pack_propagate(False)

        # 搜索
        sf = tk.Frame(tb, bg=CARD, highlightthickness=1, highlightbackground=BDR)
        sf.pack(side="left")
        tk.Label(sf, text="\U0001F50D", font=("Segoe UI", 11),
                bg=CARD, fg=GR).pack(side="left", padx=(12,2), pady=8)
        self.search_var = tk.StringVar()
        tk.Entry(sf, textvariable=self.search_var, font=("Segoe UI", 10),
                width=30, bg="#0D1117", fg=WT, insertbackground=BL,
                relief="flat", bd=0, highlightthickness=1,
                highlightbackground=BDR, highlightcolor=BL).pack(
            side="left", padx=(0,12), pady=8)
        self.search_var.trace_add("write", self._on_search)

        # 操作按钮
        bf = tk.Frame(tb, bg=BG)
        bf.pack(side="right")
        btns = [
            ("+ Add", GN, self.add_order),
            ("\u2212 Del", RD, self.delete_order),
            ("Report", BL, self.show_report),
            ("Save", AM, self.save_csv),
            ("Load", PU, self.load_csv),
            ("Sample", CY, self.load_sample),
        ]
        for txt, col, cmd in btns:
            btn = tk.Label(bf, text=txt, font=("Segoe UI", 9, "bold"),
                          bg=BG, fg=col, cursor="hand2",
                          padx=10, pady=4)
            btn.pack(side="left", padx=2)
            btn.bind("<Enter>", lambda e,b=btn,c=col: b.config(bg=_m(c,0.15), fg="#FFFFFF"))
            btn.bind("<Leave>", lambda e,b=btn,c=col: b.config(bg=BG, fg=c))
            btn.bind("<Button-1>", lambda e,c=cmd: c())

    def _build_dash_cards(self, parent):
        """顶部统计卡片区"""
        cards_frame = tk.Frame(parent, bg=BG)
        cards_frame.pack(fill="x", pady=(0, 10))

        self._card_labels = {}
        for key, title, color in [
            ("cnt", "Orders", BL),
            ("rev", "Revenue", GN),
            ("avg", "Avg Order", AM),
            ("top", "Top Product", PU),
        ]:
            cf = tk.Frame(cards_frame, bg=CARD, highlightthickness=1, highlightbackground=BDR, width=120, height=68)
            cf.pack(side="left", padx=3)
            cf.pack_propagate(False)
            tk.Label(cf, text=title, font=("Segoe UI", 9),
                    bg=CARD, fg=GR).pack(padx=10, pady=(10, 4))
            lbl = tk.Label(cf, text="--", font=("Segoe UI", 20, "bold"),
                          bg=CARD, fg=color)
            lbl.pack(fill="x", padx=10, pady=(0, 6))
            self._card_labels[key] = lbl

    def _build_table(self, parent):
        tf = tk.Frame(parent, bg=BG)
        tf.pack(side="left", fill="both", expand=True)

        columns = [
            ("order_id", "Order ID", 140, "w"),
            ("date", "Date", 105, "center"),
            ("product", "Product", 130, "w"),
            ("quantity", "Qty", 55, "center"),
            ("unit_price", "Unit Price", 110, "e"),
            ("total", "Total", 120, "e"),
            ("salesperson", "Agent", 145, "w"),
        ]
        self.table = CanvasTable(tf, columns)
        self.table.pack(fill="both", expand=True)
        self.table.bind_dbl_click(lambda d: self._detail(d))

    def _build_right_panel(self, parent):
        rp = tk.Frame(parent, bg=BG, width=310)
        rp.pack(side="right", fill="y", padx=(8,0))
        rp.pack_propagate(False)

        # 产品排行
        pf = tk.Frame(rp, bg=CARD, highlightthickness=1, highlightbackground=BDR)
        pf.pack(fill="x", pady=(0,6))
        tk.Label(pf, text="  Top Products", font=("Segoe UI", 10, "bold"),
                bg=CARD, fg=WT).pack(anchor="w", padx=10, pady=(8,2))
        self._product_img_lbl = tk.Label(pf, bg=CARD)
        self._product_img_lbl.pack(padx=6, pady=(0,8))

        # 销售员排行
        sf = tk.Frame(rp, bg=CARD, highlightthickness=1, highlightbackground=BDR)
        sf.pack(fill="x", pady=(0,6))
        tk.Label(sf, text="  Top Agents", font=("Segoe UI", 10, "bold"),
                bg=CARD, fg=WT).pack(anchor="w", padx=10, pady=(8,2))
        self._agent_img_lbl = tk.Label(sf, bg=CARD)
        self._agent_img_lbl.pack(padx=6, pady=(0,8))

        # 月度趋势
        mf = tk.Frame(rp, bg=CARD, highlightthickness=1, highlightbackground=BDR)
        mf.pack(fill="x", pady=(0,6))
        tk.Label(mf, text="  Monthly Revenue", font=("Segoe UI", 10, "bold"),
                bg=CARD, fg=WT).pack(anchor="w", padx=10, pady=(8,2))
        self._month_img_lbl = tk.Label(mf, bg=CARD)
        self._month_img_lbl.pack(padx=6, pady=(0,8))

        # 汇总
        af = tk.Frame(rp, bg=CARD, highlightthickness=1, highlightbackground=BDR)
        af.pack(fill="both", expand=True)
        tk.Label(af, text="  Summary", font=("Segoe UI", 10, "bold"),
                bg=CARD, fg=WT).pack(anchor="w", padx=10, pady=(8,2))
        self._summary_text = tk.Text(af, font=("Consolas", 8),
                                     bg="#0D1117", fg=WT2, relief="flat",
                                     padx=8, pady=6, wrap="word",
                                     state="disabled", height=6,
                                     borderwidth=0, highlightthickness=1,
                                     highlightbackground=BDR)
        self._summary_text.pack(fill="both", expand=True, padx=6, pady=(0,8))

    def _build_statusbar(self):
        sb = tk.Frame(self.root, bg=CARD, height=26,
                     highlightthickness=1, highlightbackground=BDR)
        sb.pack(fill="x", side="bottom")
        sb.pack_propagate(False)
        self._status_lbl = tk.Label(sb, text="Ready", font=("Segoe UI", 8),
                                   bg=CARD, fg=GR)
        self._status_lbl.pack(side="left", padx=14, pady=2)
        tk.Label(sb, text="\u25CF Live", font=("Segoe UI", 8, "bold"),
                bg=CARD, fg=GN).pack(side="right", padx=14, pady=2)

    def _anim_loop(self):
        self._anim_tick += 1
        # LED 闪烁
        if self._anim_tick % 60 == 0:
            c = GN if self._anim_tick % 120 < 60 else "#1A3320"
            self._led.delete("all"); self._led.create_oval(1,1,9,9,fill=c,outline="")
        # 粒子浮层
        if hasattr(self, 'overlay') and self.overlay.winfo_exists():
            try: self.overlay.tick()
            except: pass
        self.root.after(30, self._anim_loop)

    # ────────── 数据刷新 ──────────
    def _refresh(self, orders=None):
        if orders is None:
            orders = self.manager.get_all_orders()

        # 表格
        table_data = []
        for o in orders:
            em = SALESPERSON_AVATAR.get(o.salesperson, "")
            table_data.append({
                "order_id": o.order_id,
                "date": o.date,
                "product": o.product,
                "quantity": str(o.quantity),
                "unit_price": f"\xa5{o.unit_price:,.0f}",
                "total": f"\xa5{o.total_amount():,.0f}",
                "salesperson": f"{em} {o.salesperson}",
            })
        self.table.set_data(table_data)

        # 仪表卡片
        cnt = len(self.manager.get_all_orders())
        tot = self.manager.total_revenue()
        avg = self.manager.average_order_value()
        tp = self.manager.top_products(1)
        top_name = tp[0][0] if tp else "--"
        self._card_labels["cnt"].config(text=str(cnt))
        self._card_labels["rev"].config(text=f"\xa5{tot:,.0f}")
        self._card_labels["avg"].config(text=f"\xa5{avg:,.0f}")
        self._card_labels["top"].config(text=top_name[:12] if len(top_name)>12 else top_name)

        # 右侧图表（PIL 渲染）
        prod_data = self.manager.top_products(5)
        agent_data = self.manager.top_salespersons(5)
        month_data = sorted(self.manager.monthly_revenue().items(), key=lambda x: x[0])

        w_bar = 290
        if prod_data:
            img = render_hbar_chart(prod_data, [BL,GN,AM,PU,CY], w_bar, 140)
            self._product_img_lbl.config(image=img)
            self._product_img_lbl.image = img
        if agent_data:
            img = render_hbar_chart(agent_data, [PU,AM,BL,CY,GN], w_bar, 140)
            self._agent_img_lbl.config(image=img)
            self._agent_img_lbl.image = img
        if month_data:
            img = render_area_chart(month_data, BL, w_bar, 130)
            self._month_img_lbl.config(image=img)
            self._month_img_lbl.image = img

        # 汇总
        tp_all = self.manager.top_products(999)
        ts_all = self.manager.top_salespersons(999)
        lines = [
            f"Total Orders:    {cnt}",
            f"Total Revenue:   \xa5{tot:,.0f}",
            f"Avg Order Value: \xa5{avg:,.0f}",
            f"Top Product:     {tp_all[0][0] if tp_all else 'N/A'}",
            f"Top Agent:       {ts_all[0][0] if ts_all else 'N/A'}",
            f"Updated:         {_time.strftime('%H:%M:%S')}",
        ]
        self._summary_text.config(state="normal")
        self._summary_text.delete("1.0", "end")
        self._summary_text.insert("1.0", "\n".join(lines))
        self._summary_text.config(state="disabled")

        # 状态栏
        self._status_lbl.config(
            text=f"Records: {cnt}  |  Revenue: \xa5{tot:,.0f}  |  {_time.strftime('%H:%M:%S')}"
        )

    def _on_search(self, *a):
        kw = self.search_var.get().strip()
        r = self.manager.search_orders(kw) if kw else self.manager.get_all_orders()
        self._refresh(r)

    # ────────── 详情弹窗 ──────────
    def _detail(self, row_data):
        oid = row_data.get("order_id","")
        o = self.manager.find_by_id(oid)
        if not o: return

        w,h = 400, 320
        win = tk.Toplevel(self.root)
        win.title(""); win.resizable(False,False)
        win.configure(bg=BG); win.overrideredirect(True)
        px = self.root.winfo_rootx()+(self.root.winfo_width()-w)//2
        py = self.root.winfo_rooty()+(self.root.winfo_height()-h)//2
        win.geometry(f"{w}x{h}+{px}+{py}")

        # PIL 玻璃态背景
        bg = render_glass_card(w, h, BL, 12)
        tk.Label(win, image=bg, bg=BG).place(x=0,y=0)

        tk.Label(win, text="Order Detail", font=("Segoe UI", 15, "bold"),
                bg=CARD, fg=WT).place(x=28, y=18)
        cb = tk.Label(win, text="\u2715", font=("Segoe UI", 16),
                     bg=CARD, fg=GR, cursor="hand2")
        cb.place(x=w-35, y=12)
        cb.bind("<Enter>", lambda e: cb.config(fg=RD))
        cb.bind("<Leave>", lambda e: cb.config(fg=GR))
        cb.bind("<Button-1>", lambda e: win.destroy())

        info = [
            f"Order ID:     {o.order_id}",
            f"Date:         {o.date}",
            f"Product:      {o.product}",
            f"Quantity:     {o.quantity}",
            f"Unit Price:   \xa5{o.unit_price:,.2f}",
            f"Total:        \xa5{o.total_amount():,.2f}",
            f"Agent:        {o.salesperson}",
        ]
        for i,line in enumerate(info):
            tk.Label(win, text=line, font=("Consolas", 10),
                    bg=CARD, fg=WT2).place(x=40, y=58+i*26)

        # 关闭按钮
        cls = tk.Label(win, text="  Close  ", font=("Segoe UI", 10, "bold"),
                      bg=BL, fg="#0D1117", cursor="hand2", padx=20, pady=6)
        cls.place(x=w-120, y=h-48)
        cls.bind("<Button-1>", lambda e: win.destroy())
        cls.bind("<Enter>", lambda e: cls.config(bg="#79C0FF"))
        cls.bind("<Leave>", lambda e: cls.config(bg=BL))
        win._bg_img = bg

    # ────────── 操作 ──────────
    def add_order(self):
        AddOrderDialog(self.root, lambda o: (self.manager.add_order(o), self._refresh()))
    def delete_order(self):
        sel = self.table.selected_data()
        if not sel:
            messagebox.showwarning("Warning", "Select an order first.")
            return
        oid = sel["order_id"]
        if messagebox.askyesno("Confirm", f"Delete {oid}?"):
            try:
                self.manager.remove_order(oid)
                self._refresh()
            except ValueError as e:
                messagebox.showerror("Error", str(e))

    def show_report(self):
        if not self.manager.get_all_orders():
            messagebox.showwarning("Warning", "No orders to report.")
            return
        rpt = self.manager.summary_report()
        w,h = 680, 680
        win = tk.Toplevel(self.root)
        win.title(""); win.configure(bg=BG); win.overrideredirect(True)
        px = self.root.winfo_rootx()+(self.root.winfo_width()-w)//2
        py = self.root.winfo_rooty()+(self.root.winfo_height()-h)//2
        win.geometry(f"{w}x{h}+{px}+{py}")

        bg = render_glass_card(w, h, CY, 12)
        tk.Label(win, image=bg, bg=BG).place(x=0,y=0)

        tk.Label(win, text="Summary Report", font=("Segoe UI", 16, "bold"),
                bg=CARD, fg=CY).place(x=30, y=18)
        cb = tk.Label(win, text="\u2715", font=("Segoe UI", 16),
                     bg=CARD, fg=GR, cursor="hand2")
        cb.place(x=w-35, y=12)
        cb.bind("<Enter>", lambda e: cb.config(fg=RD))
        cb.bind("<Leave>", lambda e: cb.config(fg=GR))
        cb.bind("<Button-1>", lambda e: win.destroy())

        tx = tk.Text(win, font=("Consolas", 9), bg="#0D1117", fg=WT2,
                    relief="flat", padx=16, pady=12, wrap="none",
                    borderwidth=0, insertbackground=BL,
                    highlightthickness=1, highlightbackground=BDR)
        tx.insert("1.0", rpt); tx.config(state="disabled")
        tx.place(x=20, y=50, width=w-40, height=h-120)

        cls = tk.Label(win, text="  Close  ", font=("Segoe UI", 11, "bold"),
                      bg=CY, fg="#0D1117", cursor="hand2", padx=24, pady=8)
        cls.place(x=w//2-50, y=h-56)
        cls.bind("<Button-1>", lambda e: win.destroy())
        win._bg_img = bg

    def save_csv(self):
        orders = self.manager.get_all_orders()
        if not orders:
            messagebox.showwarning("Warning", "No data to save."); return
        data_handler.save_to_csv(orders, "data111.csv")
        messagebox.showinfo("Saved", "Data saved to data111.csv")

    def load_csv(self):
        orders = data_handler.load_from_csv("data111.csv")
        if orders:
            self.manager.set_orders(orders, True); self._refresh()
            messagebox.showinfo("Loaded", f"{len(orders)} orders loaded.")
        else:
            self.manager.set_orders([], True); self._refresh()
            messagebox.showwarning("Warning", "No valid data found.")

    def load_sample(self):
        self.manager.set_orders(data_handler.generate_sample_data(), True)
        self._refresh()

    def _on_root_move(self):
        if hasattr(self,"overlay") and self.overlay.winfo_exists():
            try: self.overlay._sync()
            except: pass

    def _on_root_destroy(self):
        if hasattr(self,"overlay"):
            try: self.overlay.destroy()
            except: pass


if __name__ == "__main__":
    root = tk.Tk()
    app = SalesApp(root)
    root.mainloop()
