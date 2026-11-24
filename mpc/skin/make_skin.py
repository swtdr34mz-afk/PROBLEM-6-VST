#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 swtdr34mz (PROPHET 67)
"""The MPC skin (Plugin Skins/: TUI.json, Q-Links.json and the pictures) from the renderer's output.

    python3 mpc/skin/make_skin.py <render folder> mpc/plugin/skins

The renderer (render_skin.cpp) draws each MPC page with the VST3's own controls and sections and cuts every control
out at every value; this places each picture where it was cut from, bound to the MPC plugin's parameter, so every
control works as on the VST:

  knobs         film strips (a frame a value), dragged; a double tap opens the MPC's editor (its list for a choice)
  buttons       a picture for each run of values that looks and acts alike, shown while the value is in that run; a
                tap gives the value the VST's own rule gives (a switch, POLY MOD's FREQ 1 / SHAPE 1 / PW 1 bits,
                OSC 2's SYNC / LOW FREQ / KEYBOARD, the KEYBOARD and FILTER steps, VELOCITY off and back at 100)
  LED lists     each row a radio button
  EFFECTS       SELECT and its LEDs choose the effect shown (a value of the screen's own), with its displays

The pages (the function keys): QUICK, OSC, FLT/ENV, LFO, FX. On each the 16 Q-Links turn four groups, the top row
of Q-Links the first group and so on down, each group framed on the screen. Standard library only.
"""
import json
import os
import shutil
import sys

# The MPC plugin's parameters (mpc/plugin/src/Prophet67Mpc.cpp): the VST3's ids to the MPC's indices
MPC = {"volume": 0}
MPC.update({"p%d" % i: 1 + i for i in range(74)})
for i, k in enumerate(["hp_vel", "hp_keytrk", "fenv_vel", "arp_on", "arp_mode", "arp_rate", "arp_oct", "arp_gate",
                       "arp_hold", "swing", "quality", "split", "split_point", "scale_l", "scale_r", "key", "follow",
                       "low_hold", "balance"]):
    MPC[k] = 75 + i
NPARAMS = 94

FXH = "Local/5/fx"                       # the effect EFFECTS shows (the screen's own value)

# The pages, as the renderer names them: (the function key's name, the Q-Links' groups top down; a group in brackets
# of its own, ("frame", False), is not framed: its controls are not side by side)
PAGES = [
    ("QUICK", [["p12", "p13", "p14", "p15"],                     # CUTOFF, RESONANCE, ENV AMOUNT, HP CUTOFF (not on the page)
               ["p8", "p9", "p3", "p10"],                        # MIXER: OSC 1, OSC 2, SUB OCTAVE, NOISE
               (["p52", "p53", "p41", "p69"], False),            # VINTAGE, VOICE SPREAD, REVERB, DELAY
               (["p36", "p39", "arp_on", "quality"], False)]),   # CHORUS, DISTORT, ARP ON/OFF, QUALITY
    ("OSC", [["p0", "p11#0", "p1", "p2"],                        # OSCILLATOR 1 (SYNC: OSC 2's mode, its first button)
             ["p4", "p5", "p6", "p7"],                           # OSCILLATOR 2
             ["p8", "p9", "p3", "p10"]]),                        # MIXER
    ("FLT/ENV", [["p15", "p16", "p19", "hp_keytrk"],             # HIGH-PASS
                 ["p20", "p21", "p22", "p23"],                   # FILTER ENVELOPE
                 ["p12", "p13", "p14", "p17"],                   # LOW-PASS
                 ["p24", "p25", "p26", "p27"]]),                 # AMPLIFIER ENVELOPE
    ("LFO", [["p32", "p33", "p34", "p35"],                       # POLY MOD
             ["p28", "p58", "p30", "p29"],                       # LFO 1
             ["p54", "p59", "p56", "p55"],                       # LFO 2
             (["p31", "p57"], False)]),                          # their DESTs
    ("FX", [(["p36", "p37", "p38"], False),                      # CHORUS   (the knobs shown follow SELECT,
            (["p40", "p41", "p42", "p43"], False),               # REVERB    so no frames)
            (["p66", "p67", "p68", "p69"], False),               # DELAY
            (["p70", "p71", "p72", "p73"], False)]),             # DLY MOD (and DELAY's SYNC)
]
# Program (Track) mode: the knobs as the MPC's own program knobs, bottom left Q-Link 1
PROGRAM_QLINKS = ["p51", "p29", "p39", "volume",       # 1..4   (bottom row)
                  "p24", "p25", "p26", "p27",          # 5..8   AMP ENVELOPE
                  "p20", "p21", "p22", "p23",          # 9..12  FILTER ENVELOPE
                  "p12", "p13", "p14", "p15"]          # 13..16 CUTOFF, RESONANCE, ENV AMOUNT, HP CUTOFF


