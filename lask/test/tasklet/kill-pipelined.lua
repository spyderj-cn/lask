
local help = [[
                      msg(int)         +1            +2
	task 'source'    ----->   task1   ----->  task2  ----->    ...  -> taskN
	    A                                                                 |
		|                                                                 |
		+-----------------------------------------------------------------+
                               +N

arg[1] number of tasks, defaulted to 100
arg[2] number of messages, defaulted to 10000
]]
if arg[1] == 'help' then
	print(help)
	os.exit(0)
end

local tasklet = require 'tasklet'

local NUM_NODES = tonumber(arg[1]) or 100
local NUM_MESSAGES = tonumber(arg[2]) or 10000

local SUM = (NUM_NODES + 1) * NUM_NODES / 2

local nodes = table.array(NUM_NODES + 1)
local messages = table.array(NUM_MESSAGES)

local assert = assert
local EINTR = errno.EINTR

local function general_task_entry()
	while true do
		assert(tasklet.sleep() == EINTR)
	end
end

local function make_node(id)
	return tasklet.start_task(general_task_entry, {
		sighandler = function (task, sig)
			tasklet.kill_task(nodes[id + 1], sig + id)
		end,
	})
end

for i = 1, NUM_NODES do
	nodes[i] = make_node(i)
end

-- task 'source'
local idx = 1
nodes[NUM_NODES + 1] = tasklet.start_task(function ()
	local rand = math.random
	while idx <= NUM_MESSAGES do
		local msg = rand(1, 100)
		messages[idx] = msg
		tasklet.kill_task(nodes[1], msg)
		assert(tasklet.sleep() == EINTR)
		idx = idx + 1
	end
	os.exit(0)
end, {
	sighandler = function (task, sig)
		assert(sig == messages[idx] + SUM)
	end
})

tasklet.loop()
