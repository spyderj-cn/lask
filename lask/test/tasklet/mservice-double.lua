
--[[
    x        +------------------+
   --->      |                  |
   <---      |                  |
    2*x      |      DOUBLE      |
	         |                  |
   ...       |                  |
             +------------------+


arg[1]  number of clients, defaulted to 1000
arg[2]  number of requests for each client, defaulted to 1000
arg[3]  max request collected before timed-out for the service, defaulted to 100
]]

local NUM_CLIENTS = tonumber(arg[1]) or 1000
local NUM_REQUESTS = tonumber(arg[2]) or 1000
local MAX_COLLECTED = tonumber(arg[3]) or 100

local tasklet = require 'tasklet.service'

local total = 0

local svc = tasklet.create_multi_service(
	'double',
	function (req, resp, err, num)
		for i = 1, num do
			resp[i] = req[i] * 2
		end
		total = total + num
	end,
	MAX_COLLECTED,
	3)

local assert = assert
local done = 0

for i = 1, NUM_CLIENTS do
	tasklet.start_task(function ()
		for j = 1, NUM_REQUESTS do
			assert(tasklet.request(svc, 1) == 2)
		end
		done = done + 1
		if done == NUM_CLIENTS then
			assert(total == NUM_CLIENTS * NUM_REQUESTS)
			os.exit(0)
		end
	end)
end

tasklet.loop()
