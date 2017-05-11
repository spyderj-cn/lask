
--[[

input -->  NODE1  --> NODE2  --> .... --> NODE_N    -> output
            +1         +2                   +N


arg[1]  number of nodes, defaulted to 100
arg[2]  number of data, defaulted to 10000
arg[3]  capacity of each channel, defaulted to 0
]]

local tasklet = require 'tasklet.channel.message'
local log = require 'log'

local NUM_NODE = tonumber(arg[1]) or 100
local NUM_DATA = tonumber(arg[2]) or 10000
local NUM_CAP = tonumber(arg[3]) or 0
local DATA_SUM = (1 + NUM_NODE) * NUM_NODE / 2

local nodes = table.array(NUM_NODE)
local ch_array = table.array(NUM_NODE + 1)

local index = 1

local function node_task()
    local id = index
    index = index + 1
    local ch_src = ch_array[id]
    local ch_dst = ch_array[id + 1]
    while true do
        local data = ch_src:read()
        ch_dst:write(data + id)
    end
end

for i = 1, NUM_NODE do
    ch_array[i] = tasklet.message_channel.new(NUM_CAP)
    nodes[i] = tasklet.start_task(node_task)
end
ch_array[NUM_NODE + 1] = tasklet.message_channel.new(NUM_CAP)

tasklet.start_task(function ()
    local ch = ch_array[1]
    for i = 1, NUM_DATA do
        ch:write(0)
    end
end)

tasklet.start_task(function ()
    local ch = ch_array[NUM_NODE + 1]
    local assert = assert
    for i = 1, NUM_DATA do
        assert(ch:read() == DATA_SUM)
    end
    os.exit(0)
end)

tasklet.loop()
