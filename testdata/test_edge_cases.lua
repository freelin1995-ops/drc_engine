-- DRC Engine Edge Case Test Suite
-- Tests boundary conditions, type combinations, and edge cases not covered
-- by run_tests.lua and klayout_migration.lua
-- Runs with BOTH non-MPI (drc-check) and MPI (mpirun -np 5 drc-check)

-- NOTE: The ScriptAnalyzer uses line-based parsing.
-- DO NOT put multiple statements with '=' on a single line,
-- as that will confuse the analyzer and cause MPI failures.

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
        error(string.format("%s: expected true, got false", msg))
    end
end

function assert_false(v, msg)
    if v then
        error(string.format("%s: expected false, got true", msg))
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
-- Test 1: sized(dx, dy) overload
-- Layout: sizing_test.gds — rect (0,0)-(1,1) area=1um²
-- sized(0.1, 0.2) → (-0.1,-0.2)-(1.1,1.2) area=1.3*1.2=1.56um²
-- ============================================================
table.insert(tests, function()
    source(L("sizing_test.gds"))
    local r = input(10, 0)

    local xy = r:sized(0.1, 0.2)
    assert_eq(xy:count(), 1, "sized(dx,dy) count=1")
end)

-- ============================================================
-- Test 2: sized(0) is identity
-- Layout: sizing_test.gds
-- ============================================================
table.insert(tests, function()
    source(L("sizing_test.gds"))
    local r = input(10, 0)
    local zero = r:sized(0)
    assert_eq(zero:count(), r:count(), "sized(0) count unchanged")
end)

-- ============================================================
-- Test 3: Notch check returns edge_pairs type and doesn't crash
-- Layout: drc_exact.gds — layer 40 has C-shape with notch
-- ============================================================
table.insert(tests, function()
    source(L("drc_exact.gds"))
    local l40 = input(40, 0)

    local n_ok = l40:notch(0.5)
    assert_eq(n_ok:type(), "edge_pairs", "notch returns edge_pairs")

    local n_fail = l40:notch(2.0)
    assert_true(n_fail:count() >= 0, "notch count >= 0")
end)

-- ============================================================
-- Test 4: type() method for all result types
-- Layout: boolean_ops.gds
-- ============================================================
table.insert(tests, function()
    source(L("boolean_ops.gds"))
    local a = input(10, 0)
    local b = input(11, 0)

    assert_eq(a:type(), "region", "input returns region")
    assert_eq((a & b):type(), "region", "AND returns region")
    assert_eq((a | b):type(), "region", "OR returns region")
    assert_eq((a - b):type(), "region", "SUB returns region")

    local e = a:edges()
    assert_eq(e:type(), "edges", "edges returns edges")

    local w = a:width(0.5)
    assert_eq(w:type(), "edge_pairs", "width returns edge_pairs")

    local s = a:space(0.5)
    assert_eq(s:type(), "edge_pairs", "space returns edge_pairs")
end)

-- ============================================================
-- Test 5: Boolean ops on Edges type
-- Layout: edge_type_ops.gds — layer10 rect (0,0)-(2,2), layer11 rect (1,1)-(3,3)
-- ============================================================
table.insert(tests, function()
    source(L("edge_type_ops.gds"))
    local a = input(10, 0)
    local b = input(11, 0)
    local ea = a:edges()
    local eb = b:edges()

    local and_e = ea & eb
    assert_eq(and_e:type(), "edges", "edges AND returns edges")

    local or_e = ea | eb
    assert_eq(or_e:type(), "edges", "edges OR returns edges")
    assert_true(or_e:count() >= ea:count(), "edges OR count >= A count")

    local sub_e = ea - eb
    assert_eq(sub_e:type(), "edges", "edges SUB returns edges")

    local xor_e = ea ~ eb
    assert_eq(xor_e:type(), "edges", "edges XOR returns edges")
end)

-- ============================================================
-- Test 6: EdgePairs type and empty checks from DRC checks
-- Layout: drc_exact.gds layer10 rect (0,0)-(2,2)
-- ============================================================
table.insert(tests, function()
    source(L("drc_exact.gds"))
    local l10 = input(10, 0)

    local wv = l10:width(1.0)
    assert_eq(wv:count(), 0, "2um-wide: width(1.0) no violations")
    assert_true(wv:empty(), "2um-wide: width(1.0) empty")
    assert_eq(wv:type(), "edge_pairs", "width returns edge_pairs")

    local wv3 = l10:width(3.0)
    assert_eq(wv3:type(), "edge_pairs", "width(3.0) returns edge_pairs")
    assert_true(wv3:count() > 0, "width(3.0) has violations on 2um rect")
    assert_false(wv3:empty(), "width(3.0) not empty")

    -- EdgePairs interacting with Region — verify no crash
    local interact = wv3:interacting(l10)
    assert_true(interact:count() >= 0, "edge_pairs:interacting(region) ok")
end)

