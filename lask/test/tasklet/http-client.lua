
--[[

arg[1]  https/http, defaulted to http
]]

local tasklet
local log = require 'log'
local http = require 'http'

local HTTP_MISSIONS = {
	['www.qq.com'] = '/',
	['www.sina.com.cn'] = '/',
}

local HTTPS_MISSIONS = {
	['github.com'] = '/',
	['mail.aliyun.com'] = '/alimail/auth/login',
	['www.baidu.com'] = '/',
}

local missions, channel_type, port
if arg[1] == 'https' or arg[1] == 'ssl' then
	local ssl = require 'ssl'
	tasklet = require 'tasklet.channel.sslstream'
	tasklet.sslstream_channel.ctx = ssl.context.new('sslv23') or log.fatal('failed to create a ssl context')
	missions = HTTPS_MISSIONS
	channel_type = tasklet.sslstream_channel
	port = 443
else
	tasklet = require 'tasklet.channel.stream'
	missions = HTTP_MISSIONS
	channel_type = tasklet.stream_channel
	port = 80
end

local req = http.request.new()
local resp = http.response.new()
local buf = buffer.new()

local function get(server, path)
	local ip = netdb.getaddrbyname(server)[1] or log.fatal('DNS error')
	print(server, '->', ip)

	local ch = channel_type.new()
	if ch:connect(ip, port) == 0 then
		local tm_start = tasklet.now

		req.host = server
		req.urlpath = path
		req:serialize(nil, buf)
		ch:write(buf)
		buf:rewind()

		local err = resp:read(ch, 30)
		if err == 0 then
			log.info('succeed: ', resp.status, ' ', tostring(resp.content))
		else
			log.error('failed, err -> ', err) 
		end
		ch:close()
	end
end

tasklet.start_task(function ()
	for server, path in pairs(missions) do
		print('\n\n-------------------------------------------------------')
		print('MISSION: ', arg[1] or 'http', '://' .. server .. path)
		print('-------------------------------------------------------')
		local time_progress = tasklet.start_task(function ()
			local total = 0;
			while true do
				tasklet.sleep(0.1)
				total = total + 0.1
	--			print(total, 'seconds ...')
			end
		end)
		get(server, path)
		tasklet.reap_task(time_progress)
	end
	os.exit(0)
end)

tasklet.loop()
