
--[[
generate some producer/consumer pairs, for each pair:

Pairs of type 1
producer write to message channel with timed-out secoond=0.052
consumer read from message channel for every 0.1 second

Pairs of type 2
producer write to message channel for every 0.1 second
consumer read from message channel with timed-out secoond=0.052

arg[1]   number of producer/comsumer pairs, defaulted to 100
arg[2]   number of messages for each producer to write, defaulted to 100
]]

local NUM_PAIRS = tonumber(arg[1]) or 100
local NUM_MESSAGES = tonumber(arg[2]) or 100


local tasklet = require 'tasklet.channel.message'
local log = require 'log'

local done1 = 0
local done2 = 0

local ETIMEDOUT = errno.ETIMEDOUT
local assert = assert

local function pair1(id)
	local ch = tasklet.message_channel.new()

	-- comsumer
	tasklet.start_task(function ()
		while true do
			assert(ch:read() == id)
			tasklet.sleep(0.1)
		end
	end)

	-- producer
	tasklet.start_task(function ()
		local failed = 0
		local err
		for i = 1, NUM_MESSAGES do
			err = ch:write(id, 0.052)
			if err ~= 0 then
				assert(err == ETIMEDOUT)
				failed = failed + 1
			end
		end
		local percent = failed / NUM_MESSAGES
		log.info('pair_1[', id, '] ->', tostring(percent))
		assert(percent >= 0.49 and percent <= 0.51, 'failed = ' .. failed)

		done1 = done1 + 1
		if done2 == NUM_PAIRS and done1 == NUM_PAIRS then
			os.exit(0)
		end
	end)
end

local function pair2(id)
	local ch = tasklet.message_channel.new()

	-- producer
	tasklet.start_task(function ()
		while true do
			assert(ch:write(id) == 0)
			tasklet.sleep(0.1)
		end
	end)

	-- consumer
	tasklet.start_task(function ()
		local failed = 0
		local data, err
		for i = 1, NUM_MESSAGES do
			data, err = ch:read(0.052)
			if data then
				assert(data == id)
			else
				assert(err == ETIMEDOUT)
				failed = failed + 1
			end
		end
		local percent = failed / NUM_MESSAGES
		log.info('pair_2[', id, '] ->', tostring(percent))
		assert(percent >= 0.49 and percent <= 0.51, 'failed = ' .. failed)

		done2 = done2 + 1
		if done2 == NUM_PAIRS and done1 == NUM_PAIRS then
			os.exit(0)
		end
	end)
end

for i = 1, NUM_PAIRS do
	pair1(i)
	pair2(i)
end

tasklet.loop()
