
local tasklet = require 'tasklet.channel.stream'
local log = require 'log'
local http = require 'http'

local MISSIONS = {
	['www.qq.com'] = '/',
	['www.sina.com.cn'] = '/',
}
local req = http.new_request()
local resp = http.new_response() 
local buf = buffer.new()

local function get(server, path)
	local ip = netdb.getaddrbyname(server)[1] or log.fatal('DNS error')
	print(server, '->', ip)

	local ch = tasklet.create_stream_channel()
	if ch:connect(ip, 80) == 0 then
		local tm_start = tasklet.now
		
		req.host = server
		req.urlpath = path
		req:serialize(nil, buf)
		ch:write(buf)
		buf:rewind()
		
		if resp:read(ch, 30) == 0 then
			log.info('succeed: ', resp.status, ' ', tostring(resp.content))
		else
			log.error('failed')
		end
		ch:close()
	end	
end

tasklet.start_task(function ()
	for server, path in pairs(MISSIONS) do
		print('\n\n-------------------------------------------------------')
		print('MISSION: ', 'http://' .. server .. path)
		print('-------------------------------------------------------')
		local time_progress = tasklet.start_task(function ()
			local total = 0;
			while true do
				tasklet.sleep(0.1)
				total = total + 0.1
				print(total, 'seconds ...')
			end
		end)
		get(server, path)
		tasklet.reap_task(time_progress)
	end
	tasklet.quit()
end)

tasklet.loop()

