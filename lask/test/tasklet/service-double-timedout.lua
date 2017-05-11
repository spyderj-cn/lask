
--[[
                +---------------+
	x   --->	|               |
	            |double-service |
    2*x <-----  |               |
                +---------------+


arg[1]  1/arg[1] is the timed out value

arg[2]  number of clients, defaulted to 100

arg[3]  number of queries for each client, defaulted to 10000
]]

local tasklet = require 'tasklet.service'

local SEC = 1 / (tonumber(arg[1]) or 100)
local NUM_CLIENTS = tonumber(arg[2]) or 100
local NUM_INPUT = tonumber(arg[3]) or 10000
local ETIMEDOUT = errno.ETIMEDOUT

local assert = assert
local request = tasklet.request

local svc = tasklet.create_service('double', function (svc, x)
    return x * 2
end)

local succeed = 0
local failed = 0
local index = 1

for c = 1, NUM_CLIENTS do
	tasklet.start_task(function ()
		local id = index
        local val, err
        local ts

		index = index + 1

		for i = 1, NUM_INPUT do
			val, err = request(svc, i, SEC)
            if val then
                assert(val == i * 2)
                succeed = succeed + 1
            else
                assert(err == ETIMEDOUT)
                failed = failed + 1
            end
		end

		if id == NUM_CLIENTS then
            print('succeed:', succeed)
            print('failed:', failed)
			assert(succeed + failed == NUM_INPUT * NUM_CLIENTS)
			os.exit(0)
		end
	end)
end

tasklet.loop()
