local tl = require 'tasklet.service'

local assert = assert
local serve = tl.serve
local request = tl.request

local svc = tl.create_service('double', function (svc, x) return x * 2 end)

tl.start_task(function ()
	local total = 0
	local count = 0
	while true do
		assert(request(svc, count) == 2 * count) 
		count = count + 1
		if count == 10000 then
			count = 0
			total = total + 10000
			print(total)
		end
	end
end)

tl.loop()

