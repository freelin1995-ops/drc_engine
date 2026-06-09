"""Generate independent GDS test layouts, one per test case."""

import gdstk
import os
import math

OUTDIR = os.path.join(os.path.dirname(__file__), "layouts")
os.makedirs(OUTDIR, exist_ok=True)

def make_layout(name, dbu=0.001):
    lib = gdstk.Library(name=name, unit=1e-6, precision=1e-9)
    top = lib.new_cell("TOP")
    return lib, top

def write_layout(lib, name):
    path = os.path.join(OUTDIR, f"{name}.gds")
    lib.write_gds(path)
    return path

# ================================================================
# Layout 1: Basic input
# Layer 10/0: one rect → count=1
# Layer 99/0: empty → count=0
# ================================================================
lib, top = make_layout("basic_input")
top.add(gdstk.rectangle((5, 0), (7, 2), layer=10, datatype=0))
write_layout(lib, "basic_input")

# ================================================================
# Layout 2: Boolean operations
# Layer 10/0: rect (0,0)-(2,2) area=4
# Layer 11/0: rect (1,1)-(3,3) area=4
# Intersection: (1,1)-(2,2) area=1
# ================================================================
lib, top = make_layout("boolean_ops")
top.add(gdstk.rectangle((0, 0), (2, 2), layer=10, datatype=0))
top.add(gdstk.rectangle((1, 1), (3, 3), layer=11, datatype=0))
write_layout(lib, "boolean_ops")

# ================================================================
# Layout 3: Merge — two adjacent rects that merge into one
# Layer 10/0: (0,0)-(1,1) and (1,0)-(2,1) → merge → count=1
# ================================================================
lib, top = make_layout("merge_test")
top.add(gdstk.rectangle((0, 0), (1, 1), layer=10, datatype=0))
top.add(gdstk.rectangle((1, 0), (2, 1), layer=10, datatype=0))
write_layout(lib, "merge_test")

# ================================================================
# Layout 4: Sizing
# Layer 10/0: rect (0,0)-(1,1) area=1um²
# ================================================================
lib, top = make_layout("sizing_test")
top.add(gdstk.rectangle((0, 0), (1, 1), layer=10, datatype=0))
write_layout(lib, "sizing_test")

# ================================================================
# Layout 5: Selection — Interacting/Inside/Outside/Enclosing
# Layer 40/0: big rect (0,0)-(3,3)
# Layer 41/0: 3 polys: inside (0.5,0.5)-(2.5,2.5),
#             touching (3,0.5)-(4,1.5),
#             far (5,5)-(6,6)
# ================================================================
lib, top = make_layout("selection_test")
top.add(gdstk.rectangle((0, 0), (3, 3), layer=40, datatype=0))
top.add(gdstk.rectangle((0.5, 0.5), (2.5, 2.5), layer=41, datatype=0))
top.add(gdstk.rectangle((3, 0.5), (4, 1.5), layer=41, datatype=0))
top.add(gdstk.rectangle((5, 5), (6, 6), layer=41, datatype=0))
write_layout(lib, "selection_test")

# ================================================================
# Layout 6: Width / Space checks
# Layer 10/0: U-shape with 0.3um narrow arm + thin rect width=0.3
# Layer 10/0: also two rects with 0.5um gap
# ================================================================
lib, top = make_layout("width_space_check")
# U-shape: width at narrowest = 0.8 (actually a bit complex to compute)
# Let's use a simpler pattern: rect with notch creating 0.3 width
w = gdstk.Polygon([
    (0, 0), (0, 3), (3, 3), (3, 2),
    (0.8, 2), (0.8, 1), (3, 1), (3, 0)
], layer=10, datatype=0)
# narrowest width = 0.8 (at x=0 to x=0.8, between y=1 and y=2)
# Actually wait, the narrow part is between x=0 and x=0.8 (width=0.8)
# Let me use a thinner width:
# Simple thin line
thin = gdstk.rectangle((4, 0), (4.3, 2), layer=10, datatype=0)
# width = 0.3

# Space test: gap = 0.5
sp1 = gdstk.rectangle((6, 0), (8, 2), layer=10, datatype=0)
sp2 = gdstk.rectangle((8.5, 0), (10.5, 2), layer=10, datatype=0)
# gap = 0.5

top.add(w, thin, sp1, sp2)
write_layout(lib, "width_space_check")

