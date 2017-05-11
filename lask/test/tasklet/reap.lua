
--[[
create masses of tasks then gradually reap them and show the memory cost

arg[1]   number of tasks to create, defaulted to 10000
 ]]


local tasklet = require 'tasklet'

local NUM_TASKS = tonumber(arg[1]) or 10000
NUM_TASKS = math.floor((NUM_TASKS + 99) / 100) * 100

local tasks = table.array(NUM_TASKS)
local count = 0

local org_memcost = collectgarbage('count')
local top_memcost

print('org_memcost -> ', org_memcost)

for i = 1, NUM_TASKS do
	tasks[i] = tasklet.start_task(function ()
		while true do
			tasklet.sleep(0.2)
			count = count + 1
		end
	end)
end

tasklet.start_task(function ()
	local cursor = 0
	local memcost

	tasklet.sleep(1)
	top_memcost = collectgarbage('count')
	print('top_memcost -> ', top_memcost)

	while cursor < NUM_TASKS do
		for i = 1,  100 do
			tasklet.reap_task(tasks[i + cursor])
			tasks[i + cursor] = false
		end
		cursor = cursor + 100
		collectgarbage('collect')
		memcost = collectgarbage('count')
		print('memcost -> ', memcost)
		print('count -> ', count)
		count = 0
		tasklet.sleep(1)
	end
	tasklet.quit()
end)



tasklet.loop()
