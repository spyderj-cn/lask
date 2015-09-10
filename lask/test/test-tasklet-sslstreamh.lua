
-- 

local tasklet = require 'tasklet.channel.sslstream'
local log = require 'log'
local http = require 'http'
local ssl = require 'ssl'

local MISSIONS = {
	['github.com'] = '/',
	['mail.aliyun.com'] = '/alimail/auth/login',
	['www.baidu.com'] = '/',
}
local ctx = ssl.context.new('sslv23') or log.fatal('failed to create a ssl context')

tasklet.sslstream_channel.ctx = ctx

local req = http.request.new()
local resp = http.response.new() 
local buf = buffer.new()

local function get(server, path)
	local ip = netdb.getaddrbyname(server)[1] or log.fatal('DNS error')
	print(server, '->', ip)

	local ch = tasklet.create_sslstream_channel()
	if ch:connect(ip, 443) == 0 then
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
		print('MISSION: ', 'https://' .. server .. path)
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