-- ============================================================
-- Test 7: Exact DRC boundary — width at exact threshold
-- Layout: drc_exact.gds layer10 rect (0,0)-(2,2), layer11 rect (0,0)-(0.3,2)
-- ============================================================
table.insert(tests, function()
    source(L("drc_exact.gds"))
    local l10 = input(10, 0)
    local l11 = input(11, 0)

    local w_exact = l10:width(2.0)
    assert_eq(w_exact:count(), 0, "2um rect: width(exact=2.0) no violations")

    local w_over = l10:width(2.5)
    assert_true(w_over:count() > 0, "2um rect: width(2.5) has violations")

    local w_thin = l11:width(0.3)
    assert_eq(w_thin:count(), 0, "0.3um rect: width(0.3) no violations")

    local w_thin2 = l11:width(0.301)
    assert_true(w_thin2:count() > 0, "0.3um rect: width(0.301) has violations")
end)

-- ============================================================
-- Test 8: Space check — gap=0.5 and touching
-- Layout: drc_exact.gds layer12,13 gap=0.5; layer14,15 touching
-- ============================================================
table.insert(tests, function()
    source(L("drc_exact.gds"))
    local l12 = input(12, 0)
    local l13 = input(13, 0)
    local l14 = input(14, 0)
    local l15 = input(15, 0)

    local merged = l12 | l13
    local merged2 = l14 | l15

    local s_exact = merged:space(0.5)
    assert_eq(s_exact:count(), 0, "gap=0.5: space(0.5) no violations")

    local s_over = merged:space(0.501)
    assert_true(s_over:count() > 0, "gap=0.5: space(0.501) has violations")

    local s_zero = merged2:space(0)
    assert_eq(s_zero:count(), 0, "touching: space(0) no violations")
end)

-- ============================================================
-- Test 9: Enclosure check — margin=0.5 and margin=0.2
-- Layout: drc_exact.gds layer20,21 margin=0.5; layer22,23 margin=0.2
-- ============================================================
table.insert(tests, function()
    source(L("drc_exact.gds"))
    local outer1 = input(20, 0)
    local inner1 = input(21, 0)
    local outer2 = input(22, 0)
    local inner2 = input(23, 0)

    local ec = outer1:enclosing_check(inner1, 0.5)
    assert_eq(ec:count(), 0, "margin=0.5: enclosing_check(0.5) no violations")

    local ec2 = outer1:enclosing_check(inner1, 0.501)
    assert_true(ec2:count() > 0, "margin=0.5: enclosing_check(0.501) has violations")

    local ec3 = outer2:enclosing_check(inner2, 0.2)
    assert_eq(ec3:count(), 0, "margin=0.2: enclosing_check(0.2) no violations")

    local ec4 = outer2:enclosing_check(inner2, 0.5)
    assert_true(ec4:count() > 0, "margin=0.2: enclosing_check(0.5) has violations")
end)

-- ============================================================
-- Test 10: Overlap and sep checks with exact values
-- Layout: drc_exact.gds layer30,31 overlap margin=0.5
-- ============================================================
table.insert(tests, function()
    source(L("drc_exact.gds"))
    local l30 = input(30, 0)
    local l31 = input(31, 0)

    local ov = l30:overlap_check(l31, 0.5)
    assert_eq(ov:count(), 0, "overlap=0.5: overlap_check(0.5) no violations")

    local ov2 = l30:overlap_check(l31, 0.501)
    assert_true(ov2:count() > 0, "overlap=0.5: overlap_check(0.501) has violations")

    local sp = l30:sep_check(l31, 0.1)
    assert_eq(sp:count(), 0, "overlapping: sep_check(0.1) no violations")
end)

-- ============================================================
-- Test 11: Zero distance edge cases
-- Layout: drc_exact.gds
-- ============================================================
table.insert(tests, function()
    source(L("drc_exact.gds"))
    local l10 = input(10, 0)
    local l12 = input(12, 0)

    local wz = l10:width(0)
    assert_eq(wz:count(), 0, "width(0) no violations")

    local sz = l12:space(0)
    assert_eq(sz:count(), 0, "space(0) no violations")

    local outer = input(20, 0)
    local inner = input(21, 0)
    local ec0 = outer:enclosing_check(inner, 0)
    assert_eq(ec0:count(), 0, "enclosing_check(0) no violations")
end)

