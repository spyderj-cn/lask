local redis = require 'aredis'
local tl = require 'tasklet'

rclient = redis.connect()

--[[
tl.start_task(function ()
	local ch = tl.create_stream_channel(0)
	while true do 
		local line, err = ch:read()
		if not line then
			tl.quit()
		end

		local arr = line:tokenize(' ')
		if #arr > 0 then
			local cmd = arr[1]
			local argv = table.remove(arr, 1)
			local retv = rclient[cmd](unpack(argv))
			dump(retv)
		end
	end
end)
--]]

local assert = assert
local svc_req = tl.request
---[[
tl.create_task(function ()
	local total = 0
	local num = 0
	local argv = {'set', 'a', '1'}
	while true do 
		svc_req(rclient, argv)
		num = num + 1
		if num == 10000 then
			total = total + 10000
			print(total)
			num = 0
		end
	end
end)
--]]

tl.create_task(function ()
	while true do 
		tl.sleep(1)
		print('dida')
	end
end)

tl.loop()

