#!/usr/bin/env python3
"""bakes the wordmark svg into src/ui/WordmarkData.h.

usage: bake_wordmark.py <wordmark.svg>

only absolute M, C and Z path commands and translate() transforms are supported,
which is what the exported brand asset uses.
"""

import re
import sys


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    svg = open(sys.argv[1], encoding="utf-8").read()
    svg = re.sub(r"<metadata>.*?</metadata>", "", svg, flags=re.S)
    view = re.search(r'viewBox="([^"]+)"', svg).group(1).split()
    paths = []
    for attrs in re.findall(r"<path([^>]*)/?>", svg):
        d = re.search(r'\bd="([^"]+)"', attrs).group(1)
        tx = ty = 0.0
        t = re.search(r'transform="translate\(([^)]+)\)"', attrs)
        if t:
            parts = [float(v) for v in re.split(r"[ ,]+", t.group(1).strip())]
            tx, ty = parts[0], parts[1] if len(parts) > 1 else 0.0
        tokens = re.findall(r"[MCZ]|-?\d*\.?\d+(?:e-?\d+)?", d)
        cmds = []
        i = 0
        while i < len(tokens):
            tok = tokens[i]
            if tok == "M":
                cmds.append(("M", [float(tokens[i + 1]) + tx, float(tokens[i + 2]) + ty]))
                i += 3
            elif tok == "C":
                vals = [float(v) for v in tokens[i + 1:i + 7]]
                cmds.append(("C", [vals[0] + tx, vals[1] + ty, vals[2] + tx, vals[3] + ty, vals[4] + tx, vals[5] + ty]))
                i += 7
            elif tok == "Z":
                cmds.append(("Z", []))
                i += 1
            else:
                raise SystemExit(f"unsupported token {tok}")
        paths.append(cmds)

    out = []
    out.append("#pragma once\n")
    out.append("namespace citron::wordmark {\n")
    out.append(f"constexpr float kViewX = {float(view[0])}f;")
    out.append(f"constexpr float kViewY = {float(view[1])}f;")
    out.append(f"constexpr float kViewWidth = {float(view[2])}f;")
    out.append(f"constexpr float kViewHeight = {float(view[3])}f;\n")
    out.append("enum class Op : unsigned char { Move, Curve, Close, End };\n")
    out.append("struct Command { Op op; float v[6]; };\n")
    out.append("constexpr Command kCommands[] = {")
    for cmds in paths:
        for op, vals in cmds:
            padded = vals + [0.0] * (6 - len(vals))
            name = {"M": "Move", "C": "Curve", "Z": "Close"}[op]
            out.append("    {Op::%s, {%s}}," % (name, ", ".join(f"{v:.3f}f" for v in padded)))
        out.append("    {Op::End, {0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},")
    out.append("};\n")
    out.append("}\n")
    with open("src/ui/WordmarkData.h", "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))
    print(f"{len(paths)} paths baked", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
