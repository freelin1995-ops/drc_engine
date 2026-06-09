-- KLayout DRC Migration Tests — each test uses its own layout
local tests = {}
local passed = 0
local failed = 0

function assert_eq(a, b, msg)
    if a ~= b then
        error(string.format("%s: expected %s, got %s", msg, tostring(b), tostring(a)))
    end
end

function assert_true(v, msg)
    if not v then
        error(string.format("%s: expected true", msg))
    end
end

function assert_false(v, msg)
    if v then
        error(string.format("%s: expected false", msg))
    end
end

function assert_near(a, b, tol, msg)
    if math.abs(a - b) > tol then
        error(string.format("%s: expected %f, got %f (tol=%f)", msg, b, a, tol))
    end
end

function run_test(name, fn)
    local ok, err = pcall(fn)
    if ok then
        passed = passed + 1
        print(string.format("  PASS: %s", name))
    else
        failed = failed + 1
        print(string.format("  FAIL: %s -- %s", name, err))
    end
end

function L(path) return "testdata/layouts/" .. path end

-- ============================================================
-- Test 1: Basic polygon creation and booleans
-- Layout: klayout_basic.gds — layer1 rect (0,0)-(2,2), layer2 rect (1,1)-(3,3)
-- ============================================================
table.insert(tests, function()
    source(L("klayout_basic.gds"))
    local a = input(1, 0)
    local b = input(2, 0)

    assert_eq(a:count(), 1, "layer1 count")
    assert_eq(b:count(), 1, "layer2 count")

    local and_r = a & b
    assert_eq(and_r:count(), 1, "AND count")

    local or_r = a | b
    assert_true(or_r:count() >= 1, "OR count >= 1")

    local sub_r = a - b
    assert_true(sub_r:count() >= 1, "A-B count >= 1")

    local xor_r = a ~ b
    assert_true(xor_r:count() >= 1, "XOR count >= 1")
end)

-- ============================================================
-- Test 2: Width check
-- Layout: klayout_width.gds
--   layer1: polygon with narrow section (min width ~1.5um depending on geometry)
--   Narrow part: (-4,2)-(-2,2.5) width = 2um, arms at x=-5..-2
--   Actually the narrow section is between the notch. Min width is 2.0
-- ============================================================
table.insert(tests, function()
    source(L("klayout_width.gds"))
    local a = input(1, 0)

    local w_small = a:width(1.0)
    assert_eq(w_small:count(), 0, "width 1.0: no violations (min width ~2.0)")

    local w_medium = a:width(1.5)
    assert_true(w_medium:count() > 0, "width 1.5 has violations (min width ~1.0)")
end)

-- ============================================================
-- Test 3: Space check
-- Layout: klayout_space.gds — layer1 rects (0,0)-(2,2) and (3,0)-(5,2), gap=1.0
-- ============================================================
table.insert(tests, function()
    source(L("klayout_space.gds"))
    local a = input(1, 0)

    local s_small = a:space(0.5)
    assert_eq(s_small:count(), 0, "space 0.5: no violations (gap=1.0)")

    local s_med = a:space(1.5)
    assert_true(s_med:count() > 0, "space 1.5: violates (gap=1.0 < 1.5)")
end)

-- ============================================================
-- Test 4: Enclosure
-- Layout: klayout_enclosure.gds
--   layer1 outer (-2,-2)-(2,2)
--   layer2 inner (-1,-1)-(1,1)  margin=1.0
-- ============================================================
table.insert(tests, function()
    source(L("klayout_enclosure.gds"))
    local outer = input(1, 0)
    local inner = input(2, 0)

    local enc_ok = outer:enclosing_check(inner, 0.5)
    assert_eq(enc_ok:count(), 0, "enclosing_check 0.5: ok (margin=1.0)")

    local enc_fail = outer:enclosing_check(inner, 1.5)
    assert_true(enc_fail:count() > 0, "enclosing_check 1.5: violations (margin=1.0)")
end)

