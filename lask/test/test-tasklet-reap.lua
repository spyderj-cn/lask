
-- create masses of tasks then gradually reap them and show the memory cost

require 'std'

local tl = require 'tasklet'

local tasks = {}

for i = 1, 10000 do 
	tasks[i] = tl.start_task(function ()
		while true do 
			tl.sleep(1)
		end
	end)
end

print(collectgarbage('count'))

tl.start_task(function ()
	local cursor = 0
	while cursor < 10000 do 
		tl.sleep(1)
		for i = 1,  100 do 
			tl.reap_task(tasks[i + cursor])
			tasks[i + cursor] = false
		end
		cursor = cursor + 100
		collectgarbage('collect')
		print(collectgarbage('count'))
	end
end)

tl.loop()

