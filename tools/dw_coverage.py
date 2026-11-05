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
status = {}
sp = os.path.join(root, "tools", "interface_support.tsv")
if os.path.exists(sp):
    for l in list(open(sp))[1:]:
        f = l.rstrip("\n").split("\t"); status[f[0]] = f[1]
# per-method call-site counts (macro calls anywhere in tests, attributed to the declaring interface)
cnt = collections.Counter(re.findall(r"\bI(DWrite\w+)_(\w+)\s*\(", src))
def calls(i, m):
    n = 0
    for (a, b), v in cnt.items():
        if b != m: continue
        k = "I" + a
        while k:
            if k == i: n += v; break
            k = base.get(k)
    return n
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
thin = [(i, m, calls(i, m)) for i in decl for m in decl[i] if m in covered[i] and 0 < calls(i, m) <= 3 and m not in ("QueryInterface","AddRef","Release")]
lines += ["", "## Interface status (19041) and callback interfaces", "",
          "Interfaces marked callback are implemented by the test; their methods are driven by dwrite and observed through the common/dwx doubles, so zero macro calls is expected.",
          "not-in-19041 interfaces are not reachable on build 19041 (QueryInterface must fail).", ""]
for i in decl:
    if status.get(i) in ("callback", "not-in-19041"): lines.append(f"- {i}: {status[i]}")
lines += ["", "## Thin (1-3 macro call sites)", ""] + [f"- {i}::{m} ({n})" for i, m, n in thin]
open(os.path.join(root, "COVERAGE.md"), "w").write("\n".join(lines) + "\n")
open(os.path.join(root, "COVERAGE.md"), "w").write("\n".join(lines) + "\n")