class O(list):
    """A JSON object kept in order, with repeated keys (the overlays' two "version"s)."""
    def __init__(self, *pairs):
        super().__init__(pairs)


def dump(v, ind=0):
    sp = "  " * ind
    if isinstance(v, O):
        if not v:
            return "{}"
        return "{\n" + ",\n".join("%s  %s: %s" % (sp, json.dumps(k), dump(x, ind + 1)) for k, x in v) + "\n" + sp + "}"
    if isinstance(v, list):
        if not v:
            return "[]"
        if all(not isinstance(x, (O, list)) for x in v):
            return "[" + ", ".join(json.dumps(x) for x in v) + "]"
        return "[\n" + ",\n".join(sp + "  " + dump(x, ind + 1) for x in v) + "\n" + sp + "]"
    return json.dumps(v)


def box(r):
    return "%d %d %d %d" % tuple(r)


def image_data(img):
    return O(("version", 2), ("imageType", "Regular"), ("colour", "0"), ("image", img))


def bounds(r, focus="No", hide_handles=None, show="Show"):
    if hide_handles:
        return O(("version", 2), ("acceptsHWFocus", focus), ("showWhenDataModelInvalid", show), ("whenVisible", "Always"),
                 ("boundsType", "Absolute"), ("bounds", box(r)), ("additionalInvalidatingHandles", hide_handles))
    return O(("version", 1), ("acceptsHWFocus", focus), ("showWhenDataModelInvalid", show), ("whenVisible", "Always"),
             ("boundsType", "Absolute"), ("bounds", box(r)))


def entry(name, typ, data, b, remap=None):
    return O(("version", 2),
             ("componentData", O(("version", 1), ("name", name), ("type", typ), ("data", data))),
             ("handle remapping", O(("version", 1), ("map", [O(("key", k), ("value", v)) for k, v in (remap or [])]))),
             ("bounds", b))


def action(on, handler, handle, extra):
    return O(("version", 1), ("onAction", on), ("handler", handler), ("handleName", handle), ("additionalData", extra))


def component(actions, children, ignore_mouse=False):
    clear = O(("version", 1), ("colour", "0"), ("image", ""))
    return O(("version", 2), ("actions", actions),
             ("backgroundData", O(("version", 1), ("focussed", clear), ("unfocussed", clear))),
             ("ignoreMousePresses", ignore_mouse), ("disableCoarseDataWheel", False), ("componentsData", children))


FOCUS_LOST = O(("version", 1), ("onAction", "Focus Lost"), ("handler", "Focus Lost"), ("handleName", "Focus Lost"),
               ("additionalData", ""))


def overlay(typ, data):
    return component([], [entry("overlay", typ, data, bounds((0, 0, 1280, 800), "Yes"))], ignore_mouse=True)


def handle(pid):
    return "Parameter %d" % MPC[pid]