# ================================================================
# Layout 7: Enclosure check
# Layer 20/0: outer rect (0,0)-(2,2)
# Layer 21/0: inner rect (0.5,0.5)-(1.5,1.5)
# Enclosure margin = 0.5 on each side
# ================================================================
lib, top = make_layout("enclosure_check")
top.add(gdstk.rectangle((0, 0), (2, 2), layer=20, datatype=0))
top.add(gdstk.rectangle((0.5, 0.5), (1.5, 1.5), layer=21, datatype=0))
write_layout(lib, "enclosure_check")

# ================================================================
# Layout 8: With_area filter
# Layer 10/0: 5 rects of different areas
# (0,0)-(0.05,1): area=0.05
# (1,0)-(1.10,1): area=0.10
# (2,0)-(2.15,1): area=0.15
# (3,0)-(3.20,1): area=0.20
# (4,0)-(4.25,1): area=0.25
# ================================================================
lib, top = make_layout("area_filter")
for i, w in enumerate([0.05, 0.10, 0.15, 0.20, 0.25]):
    top.add(gdstk.rectangle((i * 2, 0), (i * 2 + w, 1), layer=10, datatype=0))
write_layout(lib, "area_filter")

# ================================================================
# Layout 9: Edge operations (Region → Edges)
# Layer 60/0: 1x1 um square → 4 edges
# ================================================================
lib, top = make_layout("edge_ops")
top.add(gdstk.rectangle((0, 0), (1, 1), layer=60, datatype=0))
write_layout(lib, "edge_ops")

# ================================================================
# Layout 10: Output test
# Layer 10/0: simple rect
# ================================================================
lib, top = make_layout("output_test")
top.add(gdstk.rectangle((0, 0), (2, 1), layer=10, datatype=0))
write_layout(lib, "output_test")

# ================================================================
# Layout 11: Chained operations
# Layer 30/0: rect (0,0)-(2,2)
# Layer 31/0: rect (1,1)-(3,3)
# ================================================================
lib, top = make_layout("chained_ops")
top.add(gdstk.rectangle((0, 0), (2, 2), layer=30, datatype=0))
top.add(gdstk.rectangle((1, 1), (3, 3), layer=31, datatype=0))
write_layout(lib, "chained_ops")

# ================================================================
# Layout 12: Edge extended (same as edge_ops)
# ================================================================
lib, top = make_layout("edge_extended")
top.add(gdstk.rectangle((0, 0), (1, 1), layer=60, datatype=0))
write_layout(lib, "edge_extended")

# ================================================================
# Layout 13: Edge segments
# ================================================================
lib, top = make_layout("edge_segments")
top.add(gdstk.rectangle((0, 0), (1, 1), layer=60, datatype=0))
write_layout(lib, "edge_segments")

# ================================================================
# Layout 14: Corner detection
# Layer 60/0: 1x1 square (4 convex -90 corners)
# Layer 61/0: L-shape (6 convex -90 corners)
# ================================================================
lib, top = make_layout("corner_detection")
top.add(gdstk.rectangle((0, 0), (1, 1), layer=60, datatype=0))
lshape = gdstk.Polygon([
    (2, 0), (2, 2), (3, 2), (3, 1), (4, 1), (4, 0)
], layer=61, datatype=0)
top.add(lshape)
write_layout(lib, "corner_detection")

# ================================================================
# Layout 15: Perimeter
# Layer 60/0: 1x1 square (perimeter=4)
# ================================================================
lib, top = make_layout("perimeter_test")
top.add(gdstk.rectangle((0, 0), (1, 1), layer=60, datatype=0))
write_layout(lib, "perimeter_test")

# ================================================================
# Layout 16: DRC checks with EdgePairs
# Layer 10/0: same as width_space_check
# ================================================================
lib, top = make_layout("drc_checks")
w = gdstk.Polygon([
    (0, 0), (0, 3), (3, 3), (3, 2),
    (0.8, 2), (0.8, 1), (3, 1), (3, 0)
], layer=10, datatype=0)
thin = gdstk.rectangle((4, 0), (4.3, 2), layer=10, datatype=0)
sp1 = gdstk.rectangle((6, 0), (8, 2), layer=10, datatype=0)
sp2 = gdstk.rectangle((8.5, 0), (10.5, 2), layer=10, datatype=0)
top.add(w, thin, sp1, sp2)
write_layout(lib, "drc_checks")

