local fs = require "fs"
local process = require "process" .globalProcess()
local mp = require "./msgpack"

local bin = fs.readFileSync(process.argv[2])
local _, table = mp.unpack(bin)

p(table)

-- for i, row in ipairs(table) do
--     if type(row) == "table" then
--         print("row("..i.."):")
--         for j, col in ipairs(row) do
--             print("  col("..j.."): "..col)
--         end
--     else
--         print("row("..i.."): "..tostring(row))
--     end
-- end