def range_handle(a, b, steps, h):
    return ("IndexedEnabling/%d/%d/%s" % (a, steps, h)) if a == b else ("RangedEnabling/%d/%d/%d/%s" % (a, b, steps, h))


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    src, out = argv[1], argv[2]
    m = json.load(open(os.path.join(src, "manifest.json")))
    pages = {p["name"]: p for p in m["pages"]}
    for p in m["pages"]:
        for c in p["controls"]:
            if c["id"] not in MPC:
                sys.exit("%s: the page's %s is not one of the MPC plugin's parameters" % (p["name"], c["id"]))

    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(out)
    used = set()

    def use(f):
        used.add(f)
        return f

    def knob_overlay(bipolar):
        return overlay("KnobOverlay", O(("version", 2), ("version", 1), ("actions", [FOCUS_LOST]), ("overlayType", "Knob With Header"),
                                        ("scaleType", "Linear"), ("polarityType", "Bipolar" if bipolar else "Unipolar"),
                                        ("showMinusSign", True), ("handleName", "Data")))
    defs = [("knob overlay", knob_overlay(False)), ("knob overlay bipolar", knob_overlay(True)),
            ("menu overlay", overlay("MenuOverlay", O(("version", 1), ("version", 1), ("actions", [FOCUS_LOST]),
                                                      ("withNumbers", False), ("handleName", "Data"))))]
    focus = O(("version", 1), ("backgroundColour", "19ffffff"), ("outlineColour", "ffe81f40"), ("backgroundInset", 1.0),
              ("outlineThickness", 1.0))
    names = {}                           # the discrete parameters' names (replacementStrings)

    def tab_children(page):
        kids = [entry("panel", "Image", image_data(use(page["background"])), bounds((0, 0, 1280, 628)))]
        boxes, legends = [], []          # (parameter id, rect) for the Q-Link frames; a knob's rect with its legend

        def fx_hide(c):
            return ["IndexedEnabling/%d/5/%s" % (c["fx"], FXH)] if c["fx"] >= 0 else []

        for i, c in enumerate(page["controls"]):
            if c["names"]:
                names[c["id"]] = c["names"]
            h = handle(c["id"])
            if c["kind"] == "knob":
                # the knob's slot: the knob and its legend under it (its legend touched turns it too); the film strip
                # at the knob's place in it (an effect's legend is a picture of its own: the slot is the knob)
                fr = c["frame"] if not c.get("legendImage") else c["rect"]
                kx, ky, w, hh = c["rect"]
                ox, oy = kx - fr[0], ky - fr[1]
                ov = "menu overlay" if c["names"] else "knob overlay bipolar" if c["bipolar"] else "knob overlay"
                knob = O(("version", 3), ("knobType", "FilmStrip"), ("filmStrip", use(c["strip"])), ("numFrames", c["frames"]),
                         ("invert", False), ("dragOrientation", "Vertical"), ("handleName", "Data"))
                name = "%s knob %d %s" % (page["name"].lower(), i, c["id"])
                defs.append((name, component(
                    [action("Mouse Down", "Q-Link", "Data", ""), action("Double Click", "Show Overlay", "Data", ov),
                     action("Enter Pressed", "Show Overlay", "Data", ov)],
                    [entry("knob", "Knob", knob, bounds((ox, oy, w, hh))),
                     entry("focus", "Focus", focus, O(("version", 1), ("acceptsHWFocus", "No"), ("showWhenDataModelInvalid", "Show"),
                                                      ("whenVisible", "WhenFocussed"), ("boundsType", "Absolute"),
                                                      ("bounds", box((0, 0, fr[2], fr[3])))))])))
                kids.append(entry(c["id"], name, O(("version", 1), ("handleName", "Data")),
                                  bounds(fr, "Yes", fx_hide(c) or None, "Hide"), [("Data", h)]))
                boxes.append((c["id"], fr))
                legends.append((c["id"], c["frame"]))
                if c.get("legendImage"):     # (an effect's knob: its legend with it)
                    kids.append(entry(c["id"] + " legend", "Image", image_data(use(c["legendImage"])),
                                      bounds(c["legendRect"], "No", fx_hide(c) or None)))
            elif c["kind"] == "button":
                rs = c["ranges"]
                for r in rs:
                    hide = fx_hide(c) + ([range_handle(r["a"], r["b"], c["steps"], h)] if len(rs) > 1 else [])
                    data = O(("version", 2), ("onImage", use(r["image"])), ("offImage", r["image"]), ("buttonId", r["next"]),
                             ("numButtonsInGroup", c["steps"]), ("handleName", h), ("gestureBehaviour", "Instant"))
                    kids.append(entry(c["id"], "Button", data, bounds(c["rect"], "No", hide or None, "Hide")))
                boxes.append((c["id"], c["rect"]))
            else:
                if len(c["rows"]) != c["steps"]:
                    sys.exit("%s: %d LEDs for %d values" % (c["id"], len(c["rows"]), c["steps"]))
                for k, row in enumerate(c["rows"]):
                    data = O(("version", 2), ("onImage", use(row["on"])), ("offImage", use(row["off"])), ("buttonId", k),
                             ("numButtonsInGroup", c["steps"]), ("handleName", h), ("gestureBehaviour", "Instant"))
                    kids.append(entry(c["id"], "Button", data, bounds(row["rect"], "No", fx_hide(c) or None, "Hide")))
                    boxes.append((c["id"], row["rect"]))

        fx = page.get("effects")
        if fx:                           # EFFECTS: SELECT (the next effect), its LEDs, the effect's name and first value
            for j in range(5):
                data = O(("version", 2), ("onImage", use(fx["selectImage"])), ("offImage", fx["selectImage"]),
                         ("buttonId", (j + 1) % 5), ("numButtonsInGroup", 5), ("handleName", FXH), ("gestureBehaviour", "Instant"))
                kids.append(entry("fx select", "Button", data, bounds(fx["select"], "No", ["IndexedEnabling/%d/5/%s" % (j, FXH)])))
            for k, row in enumerate(fx["rows"]):
                data = O(("version", 2), ("onImage", use(row["on"])), ("offImage", use(row["off"])), ("buttonId", k),
                         ("numButtonsInGroup", 5), ("handleName", FXH), ("gestureBehaviour", "Instant"))
                kids.append(entry("fx led", "Button", data, bounds(row["rect"])))
            code = O(("version", 2), ("imageType", "FilmStrip"), ("colour", "0"), ("image", use(fx["codeStrip"])),
                     ("numFrames", 5), ("handleName", FXH))
            kids.append(entry("fx name", "Image", code, bounds(fx["code"])))
            for f, v in enumerate(fx["values"]):
                strip = O(("version", 2), ("imageType", "FilmStrip"), ("colour", "0"), ("image", use(v["strip"])),
                          ("numFrames", v["steps"]), ("handleName", handle(v["id"])))
                kids.append(entry("fx value", "Image", strip, bounds(fx["value"], "No", ["IndexedEnabling/%d/5/%s" % (f, FXH)])))
            boxes += [("", fx["select"]), ("", fx["code"]), ("", fx["value"])] + [("", r["rect"]) for r in fx["rows"]]
        return kids, boxes, legends

    def frame(group, boxes, legends):
        """The Q-Link group's frame: around its controls with their legends (and any control it would otherwise cut
        through), with an even gap that touches no other control."""
        mine = []
        for g in group:                  # ("id#k": only the k-th of its controls from the left)
            pid, _, k = g.partition("#")
            xs = sorted({r[0] for p, r in boxes if p == pid})
            mine += [r for p, r in boxes + legends if p == pid and (not k or r[0] == xs[int(k)])]
        if not mine:
            return None
        x0, y0 = min(r[0] for r in mine), min(r[1] for r in mine)
        x1, y1 = max(r[0] + r[2] for r in mine), max(r[1] + r[3] for r in mine)

        def cuts(a, b, c, d):
            for _pid, r in boxes:
                ix = min(c, r[0] + r[2]) - max(a, r[0])
                iy = min(d, r[1] + r[3]) - max(b, r[1])
                inside = r[0] >= a and r[1] >= b and r[0] + r[2] <= c and r[1] + r[3] <= d
                if ix > 0 and iy > 0 and not inside:
                    return r
            return None
        while True:                      # take in what it cuts through
            r = cuts(x0, y0, x1, y1)
            if r is None:
                break
            x0, y0, x1, y1 = min(x0, r[0]), min(y0, r[1]), max(x1, r[0] + r[2]), max(y1, r[1] + r[3])
        for pad in (6, 5, 4, 3, 2, 1, 0):
            a, b, c, d = max(0, x0 - pad), max(0, y0 - pad), min(1280, x1 + pad), min(628, y1 + pad)
            if cuts(a, b, c, d) is None:
                return (a, b, c - a, d - b)
        return (x0, y0, x1 - x0, y1 - y0)

    tabs, qmaps = [], []
    for key, (name, groups) in enumerate(PAGES):
        page = pages[name]
        kids, boxes, legends = tab_children(page)
        rects = []
        q = {}
        for row, g in enumerate(groups):
            ids, framed = (g, True) if isinstance(g, list) else g
            if framed:
                r = frame(ids, boxes, legends)
                if r:
                    rects.append(r)
            for col, pid in enumerate(ids):
                q[13 + col - 4 * row] = MPC[pid.partition("#")[0]]
        tabs.append(O(("version", 1), ("tabName", name), ("fnKeyIndex", key), ("fnKeySubIndex", 0),
                      ("componentDefinition", component([], kids)),
                      ("qlinkBoundsData", [box(r) for r in rects])))
        qmaps.append(O(("Tab", key + 1), ("SubTab", 1), ("Bank Direction", "RowTopDown"),
                       ("Q-Links", O(*[("Q-Link %d" % n, q.get(n, -1)) for n in range(1, 17)]))))

    repl = [O(("key", handle(pid)), ("value", v)) for pid, v in sorted(names.items(), key=lambda kv: MPC[kv[0]])]
    tui = O(("pageData", O(("version", 1),
                           ("componentDefinitions", O(("version", 2), ("importFiles", []),
                                                      ("localComponentDefinitions", [O(("key", k), ("value", v)) for k, v in defs]))),
                           ("replacementStrings", repl),
                           ("info", O(("version", 1), ("type", "CompleteDescription"))),
                           ("tabs", tabs))))
    ql = O(("version", 4), ("info", O(("version", 1), ("type", "CompleteDescription"))),
           ("Screen Mode Q-Links", O(("version", 4), ("map", qmaps))),
           ("Program Mode Q-Links", O(*[("Q-Link %d" % (n + 1), MPC[pid]) for n, pid in enumerate(PROGRAM_QLINKS)])))
    with open(os.path.join(out, "TUI.json"), "w") as f:
        f.write(dump(tui) + "\n")
    with open(os.path.join(out, "Q-Links.json"), "w") as f:
        f.write(dump(ql) + "\n")
    try:                                 # (the panel's matte specks: 256 colours make them a thirtieth the size)
        from PIL import Image
    except ImportError:
        Image = None
        print("(no Pillow: the pictures copied as they are, many times larger)")
    for f in sorted(used):
        if Image:
            im = Image.open(os.path.join(src, f)).convert("RGB")
            im.quantize(256, method=Image.MEDIANCUT, dither=Image.NONE).save(os.path.join(out, f), optimize=True)
        else:
            shutil.copy2(os.path.join(src, f), os.path.join(out, f))
    print("TUI.json: %d pages, %d components; Q-Links.json; %d pictures in %s" % (len(tabs), len(defs), len(used), out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
