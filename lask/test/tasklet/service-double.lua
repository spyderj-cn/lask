
--[[
                +---------------+
	x   --->	|               |
	            |double-service |
    2*x <-----  |               |
                +---------------+


arg[1]  number of clients, defaulted to 100
arg[2]  number of queries for each client, defaulted to 10000
]]

local tasklet = require 'tasklet.service'

local NUM_CLIENTS = tonumber(arg[1]) or 1000
local NUM_REQUESTS = tonumber(arg[2]) or 1000

local assert = assert
local request = tasklet.request

local svc = tasklet.create_service('double', function (svc, x) return x * 2 end)

local total = 0
local index = 1

for c = 1, NUM_CLIENTS do
	tasklet.start_task(function ()
		local id = index
		index = index + 1
		for i = 1, NUM_REQUESTS do
			assert(request(svc, i) == 2 * i)
			total = total + 1
		end
		if id == NUM_CLIENTS then
			assert(total == NUM_REQUESTS * NUM_CLIENTS)
			os.exit(0)
		end
	end)
end

tasklet.loop()
