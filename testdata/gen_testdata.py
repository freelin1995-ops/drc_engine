"""Generate comprehensive DRC test GDS with KLayout-style test patterns."""

import gdstk
import os

def find_or_create(lib, name):
    for c in lib.cells:
        if c.name == name:
            return c
    return lib.new_cell(name)

# === Layer assignments (following KLayout drctest.gds conventions) ===
# 1/0  - L-shape + rect (boolean ops base layer)
# 2/0  - Overlay patterns: notches, spaces, enclosures
# 3/0  - Large rectangles (sizing, merge, area checks)
# 4/0  - Small rectangles (covering, interacting)
# 5/0  - Complex 13-point polygon (multi-notch, corners)
# 10/0 - Simple rects for boolean tests
# 11/0 - Simple rects for boolean tests (overlapping with 10)
# 12/0 - Rects for boolean/subtraction tests (inside 10)
# 13/0 - Rects for touch/edge tests
# 20/0 - Width check patterns (U-shape with known min width)
# 21/0 - Space check patterns (two rects with known gap)
# 22/0 - Notch patterns (C-shape with known notch)
# 30/0 - Outer enclosure polygon
# 31/0 - Inner polygon (enclosed by 30)
# 32/0 - Non-overlapping for sep_check
# 40/0 - Complex polygon with notches and corners
# 41/0 - Angled edges (non-orthogonal polygon)
# 50/0 - Edge test patterns
# 99/0 - Hierarchy test

