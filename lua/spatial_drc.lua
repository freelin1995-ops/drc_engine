-- Spatial DRC operations example
-- Demonstrates enclosing, interacting, inside, outside, and DRC check methods
--
-- Usage: drc-check lua/spatial_drc.lua

source("testdata/test_drc.gds")
target("/tmp/spatial_output.gds")

print("=== Spatial Operations Demo ===")
print()

-- ========================
-- Enclosure check example
-- Layer 20: outer rect (0,0)-(2,2)
-- Layer 21: inner rect (0.5,0.5)-(1.5,1.5)
-- Enclosure margin = 0.5 on each side
-- ========================
print("--- Enclosure Check ---")
local outer = input(20, 0)
local inner = input(21, 0)

print("Outer: " .. outer:count() .. " polygon(s), area=" .. outer:area())
print("Inner: " .. inner:count() .. " polygon(s), area=" .. inner:area())

-- enclosure_check(self, other, d): violations where self encloses other by < d
-- Margin is 0.5, so d=0.4 should pass, d=0.6 should fail
local enc_ok  = outer:enclosing_check(inner, 0.4)
local enc_bad = outer:enclosing_check(inner, 0.6)
print("Enclosure check d=0.4 (margin OK):      " .. enc_ok:count() .. " violations")
print("Enclosure check d=0.6 (margin too small): " .. enc_bad:count() .. " violations")
enc_bad:output(200, 0)

-- ========================
-- Interacting / Inside / Outside / Enclosing
-- Layer 40: big rect (0,0)-(3,3)
-- Layer 41: 3 rects: inside, touching, far
-- ========================
print()
print("--- Spatial Relations ---")
local big   = input(40, 0)
local small = input(41, 0)

print("Big:   1 polygon")
print("Small: " .. small:count() .. " polygons (inside + touching + far)")

local inside_polys   = small:inside(big)
local outside_polys  = small:outside(big)
local interacting_s  = small:interacting(big)
local interacting_b  = big:interacting(small)
local enclosing_poly = big:enclosing(small)

print("small:inside(big):       " .. inside_polys:count() .. " (fully inside)")
print("small:outside(big):      " .. outside_polys:count() .. " (not fully inside)")
print("small:interacting(big):  " .. interacting_s:count() .. " (touch or overlap)")
print("big:interacting(small):  " .. interacting_b:count() .. " (big poly interacting)")
print("big:enclosing(small):    " .. enclosing_poly:count() .. " (big poly that encloses)")

inside_polys:output(41, 0)
outside_polys:output(42, 0)

-- ========================
-- Boolean operations
-- Layer 30: rect A (0,0)-(2,2)
-- Layer 31: rect B (1,1)-(3,3)
-- ========================
print()
print("--- Boolean Operations ---")
local a = input(30, 0)
local b = input(31, 0)

local and_op = a & b
local or_op  = a | b
local sub_op = a - b
local xor_op = a ~ b

print("A area: " .. a:area())
print("B area: " .. b:area())
print("A & B:  " .. and_op:count() .. " poly(s), area=" .. and_op:area())
print("A | B:  " .. or_op:count() ..  " poly(s), area=" .. or_op:area())
print("A - B:  " .. sub_op:count() .. " poly(s), area=" .. sub_op:area())
print("A ~ B:  " .. xor_op:count() .. " poly(s), area=" .. xor_op:area())

and_op:output(31, 0)
or_op:output(32, 0)
sub_op:output(33, 0)
xor_op:output(34, 0)

-- ========================
-- Width / Space checks
-- Layer 10: 5 fingers, widths 0.05..0.25, spaces 0.05..0.25
-- ========================
print()
print("--- DRC Checks ---")
local metal = input(10, 0)

local w0 = metal:width(0.04)
local w1 = metal:width(0.10)
local w2 = metal:width(0.20)

local s0 = metal:space(0.04)
local s1 = metal:space(0.10)

print("Width check  d=0.04: " .. w0:count() .. " violations")
print("Width check  d=0.10: " .. w1:count() .. " violations")
print("Width check  d=0.20: " .. w2:count() .. " violations")
print("Space check  d=0.04: " .. s0:count() .. " violations")
print("Space check  d=0.10: " .. s1:count() .. " violations")

w1:output(100, 0)
s1:output(101, 0)

-- ========================
-- Sizing
-- Layer 60: rect 1x1 um
-- ========================
print()
print("--- Sizing ---")
local r = input(60, 0)
local grown = r:sized(0.1)
local shrunk = r:sized(-0.05)
print("Original area: " .. r:area())
print("Sized +0.1:   " .. grown:count() .. " poly(s), area=" .. grown:area())
print("Sized -0.05:  " .. shrunk:count() .. " poly(s), area=" .. shrunk:area())

r:output(60, 0)
grown:output(61, 0)
shrunk:output(62, 0)

-- ========================
-- Area filter
-- ========================
print()
print("--- Area Filter ---")
print("Metal layer: " .. metal:count() .. " polygons total")
local filtered = metal:with_area(0.10, 0.20)
print("Polygons with area 0.10-0.20 um2: " .. filtered:count())

print()
print("Writing output to /tmp/spatial_output.gds")

write()