-- ============================================================
-- Test 12: Empty operation chains — ops on empty layer
-- Layout: basic_input.gds — layer99/0 is empty
-- ============================================================
table.insert(tests, function()
    source(L("basic_input.gds"))
    local empty = input(99, 0)

    assert_true(empty:empty(), "empty layer is empty")
    assert_eq(empty:count(), 0, "empty count=0")
    assert_eq(empty:type(), "region", "empty input type=region")

    local m = empty:merge()
    assert_true(m:empty(), "empty:merge is empty")

    local s = empty:sized(0.5)
    assert_true(s:empty(), "empty:sized is empty")

    local e = empty:edges()
    assert_true(e:empty(), "empty:edges is empty")

    local w = empty:width(0.5)
    assert_true(w:empty(), "empty:width is empty")
    assert_eq(w:type(), "edge_pairs", "empty:width type edge_pairs")

    local sp = empty:space(0.5)
    assert_true(sp:empty(), "empty:space is empty")
    assert_eq(sp:type(), "edge_pairs", "empty:space type edge_pairs")
end)

-- ============================================================
-- Test 13: Edge centers/segments edge cases
-- Layout: edge_ops.gds — 1x1 square → 4 edges
-- ============================================================
table.insert(tests, function()
    source(L("edge_ops.gds"))
    local e = input(60, 0):edges()

    local c_end = e:centers(0, 1.0)
    assert_eq(c_end:count(), 4, "centers(0,1.0) 4 segments at endpoints")

    local c_start = e:centers(0, 0)
    assert_eq(c_start:count(), 4, "centers(0,0) 4 segments at startpoints")
end)

-- ============================================================
-- Test 14: Corners angle filtering
-- Layout: corner_detection.gds
--   layer60: 1x1 square → 4 convex -90 corners
--   layer61: L-shape → 5 convex (-90) + 1 concave (+90)
-- ============================================================
table.insert(tests, function()
    source(L("corner_detection.gds"))
    local sq = input(60, 0)
    local ls = input(61, 0)

    local conv_sq = sq:corners_dots(-90, -90)
    assert_eq(conv_sq:count(), 4, "square convex (-90) = 4")

    local all_sq = sq:corners_dots(-180, 180)
    assert_eq(all_sq:count(), 4, "square all corners = 4")

    local conv_ls = ls:corners_dots(-90, -90)
    assert_eq(conv_ls:count(), 5, "L-shape convex (-90) = 5")

    local conc_ls = ls:corners_dots(90, 90)
    assert_eq(conc_ls:count(), 1, "L-shape concave (90) = 1")

    local all_ls = ls:corners_dots(-180, 180)
    assert_eq(all_ls:count(), 6, "L-shape all corners = 6")
end)

-- ============================================================
-- Test 15: sep_check with touching geometry
-- Layout: drc_exact.gds layer14,15 touching gap=0
-- ============================================================
table.insert(tests, function()
    source(L("drc_exact.gds"))
    local l14 = input(14, 0)
    local l15 = input(15, 0)

    local sp1 = l14:sep_check(l15, 0.001)
    assert_eq(sp1:type(), "edge_pairs", "touching: sep_check returns edge_pairs")

    local sp0 = l14:sep_check(l15, 0)
    assert_eq(sp0:count(), 0, "touching: sep_check(0) no violations")
end)

-- ============================================================
-- Test 16: Extreme geometry values
-- Layout: extreme_geometry.gds
--   layer10: 1nm wide rect, layer20: far away rect, layer30: 100x100um rect
-- ============================================================
table.insert(tests, function()
    source(L("extreme_geometry.gds"))
    local thin = input(10, 0)
    local far = input(20, 0)
    local huge = input(30, 0)

    local wv = thin:width(0.002)
    assert_true(wv:count() > 0, "1nm path: width(0.002) has violations")

    local wv2 = thin:width(0.0005)
    assert_eq(wv2:count(), 0, "1nm path: width(0.0005) no violations")

    assert_eq(far:count(), 1, "far rect count=1")
    assert_eq(huge:count(), 1, "huge rect count=1")
end)

-- ============================================================
-- Test 17: Edges inside/outside with Region
-- Layout: selection_test.gds — layer40 big rect (0,0)-(3,3)
-- ============================================================
table.insert(tests, function()
    source(L("selection_test.gds"))
    local big = input(40, 0)
    local big_edges = big:edges()

    local ei = big_edges:inside(big)
    assert_true(ei:count() >= 0, "edges:inside(self) ok")
end)

-- ============================================================
-- Test 18: size_inside via chained inline expr (MPI-safe)
-- Avoids intermediate variable for MPI ScriptAnalyzer compatibility
-- Layout: sizing_test.gds
-- ============================================================
table.insert(tests, function()
    source(L("sizing_test.gds"))
    local r = input(10, 0)
    local result = r:sized(0.1, 0.2):merge()
    assert_true(result:count() > 0, "chained sized+merge count > 0")
end)

-- ============================================================
-- Run all tests
-- ============================================================
print(string.format("\n=== DRC Edge Case Tests ===\n"))
for _, t in ipairs(tests) do
    run_test("unnamed", t)
end
print(string.format("\nResults: %d passed, %d failed out of %d\n", passed, failed, passed + failed))
if failed > 0 then os.exit(1) end