-- ============================================================
-- Test 5: Boolean operations (region)
-- Layout: klayout_boolean.gds
--   layer1 rect (0,0)-(3,2), layer2 rect (1,1)-(4,3)
--   Overlap: (1,1)-(3,2)
-- ============================================================
table.insert(tests, function()
    source(L("klayout_boolean.gds"))
    local a = input(1, 0)
    local b = input(2, 0)

    local and_r = a & b
    assert_eq(and_r:count(), 1, "AND count")
    assert_true(and_r:area() > 0, "AND area > 0")

    local or_r = a | b
    assert_true(or_r:area() > and_r:area(), "OR area > AND area")
end)

-- ============================================================
-- Test 6: Sizing
-- Layout: klayout_sizing.gds — rect (0,0)-(2,2)
-- ============================================================
table.insert(tests, function()
    source(L("klayout_sizing.gds"))
    local a = input(1, 0)

    local bigger = a:sized(0.5)
    assert_true(bigger:area() > a:area(), "sized+ area bigger")

    local smaller = a:sized(-0.5)
    assert_true(smaller:area() < a:area(), "sized- area smaller")
end)

-- ============================================================
-- Test 7: Interacting / Inside / Outside
-- Layout: klayout_selection.gds
--   layer1 big (0,0)-(4,4)
--   layer2 inside (1,1)-(3,3), touching (4,0)-(5,4), far (6,6)-(7,7)
-- ============================================================
table.insert(tests, function()
    source(L("klayout_selection.gds"))
    local big = input(1, 0)
    local small = input(2, 0)

    assert_eq(small:count(), 3, "small count = 3")

    local inside = small:inside(big)
    assert_eq(inside:count(), 1, "inside count = 1 (fully inside)")

    local interact = small:interacting(big)
    assert_eq(interact:count(), 2, "interacting count = 2 (inside + touching)")

    local outside = small:outside(big)
    assert_eq(outside:count(), 2, "outside count = 2 (touching + far)")
end)

-- ============================================================
-- Test 8: Corner detection
-- Layout: klayout_corners.gds
--   layer1: 2x2 square → 4 convex corners
--   layer2: L-shape → 6 convex corners
-- ============================================================
table.insert(tests, function()
    source(L("klayout_corners.gds"))
    local sq = input(1, 0)
    local ls = input(2, 0)

    local c_sq = sq:corners_dots(-90, -90)
    assert_eq(c_sq:count(), 4, "square convex corners = 4")

    local c_ls = ls:corners_dots(-90, -90)
    -- 6-vertex L-shape: 5 convex + 1 concave
    assert_eq(c_ls:count(), 5, "L-shape convex corners = 5")
end)

-- ============================================================
-- Test 9: Edge operations
-- Layout: klayout_edges.gds — 2x2 square
-- ============================================================
table.insert(tests, function()
    source(L("klayout_edges.gds"))
    local a = input(1, 0)
    local e = a:edges()

    assert_true(e:count() >= 4, "square has >= 4 edges")
    assert_true(e:length() > 0, "edge total length > 0")
    assert_near(e:length(), 8000, 1, "2x2 square perimeter = 8000 db")

    local eo = e:extended_out(0.1)
    assert_true(eo:count() > 0, "extended_out has polygons")

    local c = e:centers(0, 0.5)
    assert_true(c:count() > 0, "centers segments exist")
end)

-- ============================================================
-- Test 10: Setup source/target
-- Layout: klayout_setup.gds — layer1 rect (0,0)-(1,1)
-- ============================================================
table.insert(tests, function()
    source(L("klayout_setup.gds"))
    target("/tmp/klayout_setup_out.gds")

    local a = input(1, 0)
    a:output(10, 0)

    local b = input(1, 0)
    b:output(20, 0)

    write()
    print("    Output written to /tmp/klayout_setup_out.gds")
end)

-- ============================================================
-- Test 11: Merged region properties
-- Layout: klayout_basic.gds
-- ============================================================
table.insert(tests, function()
    source(L("klayout_basic.gds"))
    local a = input(1, 0)
    local b = input(2, 0)

    local merged = (a | b):merge()
    assert_true(merged:count() >= 1, "merged union count >= 1")
    assert_near(merged:area(), (a | b):area(), 1, "merged area = union area")
end)

