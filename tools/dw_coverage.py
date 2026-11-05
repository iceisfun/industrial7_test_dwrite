#!/usr/bin/env python3
"""Interface-method coverage for dwrite.dll COM interfaces.

A method is covered when a test under tests/ calls it
  * through the COBJMACROS name  I<Interface>_<Method>(  -- counts for <Interface> and, because the
    macro of a derived interface dispatches to the same vtable slot, for the interface that declares it;
  * or through  ->lpVtbl-> Method(  (heuristic: credits every interface declaring a method of that name).
Own test doubles' implementation tables (common/ and designated initialisers) are not calls.
usage: dw_coverage.py [corpus_root]   -> prints per-interface totals, writes COVERAGE.md"""
import re, sys, os, glob, collections
root = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rows = [l.rstrip("\n").split("\t") for l in open(os.path.join(root, "tools", "dwrite_interfaces.tsv"))][1:]
decl = collections.OrderedDict()          # iface -> [methods declared]
base = {}
for i, b, m, idx, intro, hdr in rows:
    decl.setdefault(i, []).append(m); base[i] = b
def chain(i):
    out = []
    while i in decl: out.append(i); i = base.get(i)
    return out
src = ""
for f in glob.glob(os.path.join(root, "tests", "**", "*.[ch]"), recursive=True):
    src += re.sub(r"/\*.*?\*/|//[^\n]*", "", open(f, errors="replace").read(), flags=re.S) + "\n"
macro = set(re.findall(r"\bI(DWrite\w+)_(\w+)\s*\(", src))          # (Iface-without-I, Method)
macro = {("I" + a, b) for a, b in macro}
vt = set(re.findall(r"lpVtbl\s*->\s*(\w+)\s*\(", src))
covered = collections.defaultdict(set)    # declaring iface -> methods
for i in decl:
    for anc in chain(i):
        for m in decl[anc]:
            if (i, m) in macro: covered[anc].add(m)
            elif m in vt and anc == i: covered[anc].add(m)
# interface-level view (a derived interface also "has" inherited methods called through it)
tot = sum(len(v) for v in decl.values()); cov = sum(len(covered[i]) for i in decl)
lines = ["# dwrite interface-method coverage", "", f"Total: {cov}/{tot} methods covered ({100*cov/max(tot,1):.1f}%)", "",
         "| Interface | Covered | Total | Missing |", "|---|---:|---:|---|"]
print(f"{'interface':36s} {'cov':>4s}/{'tot':<4s}")
for i, ms in decl.items():
    c = covered[i]; miss = [m for m in ms if m not in c]
    print(f"{i:36s} {len(c):4d}/{len(ms):<4d}")
    lines.append(f"| {i} | {len(c)} | {len(ms)} | {', '.join(miss)} |")
print(f"TOTAL {cov}/{tot}")
open(os.path.join(root, "COVERAGE.md"), "w").write("\n".join(lines) + "\n")
