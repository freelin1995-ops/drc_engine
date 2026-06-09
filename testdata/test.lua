source("testdata/layouts/klayout_width.gds")
target("/tmp/example_output1.gds")

local a = input(1, 0)

local w_small = a:width(1.0)
w_small:output(2, 0)

local w_medium = a:width(1.5)
w_medium:output(3, 0)

write()