# ================================================================
# Layout 17: Extended edges generic
# ================================================================
lib, top = make_layout("extended_generic")
top.add(gdstk.rectangle((0, 0), (1, 1), layer=60, datatype=0))
write_layout(lib, "extended_generic")

# ================================================================
# Layout 18: Compound boolean with edges
# Layer 30/0: rect (0,0)-(2,2)
# Layer 31/0: rect (1,1)-(3,3)
# ================================================================
lib, top = make_layout("compound_boolean_edges")
top.add(gdstk.rectangle((0, 0), (2, 2), layer=30, datatype=0))
top.add(gdstk.rectangle((1, 1), (3, 3), layer=31, datatype=0))
write_layout(lib, "compound_boolean_edges")

# ================================================================
# KLayout migration layouts
# ================================================================

# KLayout test 1: Basic polygon creation and booleans
lib, top = make_layout("klayout_basic")
top.add(gdstk.rectangle((0, 0), (2, 2), layer=1, datatype=0))
top.add(gdstk.rectangle((1, 1), (3, 3), layer=2, datatype=0))
write_layout(lib, "klayout_basic")

# KLayout test 2: Width check
lib, top = make_layout("klayout_width")
# Polygon with narrow section
poly = gdstk.Polygon([
    (-5, 0), (-5, 5), (-2, 5), (-2, 2.5),
    (-4, 2.5), (-4, 2), (-2, 2), (-2, 0)
], layer=1, datatype=0)
top.add(poly)
write_layout(lib, "klayout_width")

# KLayout test 3: Space check
lib, top = make_layout("klayout_space")
top.add(gdstk.rectangle((0, 0), (2, 2), layer=1, datatype=0))
top.add(gdstk.rectangle((3, 0), (5, 2), layer=1, datatype=0))
write_layout(lib, "klayout_space")

# KLayout test 4: Enclosure
lib, top = make_layout("klayout_enclosure")
top.add(gdstk.rectangle((-2, -2), (2, 2), layer=1, datatype=0))
top.add(gdstk.rectangle((-1, -1), (1, 1), layer=2, datatype=0))
write_layout(lib, "klayout_enclosure")

# KLayout test 5: Boolean operations (region)
lib, top = make_layout("klayout_boolean")
top.add(gdstk.rectangle((0, 0), (3, 2), layer=1, datatype=0))
top.add(gdstk.rectangle((1, 1), (4, 3), layer=2, datatype=0))
write_layout(lib, "klayout_boolean")

# KLayout test 6: Sizing
lib, top = make_layout("klayout_sizing")
top.add(gdstk.rectangle((0, 0), (2, 2), layer=1, datatype=0))
write_layout(lib, "klayout_sizing")

# KLayout test 7: Interacting / Inside / Outside
lib, top = make_layout("klayout_selection")
top.add(gdstk.rectangle((0, 0), (4, 4), layer=1, datatype=0))
top.add(gdstk.rectangle((1, 1), (3, 3), layer=2, datatype=0))  # inside
top.add(gdstk.rectangle((4, 0), (5, 4), layer=2, datatype=0))  # touching
top.add(gdstk.rectangle((6, 6), (7, 7), layer=2, datatype=0))  # far
write_layout(lib, "klayout_selection")

# KLayout test 8: Corner detection
lib, top = make_layout("klayout_corners")
top.add(gdstk.rectangle((0, 0), (2, 2), layer=1, datatype=0))
lshape = gdstk.Polygon([
    (3, 0), (3, 3), (5, 3), (5, 1), (6, 1), (6, 0)
], layer=2, datatype=0)
top.add(lshape)
write_layout(lib, "klayout_corners")

# KLayout test 9: Edge operations
lib, top = make_layout("klayout_edges")
top.add(gdstk.rectangle((0, 0), (2, 2), layer=1, datatype=0))
write_layout(lib, "klayout_edges")

# KLayout test 10: Setup source/target
lib, top = make_layout("klayout_setup")
top.add(gdstk.rectangle((0, 0), (1, 1), layer=1, datatype=0))
write_layout(lib, "klayout_setup")

print(f"Generated {len(os.listdir(OUTDIR))} test layouts in {OUTDIR}")
for f in sorted(os.listdir(OUTDIR)):
    lib = gdstk.read_gds(os.path.join(OUTDIR, f))
    c = lib.cells[0]
    n = len(c.polygons) + len(c.references) + len(c.paths)
    print(f"  {f}: {n} elements")
