
require 'std'
local ssl = require 'ssl'
local tasklet = require 'tasklet.channel.sslstream'
local log = require 'log'

local MULTIPLE = tonumber(arg[1]) or 1000000
local PORT = tonumber(arg[2]) or 60001

local conf = {
	protocol = 'sslv23',
	certfile = 'serverA.pem',
	keyfile = 'serverAkey.pem',
	cafile = 'rootA.pem'
}

local ctx = ssl.context.new(conf.protocol)
if ctx:load_verify_locations(conf.cafile) ~= 0 then
	log.fatal('ssl.context.load_verify_locations failed')
end
if ctx:use_certfile(conf.certfile) ~= 0 then
	log.fatal('ssl.context.use_certfile failed')
end
if ctx:use_keyfile(conf.keyfile, 'foobar') ~= 0 then
	log.fatal('ssl.context.use_keyfile failed')
end

local server_fd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
if socket.bind(server_fd, '0.0.0.0', PORT) ~= 0 then
	log.fatal('socket.bind failed')
end
tasklet.sslstream_channel.ctx = ctx
socket.listen(server_fd)
tasklet.add_handler(server_fd, tasklet.EVT_READ, function ()
	local fd, addr, port = socket.accept(server_fd)
	if fd >= 0 then
		log.info('connection established with ', addr, ':', port)
		tasklet.start_task(function ()
			local task = tasklet.current_task()
			local ch = tasklet.create_sslstream_channel(fd)
			if ch:handshake(-1) == 0 then
				local buf = buffer.new()
				while true do 
					local line = ch:read()
					if not line then
						break
					end
					
					local nreq = #line
					local nresp = MULTIPLE * nreq
					local left = MULTIPLE
					log.info('received ', nreq, ' bytes, ', 'reply ', nresp, ' bytes')
					
					local time_progress = tasklet.start_task(function ()
						local total = 0
						while true do 
							tasklet.sleep(0.1)
							total = total + 0.1
							print(total, 'seconds ...')
						end
					end)
					
					while left > 0 do 
						local num = left
						if num > 1000 then
							num = 1000
						end
						left = left - num
						
						for i = 1, num do 
							buf:putstr(line)
						end
						local err = ch:write(buf)
						if err ~= 0 then
							log.fatal('ch:write() failed: errno -> ', errno.strerror(err))
						end
						io.stdout:write(string.format('\r(%d/%d)', nresp - left * nreq, nresp))
						buf:rewind()
					end
					print('')
					log.info('succeed')
					tasklet.reap_task(time_progress)
				end
			end
			ch:close()
			log.info('connection off with ', addr, ':', port)
		end)
	end
end)

tasklet.loop()
