#!/usr/bin/env python3
"""
Render POSEIDON C5 Tier-2 screens from the firmware's actual draw logic.

NOT a device capture and NOT an emulator: this reproduces the exact draw
calls in src/features/c5_tier2.cpp + src/ui.cpp at the real 240x135
resolution, using the Deepwater theme's RGB565 palette (src/theme.cpp).
Output is labelled "rendered from code" in the README.

Native panel: 240x135, status bar 12px (STATUS_H), footer 10px (FOOTER_H),
body between. GFX text size 1 = 6px advance x 8px cell. Scaled 4x.
"""
from PIL import Image, ImageDraw, ImageFont

S = 4                       # upscale factor
W, H = 240, 135
STATUS_H, FOOTER_H = 12, 10
BODY_Y = STATUS_H
CW, CH = 6, 8               # GFX size-1 char cell

def c565(v):                # RGB565 -> RGB888
    r = (v >> 11) & 0x1F; g = (v >> 5) & 0x3F; b = v & 0x1F
    return ((r*255)//31, (g*255)//63, (b*255)//31)

# Deepwater ("POSEIDON") palette — src/theme.cpp
BG      = c565(0x0864); FG     = c565(0xEFFF); ACCENT = c565(0x4E9F)
ACCENT2 = c565(0xAC5C); WARN   = c565(0xFE68); BAD    = c565(0xF98C)
GOOD    = c565(0x4ED6); DIM    = c565(0x9D36); SEL_BG = c565(0x1147)
STATUS_BG = c565(0x10A6); RULE = c565(0x2969)

FONT = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", CH*S)  # ~fits the cell

class Screen:
    def __init__(self):
        self.img = Image.new("RGB", (W*S, H*S), BG)
        self.d = ImageDraw.Draw(self.img)
    def rect(self, x, y, w, h, col):
        self.d.rectangle([x*S, y*S, (x+w)*S-1, (y+h)*S-1], fill=col)
    def hline(self, x, y, w, col):
        self.d.rectangle([x*S, y*S, (x+w)*S-1, y*S+max(1,S//2)], fill=col)
    def vline(self, x, y, h, col):
        self.d.rectangle([x*S, y*S, x*S+max(1,S//2), (y+h)*S-1], fill=col)
    def text(self, x, y, s, col):
        # GFX draws from top-left of the 6x8 cell; nudge for the TTF ascent.
        self.d.text((x*S, y*S - S), s, font=FONT, fill=col)
    def frame_outline(self, x, y, w, h, col):
        self.d.rectangle([x*S, y*S, (x+w)*S-1, (y+h)*S-1], outline=col, width=max(1,S//2))

def status_bar(sc, radio="WiFi", c5=1, bat=82, armed=True):
    sc.rect(0, 0, W, STATUS_H, STATUS_BG)
    # signal glyph (src/ui.cpp)
    sc.vline(7, 2, 7, ACCENT); sc.vline(4, 2, 3, ACCENT); sc.vline(10, 2, 3, ACCENT); sc.hline(4, 5, 7, ACCENT)
    sc.text(18, 2, radio, FG)
    # TX-arm "ARMED" badge (white on red) in the extra slot when armed (src/ui.cpp)
    if armed:
        sc.rect(106, 1, 44, 9, BAD)
        sc.text(113, 2, "ARMED", FG)
    if c5 > 0: sc.text(150, 2, f"C5:{c5}", GOOD)
    # battery
    bx = W - 16
    sc.frame_outline(bx, 2, 13, 7, DIM); sc.vline(bx+13, 4, 3, DIM)
    fw = (bat*11)//100
    bcol = GOOD if bat > 50 else (WARN if bat > 20 else BAD)
    if fw > 0: sc.rect(bx+1, 3, fw, 5, bcol)
    sc.text(196, 2, f"{bat}%", bcol)
    # TX-arm baseline: 2px red when armed, else the theme rule (src/ui.cpp)
    if armed:
        sc.hline(0, STATUS_H-2, W, BAD); sc.hline(0, STATUS_H-1, W, BAD)
    else:
        sc.hline(0, STATUS_H-1, W, RULE)

def footer(sc, hint):
    sc.rect(0, H-FOOTER_H, W, FOOTER_H, STATUS_BG)
    sc.text(4, H-FOOTER_H+1, hint, DIM)

def picker(title, ch_label, opt_label, opt_col):
    """src/features/c5_tier2.cpp c5_tx_picker()"""
    sc = Screen(); status_bar(sc)
    sc.text(4,  BODY_Y+2,  title, ACCENT2)
    sc.hline(4, BODY_Y+12, W-8, ACCENT2)
    sc.text(6,  BODY_Y+24, f"channel : {ch_label}", FG)
    sc.text(6,  BODY_Y+40, f"mode    : {opt_label}", opt_col)
    sc.text(6,  BODY_Y+58, ";/. change channel   m change mode", DIM)
    footer(sc, ";/. chan  m=opt  ENTER=fire  `=back")
    return sc.img

def apclone_list():
    """src/features/c5_tier2.cpp feat_c5_apclone() pick loop"""
    sc = Screen(); status_bar(sc)
    sc.text(4, BODY_Y+2, "PICK AP TO CLONE", ACCENT2)
    sc.hline(4, BODY_Y+12, W-8, ACCENT2)
    aps = [(-38,"6","HomeNet-5G"), (-51,"11","CoffeeShop_Guest"), (-55,"1","<hidden>"),
           (-63,"36","Office-WiFi"), (-70,"6","printer-direct"), (-74,"149","NETGEAR-5G"),
           (-79,"11","xfinitywifi")]
    cursor = 1
    for r,(rssi,ch,ssid) in enumerate(aps):
        y = BODY_Y + 18 + r*12
        sel = (r == cursor)
        if sel: sc.rect(0, y-1, W, 12, SEL_BG)
        sc.text(2, y, f"{rssi:>4} ch{ch:<3} {ssid[:18]}", ACCENT if sel else FG)
    footer(sc, ";/. pick  ENTER=clone  `=back")
    return sc.img

def save(img, name, out):
    import os; os.makedirs(out, exist_ok=True)
    p = f"{out}/{name}.png"; img.save(p); print("wrote", p, img.size)

import os
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "screens")
save(picker("BEACON SPAM", "6  (2.4GHz)", "meme list", WARN), "c5-beacon-spam", OUT)
save(picker("CIW SSID-INJECT", "36  (5GHz)", "log4j", WARN), "c5-ciw", OUT)
save(apclone_list(), "c5-ap-clone", OUT)
print("done")