def build_layout(dbu=0.001):
    # With dbu=0.001, 1 db unit = 0.001 um = 1e-9 m
    # gdstk stores coordinates in user units (um) by default
    lib = gdstk.Library(unit=1e-6, precision=1e-9)

    # === TOP cell ===
    top = find_or_create(lib, "TOP")

    # Layer 1/0: L-shaped polygon + small rectangle (KLayout drctest compatibility)
    lshape = gdstk.Polygon(
        [(-1.6, -3.1), (-1.6, -2.2), (-1.2, -1.8), (-0.6, -1.8), (-0.6, -3.1)],
        layer=1, datatype=0
    )
    small_rect = gdstk.rectangle((-0.2, -4.0), (0.4, -2.3), layer=1, datatype=0)
    top.add(lshape, small_rect)

    # Layer 2/0: Various test patterns (notches, spaces)
    # Polygon with notch (U-shape)
    notch1 = gdstk.Polygon(
        [(0.1, -3.7), (0.1, -2.6), (0.7, -2.6), (0.7, -3.1), (1.3, -3.1), (1.3, -3.7)],
        layer=2, datatype=0
    )
    # Complex multi-notch polygon
    complex_notch = gdstk.Polygon(
        [(-1.3, -5.5), (-1.3, -5.1), (-1.0, -5.1), (-1.0, -4.8),
         (-1.3, -4.8), (-1.3, -4.6), (-0.7, -4.6), (-0.7, -5.5)],
        layer=2, datatype=0
    )
    # Polygon with protruding feature (enclosure test)
    enclosure_test = gdstk.Polygon(
        [(-2.2, -4.0), (-2.2, -3.2), (-3.2, -3.2), (-3.2, -2.5),
         (-2.0, -2.5), (-2.0, -3.3), (-0.6, -3.3), (-0.6, -4.0)],
        layer=2, datatype=0
    )
    # Simple rects for interacting/inside tests
    rect_a = gdstk.rectangle((-0.3, -5.1), (0.4, -4.4), layer=2, datatype=0)
    rect_b = gdstk.rectangle((-1.3, -4.0), (-0.5, -3.3), layer=2, datatype=0)
    rect_c = gdstk.rectangle((-4.9, -3.0), (-3.9, -2.2), layer=2, datatype=0)
    rect_d = gdstk.rectangle((-4.1, -4.0), (-3.0, -3.3), layer=2, datatype=0)
    # 0.4x0.4 squares for corner/interaction tests
    squares = [
        (-4.7, -4.8), (-4.7, -5.5), (-3.6, -4.8), (-3.6, -5.5),
        (-5.8, -4.8), (-5.8, -5.5)
    ]
    for sx, sy in squares:
        top.add(gdstk.rectangle((sx, sy), (sx+0.4, sy+0.4), layer=2, datatype=0))
    # Thin rect
    thin = gdstk.rectangle((-5.8, -5.0), (-4.5, -4.9), layer=2, datatype=0)
    # Extra rect on TOP only (not in TOP$1)
    top_extra = gdstk.rectangle((-3.4, -2.0), (-2.4, -1.6), layer=2, datatype=0)

    top.add(notch1, complex_notch, enclosure_test, rect_a, rect_b, rect_c, rect_d, thin, top_extra)

    # Layer 3/0: Large rectangles
    big1 = gdstk.rectangle((-4.0, -4.0), (-2.0, -2.0), layer=3, datatype=0)
    big2 = gdstk.rectangle((-3.0, -5.0), (0.0, -3.0), layer=3, datatype=0)
    top.add(big1, big2)

    # Layer 4/0: Small rectangles (covering, interacting patterns)
    small1 = gdstk.rectangle((-1.1, -2.7), (-0.4, -1.6), layer=4, datatype=0)
    small2 = gdstk.rectangle((-0.2, -2.1), (0.7, -1.6), layer=4, datatype=0)
    top.add(small1, small2)

    # Layer 5/0: Complex polygon (13 points, multi-notch)
    complex = gdstk.Polygon(
        [(-5.3, -4.7), (-5.3, -4.2), (-5.6, -4.2), (-5.6, -3.7),
         (-4.9, -3.8), (-4.6, -3.5), (-4.6, -4.3), (-4.9, -4.3),
         (-4.9, -4.7)],
        layer=5, datatype=0
    )
    top.add(complex)

    # ==============================
    # NEW: Expanded test patterns
    # ==============================

    # Layer 10/0: Simple rect A (2x2)
    r10a = gdstk.rectangle((5.0, 0.0), (7.0, 2.0), layer=10, datatype=0)
    r10b = gdstk.rectangle((8.0, 0.0), (10.0, 2.0), layer=10, datatype=0)
    top.add(r10a, r10b)

    # Layer 11/0: Simple rect B (overlapping with 10/0 partially)
    r11a = gdstk.rectangle((6.0, 0.0), (9.0, 2.0), layer=11, datatype=0)
    r11b = gdstk.rectangle((5.0, 3.0), (7.0, 5.0), layer=11, datatype=0)  # non-overlapping region
    top.add(r11a, r11b)

    # Layer 12/0: Inside rect (fully inside 10/0 rect A)
    r12 = gdstk.rectangle((5.5, 0.5), (6.5, 1.5), layer=12, datatype=0)
    top.add(r12)

    # Layer 13/0: Edge-touching rects
    r13a = gdstk.rectangle((5.0, -3.0), (6.0, -1.0), layer=13, datatype=0)
    r13b = gdstk.rectangle((6.0, -3.0), (7.0, -1.0), layer=13, datatype=0)  # shares edge at x=6
    r13c = gdstk.rectangle((5.0, -5.0), (7.0, -4.0), layer=13, datatype=0)
    r13d = gdstk.rectangle((5.0, -4.0), (7.0, -3.0), layer=13, datatype=0)  # shares edge at y=-3
    top.add(r13a, r13b, r13c, r13d)

    # Layer 20/0: Width check patterns
    # U-shape with known min width = 0.3
    width_u = gdstk.Polygon(
        [(12.0, 0.0), (12.0, 2.0), (12.5, 2.0), (12.5, 0.5),
         (13.5, 0.5), (13.5, 2.0), (14.0, 2.0), (14.0, 0.0)],
        layer=20, datatype=0
    )
    # Narrow arm: width = 0.3 (at 12.5 to 13.5 = 1.0 wide, notch from 0.5 to 2.0 = 1.5)
    # Actually, let me reconsider: the narrow part is between x=12.5 and x=13.5
    # The U has arms at y>1.0 and y<1.0, width at notch = (13.5-12.5)=1.0 
    # Min width is actually at the bottom: (14.0-12.0)=2.0 horizontally, not narrow
    # Let me make a better width test
    # Width test: a rect with a middle notch creating 0.3 width
    width_test = gdstk.Polygon(
        [(12.0, 0.0), (12.0, 3.0), (14.0, 3.0), (14.0, 1.5),
         (12.8, 1.5), (12.8, 1.2), (14.0, 1.2), (14.0, 0.0)],
        layer=20, datatype=0
    )
    # Width at narrowest: 12.8-12.0 = 0.8 between x=12..14? No wait.
    # Let me trace: (12,0)-(12,3)-(14,3)-(14,1.5)-(12.8,1.5)-(12.8,1.2)-(14,1.2)-(14,0)
    # Bottom section: width = 14-12 = 2.0 (y=0 to y=1.2)
    # Middle section: x=12..12.8, width = 0.8 (y=1.2 to y=1.5)
    # Top section: width = 14-12 = 2.0 (y=1.5 to y=3.0)
    # Min width = 0.8

    # Also add a classic line-width test: long thin rect
    thin_line = gdstk.rectangle((12.0, -2.0), (12.3, -0.5), layer=20, datatype=0)
    # width = 0.3

    top.add(width_test, thin_line)

    # Layer 21/0: Space check patterns
    # Two rects with gap = 0.5
    space_a = gdstk.rectangle((16.0, 0.0), (18.0, 2.0), layer=21, datatype=0)
    space_b = gdstk.rectangle((18.5, 0.0), (20.5, 2.0), layer=21, datatype=0)
    # gap between 18.0 and 18.5 = 0.5

    # Diagonal space pattern
    space_c = gdstk.rectangle((16.0, -2.0), (17.0, -0.5), layer=21, datatype=0)
    space_d = gdstk.rectangle((16.5, -1.5), (17.5, 0.0), layer=21, datatype=0)
    # overlapping in projection, min gap = 0.5
    top.add(space_a, space_b, space_c, space_d)

    # Layer 22/0: Notch patterns
    # C-shape with notch = 0.4
    notch_c = gdstk.Polygon(
        [(19.0, 0.0), (19.0, 3.0), (21.0, 3.0), (21.0, 2.5),
         (19.5, 2.5), (19.5, 0.5), (21.0, 0.5), (21.0, 0.0)],
        layer=22, datatype=0
    )
    # Notch width: 21.0 - 19.5 = 1.5 (opening), depth: 2.5-0.5 = 2.0
    # Notch dimension (minimum opening): 19.5 to 21.0 = 1.5

    # Narrow notch
    notch_narrow = gdstk.Polygon(
        [(22.0, 0.0), (22.0, 3.0), (24.0, 3.0), (24.0, 2.5),
         (22.5, 2.5), (22.5, 1.8), (22.8, 1.8), (22.8, 0.7),
         (22.5, 0.7), (22.5, 0.5), (24.0, 0.5), (24.0, 0.0)],
        layer=22, datatype=0
    )
    # Double notch: width = 0.3

    top.add(notch_c, notch_narrow)

    # ==========================================
    # Layer 30/0: Outer enclosure
    # ==========================================
    outer = gdstk.rectangle((5.0, 5.0), (10.0, 9.0), layer=30, datatype=0)
    top.add(outer)

    # Layer 31/0: Inner (enclosed by 30/0)
    inner = gdstk.rectangle((6.0, 6.0), (9.0, 8.0), layer=31, datatype=0)
    top.add(inner)

    # Layer 32/0: Non-overlapping (outside outer)
    non_overlap = gdstk.rectangle((11.0, 5.0), (13.0, 9.0), layer=32, datatype=0)
    top.add(non_overlap)

    # Layer 33/0: Partially overlapping
    partial = gdstk.rectangle((7.0, 8.0), (11.0, 10.0), layer=33, datatype=0)
    top.add(partial)

    # ==========================================
    # Layer 40/0: Complex polygon with notches, holes, corners
    # ==========================================
    # This creates a shape with convex (outward, -90) and concave (inward, 90) corners
    complex_poly = gdstk.Polygon(
        [(12.0, 5.0), (12.0, 9.0), (15.0, 9.0), (15.0, 7.5),
         (13.5, 7.5), (13.5, 6.5), (15.0, 6.5), (15.0, 5.0)],
        layer=40, datatype=0
    )
    top.add(complex_poly)

    # ==========================================
    # Layer 50/0: Edge test patterns
    # ==========================================
    edge_line = gdstk.rectangle((12.0, -5.0), (15.0, -4.5), layer=50, datatype=0)
    # This will give edges of length 3.0 (horizontal) and 0.5 (vertical)
    top.add(edge_line)

    # ==========================================
    # Layer 60/0: Angled/non-orthogonal polygons
    # ==========================================
    angled = gdstk.Polygon(
        [(17.0, 0.0), (18.0, 2.0), (20.0, 2.0), (19.0, 0.0)],
        layer=60, datatype=0
    )
    top.add(angled)

    # ==========================================
    # Layer 80/0: Wide enclosure + nested
    # ==========================================
    wide_outer = gdstk.rectangle((-0.5, 5.0), (4.5, 9.0), layer=80, datatype=0)
    wide_inner = gdstk.rectangle((0.0, 5.5), (4.0, 8.5), layer=80, datatype=0)
    # Enclosure = 0.5 on each side
    top.add(wide_outer, wide_inner)

    # ==========================================
    # Cell hierarchy
    # ==========================================
    # TOPTOP_BIG: single ref cell
    big_cell = find_or_create(lib, "TOPTOP_BIG")
    big_cell.add(gdstk.rectangle((1.6, -0.1), (2.0, 0.3), layer=3, datatype=0))

    # TOPTOP_SMALL: three ref cells
    small_cell = find_or_create(lib, "TOPTOP_SMALL")
    small_cell.add(gdstk.rectangle((0, 3), (1, 4), layer=3, datatype=0))

    # TOPTOP: references TOPTOP_BIG
    toptop = find_or_create(lib, "TOPTOP")
    toptop.add(gdstk.Reference(big_cell, origin=(0, 0)))

    # Reference into TOP
    top.add(gdstk.Reference(big_cell, origin=(5, -5)))
    top.add(gdstk.Reference(small_cell, origin=(7, -5)))
    top.add(gdstk.Reference(small_cell, origin=(8.5, -5)))
    top.add(gdstk.Reference(small_cell, origin=(10, -5)))

    # ==========================================
    # Array test
    # ==========================================
    # Sub-cell with array references (for hierarchical mode)
    array_cell = find_or_create(lib, "ARRAY_CELL")
    array_cell.add(gdstk.rectangle((0, 0), (0.2, 0.2), layer=90, datatype=0))

    return lib

if __name__ == "__main__":
    output = os.path.join(os.path.dirname(__file__), "test_drc.gds")
    lib = build_layout(dbu=0.001)
    lib.write_gds(output)
    print(f"Written {output}")
    print(f"Cells: {len(lib.cells)}")
    for c in lib.cells:
        n = len(c.polygons) + len(c.paths) + len(c.labels) + len(c.references)
        print(f"  {c.name}: {n} elements")
