-- DRC script using alm.gds test file
source("testdata/alm.gds")
target("/tmp/alm_output1.gds")  -- sets output path

print("Loaded alm.gds")

-- Read layers (using common layer numbers found in test data)
local l1 = input(1, 0)
local l2 = input(2, 0)
local l3 = input(3, 0)

print("Read layers 1/0, 2/0, 3/0")

-- Write back the input layers
l1:output(1, 0)
l2:output(2, 0)
l3:output(3, 0)
print("Output layers written")

-- Boolean operations
local m12 = l1 & l2
m12:output(10, 0)
print("AND operation done")

local m1_or_2 = l1 | l2
m1_or_2:output(11, 0)
print("OR operation done")

local m1_not_2 = l1 - l2
m1_not_2:output(12, 0)
print("SUBTRACT operation done")

-- Merge
local merged = l1:merge()
merged:output(13, 0)
print("Merge done")

-- Sizing
local wide = l1:sized(0.05)
wide:output(20, 0)
print("Sizing done")

print("All operations completed successfully")

-- Flush output to file
write()