-- ============================================================
-- Test 12: Enclosing (region predicate)
-- Layout: klayout_enclosure.gds
-- ============================================================
table.insert(tests, function()
    source(L("klayout_enclosure.gds"))
    local outer = input(1, 0)
    local inner = input(2, 0)

    local enc = outer:enclosing(inner)
    -- outer encloses inner (inner is fully inside outer)
    -- enclosing returns the polygons from outer that enclose at least one polygon from inner
    assert_true(enc:count() >= 1, "outer polygon enclosing inner exists")

    local not_enc = inner:enclosing(outer)
    assert_eq(not_enc:count(), 0, "inner does not enclose outer")
end)

-- ============================================================
-- Test 13: sep_check
-- Layout: klayout_selection.gds
--   layer1 big (0,0)-(4,4), layer2 touching (4,0)-(5,4) has 0 separation
--   layer2 far (6,6)-(7,7) has separation of 2.0 from big
-- ============================================================
table.insert(tests, function()
    source(L("klayout_selection.gds"))
    local big = input(1, 0)
    local small = input(2, 0)

    local sep = big:sep_check(small, 0.1)
    assert_true(sep:count() > 0, "sep_check: touching edges flagged")
end)

-- ============================================================
-- Test 14: overlap_check
-- Layout: klayout_selection.gds
--   layer1 = (0,0)-(4,4), layer2 inside = (1,1)-(3,3)
--   Overlap margin = 1.0 (from big edge to inner edge)
-- ============================================================
table.insert(tests, function()
    source(L("klayout_selection.gds"))
    local big = input(1, 0)
    local small = input(2, 0)

    local ov = big:overlap_check(small, 1.5)
    -- Overlap margin is 1.0 (big.x1=0, inner.x1=1, so overlap = 1)
    -- With threshold 1.5, overlap 1.0 < 1.5 → violations
    assert_true(ov:count() > 0, "overlap_check 1.5: violations (overlap margin=1.0)")
end)

-- ============================================================
-- Test 15: Area and count queries
-- Layout: area_filter.gds — 5 rects of different sizes
-- ============================================================
table.insert(tests, function()
    source(L("area_filter.gds"))
    local a = input(10, 0)

    assert_eq(a:count(), 5, "count = 5")
    assert_false(a:empty(), "not empty")

    local total_area = 0
    for i, w in ipairs({0.05, 0.10, 0.15, 0.20, 0.25}) do
        total_area = total_area + w * 1.0
    end
    local expected_db = total_area / (0.001 * 0.001)
    assert_near(a:area(), expected_db, 1000, "total area")
end)

-- ============================================================
-- Test 16: Perimeter and with_perimeter
-- Layout: perimeter_test.gds — 1x1 square
-- ============================================================
table.insert(tests, function()
    source(L("perimeter_test.gds"))
    local a = input(60, 0)

    assert_near(a:perimeter(), 4000, 1, "perimeter = 4000 db (4um)")

    local pf = a:with_perimeter(3.0, 5.0)
    assert_eq(pf:count(), 1, "perimeter filter in range keeps square")

    local pf_out = a:with_perimeter(0, 3.0)
    assert_eq(pf_out:count(), 0, "perimeter filter out of range excludes")
end)

-- ============================================================
-- Test 17: Edge type queries
-- Layout: edge_ops.gds — 1x1 square
-- ============================================================
table.insert(tests, function()
    source(L("edge_ops.gds"))
    local r = input(60, 0)
    local e = r:edges()

    -- type on result of edges()
    assert_eq(e:count(), 4, "edges count = 4")

    -- selected_interacting on edges
    local ei = e:interacting(r)  -- Edges interacting with Region
    assert_true(ei:count() > 0, "edges:interacting(region) > 0")
end)

-- ============================================================
-- Run all tests
-- ============================================================
print(string.format("\n=== KLayout Migration Tests ===\n"))
for _, t in ipairs(tests) do
    run_test("unnamed", t)
end
print(string.format("\nResults: %d passed, %d failed out of %d\n", passed, failed, passed + failed))
if failed > 0 then os.exit(1) end
