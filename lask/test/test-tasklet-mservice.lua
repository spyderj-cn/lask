local tl = require 'tasklet.service'

tl.init()

local count = 0
local total = 0

local svc = tl.create_multi_service('sum', tl.start_task(function ()
	local svc = tl.find_service('sum')
	svc.handler = function (req, resp, err, num)
		for i = 1, num do
			resp[i] = req[i] * 2
		end

		count = count + 1
		if count == 10000 then
			total = total + count
			count = 0
			print(total)
		end
	end
	
	while true do
		tl.multi_serve(svc, 100, 3)
	end
end))

local assert = assert

for i = 1, 1000 do 
tl.start_task(function ()
	while true do 
		assert(tl.request(svc, 1, 5) == 2)
	end
end)
end

--[[
tl.start_task(function ()
	local i = 100
	while true do 
		print('send', i, 'receive', svc_sum:request(i, 5))
		tl.sleep(1)
		i = i + 1
	end
end)
--]]
tl.loop()

