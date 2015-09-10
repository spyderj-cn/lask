require 'std'

local tasklet = require 'tasklet'

tasklet.init()

local assert = assert
local n_expired = 0

collectgarbage('stop')

local function make_timer(sec)
	tasklet.create_task(function () 
		local num = 0
		while num < 10 do 
			tasklet.sleep(sec)
			num = num + 1
		end
		print(sec, 'exit')

		n_expired = n_expired + 1
		assert(n_expired == math.floor(sec * 10 + 0.1))
	end)
end

--[[
tasklet.create_task(function ()
	while true do 
		collectgarbage('collect')
		print('luamem=', math.floor(collectgarbage('count')))
		print('stdmem=', math.floor(stdmem()))
		tasklet.sleep(30)
	end
end)
]]

print(time.time(), ': started')

for i = 1, 100000 do 
	make_timer(i / 10)
end

tasklet.loop()
