-- MPI Distributed Scenarios Test Suite
-- Tests tile boundary correctness, halo inference, and scatter/gather integrity.
-- Run with: mpirun -np 5 <build>/src/cli/drc-check --mpi-tiles 2x2 testdata/mpi_scenarios.lua
-- Compare with: <build>/src/cli/drc-check testdata/mpi_scenarios.lua

-- NOTE: ScriptAnalyzer uses line-based parsing.
-- Avoid multiple '=' on a single line in function bodies.

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
-- Test 1: Tile boundary crossing — simple spatial query
-- Layout: tile_boundary_test.gds
--   layer10: big center-crossing rect (-0.5,-0.5)-(0.5,0.5)
--   In 2x2 tiling centered at origin, this crosses all 4 tiles
-- ============================================================
table.insert(tests, function()
    source(L("tile_boundary_test.gds"))
    local c = input(10, 0)
    local corners = input(11, 0)

    assert_eq(c:count(), 1, "center rect count=1")

    local and_c = c & corners
    assert_eq(and_c:count(), 0, "center & corners: no overlap")
    assert_true(and_c:empty(), "center & corners: empty")

    local or_c = c | corners
    assert_eq(or_c:count(), 5, "center | corners: 5 polygons total")
end)

-- ============================================================
-- Test 2: DRC checks on tile-crossing geometry
-- Layout: tile_boundary_test.gds
--   layer12: thin vertical rect (-0.03,-1)-(0.03,1) crosses y=0
--   layer13: thin horizontal rect (-1,-0.03)-(1,0.03) crosses x=0
-- ============================================================
table.insert(tests, function()
    source(L("tile_boundary_test.gds"))
    local v = input(12, 0)
    local h = input(13, 0)

    local wv = v:width(0.1)
    assert_true(wv:count() > 0, "vertical tile-crossing: width(0.1) violations")

    local wv2 = v:width(0.05)
    assert_eq(wv2:count(), 0, "vertical tile-crossing: width(0.05) no violations")

    local wh = h:width(0.1)
    assert_true(wh:count() > 0, "horizontal tile-crossing: width(0.1) violations")
end)

-- ============================================================
-- Test 3: Merge across tile boundaries
-- Layout: tile_boundary_test.gds
-- ============================================================
table.insert(tests, function()
    source(L("tile_boundary_test.gds"))
    local v = input(12, 0)
    local h = input(13, 0)

    local merged_v = v:merge()
    assert_eq(merged_v:count(), 1, "vertical thin rect: merge count=1")

    local merged_h = h:merge()
    assert_eq(merged_h:count(), 1, "horizontal thin rect: merge count=1")
end)

-- ============================================================
-- Test 4: Boolean ops on tile-crossing layers
-- Layout: tile_boundary_test.gds layer12 & layer13 crossing at origin
-- ============================================================
table.insert(tests, function()
    source(L("tile_boundary_test.gds"))
    local v = input(12, 0)
    local h = input(13, 0)

    local and_vh = v & h
    assert_true(and_vh:count() > 0, "vertical & horizontal: intersection exists")

    local or_vh = v | h
    assert_true(or_vh:count() >= 1, "vertical | horizontal: union exists")
    assert_true(or_vh:area() > v:area(), "vertical | horizontal: union area > A area")
    assert_true(or_vh:area() > h:area(), "vertical | horizontal: union area > B area")

    local or_area = or_vh:area()
    local and_area = and_vh:area()
    assert_near(or_area, v:area() + h:area() - and_area, 100, "OR area = A + B - AND (tile-crossing)")
end)

-- ============================================================
-- Test 5: Sizing on tile-crossing geometry
-- Layout: tile_boundary_test.gds layer10: center rect
-- ============================================================
table.insert(tests, function()
    source(L("tile_boundary_test.gds"))
    local c = input(10, 0)

    local bigger = c:sized(0.1)
    assert_eq(bigger:count(), 1, "tile-crossing sized count=1")

    local shrunk = c:sized(-0.1)
    assert_eq(shrunk:count(), 1, "tile-crossing shrunk count=1")
end)

-- ============================================================
-- Test 6: Enclosing with tile-crossing geometry
-- Layout: tile_boundary_test.gds — center rect + corner rects
-- ============================================================
table.insert(tests, function()
    source(L("tile_boundary_test.gds"))
    local c = input(10, 0)
    local corners = input(11, 0)

    local enc = c:enclosing(corners)
    assert_eq(enc:count(), 0, "center does not enclose corner rects")
end)

-- ============================================================
-- Test 7: Edge operations on tile-crossing geometry
-- Layout: tile_boundary_test.gds layer12
-- ============================================================
table.insert(tests, function()
    source(L("tile_boundary_test.gds"))
    local v = input(12, 0)

    local e = v:edges()
    assert_true(e:count() >= 4, "thin rect has >= 4 edges")
    assert_true(e:length() > 0, "edge length > 0")
end)

-- ============================================================
-- Test 8: Chained operations on tile-crossing geometry
-- Layout: tile_boundary_test.gds
-- ============================================================
table.insert(tests, function()
    source(L("tile_boundary_test.gds"))
    local v = input(12, 0)
    local h = input(13, 0)
    local combined = v | h
    local combined_edges = combined:edges()
    local centers = combined_edges:centers(0, 0.5)
    assert_true(centers:count() > 0, "tile-crossing chain: centers count > 0")
    assert_eq(centers:type(), "edges", "tile-crossing chain: centers type=edges")

    local wv = combined:width(0.1)
    assert_true(wv:count() > 0, "tile-crossing width chain: violations found")
end)

-- ============================================================
-- Test 9: Corner detection on tile-crossing polygons
-- Layout: tile_boundary_test.gds layer10 center rect
-- corners_dots runs master-local — accuracy not affected by tiling
-- ============================================================
table.insert(tests, function()
    source(L("tile_boundary_test.gds"))
    local c = input(10, 0)

    local dots = c:corners_dots(-90, -90)
    assert_eq(dots:count(), 4, "tile-crossing square: convex corners=4")

    local all = c:corners_dots(-180, 180)
    assert_eq(all:count(), 4, "tile-crossing square: all corners=4")
end)

-- ============================================================
-- Run all tests
-- ============================================================
print(string.format("\n=== MPI Tile Boundary Scenarios ===\n"))
for _, t in ipairs(tests) do
    run_test("unnamed", t)
end
print(string.format("\nResults: %d passed, %d failed out of %d\n", passed, failed, passed + failed))
if failed > 0 then os.exit(1) end
