local tl = require 'tl'

local stdout = io.stdout
local random = math.random

local function new_task()
	tl.start_task(function ()
		while true do 
			local start = tl.now
			local interval = random() * 7 
			tl.sleep(interval)
			
			local diff = tl.now - start
			if diff > (interval + 0.02) or diff < (interval - 0.02) then
				stdout:write('diff=', diff, ', interval=', interval, '\n')
			end
		end
	end)
end

for i = 1, 50 do 
	new_task(i)
end

tl.loop()

