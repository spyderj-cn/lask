
local tasklet = require 'tasklet.channel.stream'
local log = require 'log'

local MULTIPLE = tonumber(arg[1]) or 1000000
local PORT = tonumber(arg[2]) or 60001

io.stdin:setvbuf('no')

local listen_fd = socket.tcpserver('0.0.0.0', PORT)
if listen_fd < 0 then 
	log.fatal('unable to bind at port ', PORT)
end

tasklet.add_handler(listen_fd, tasklet.EVT_READ, function ()
	local fd, addr, port = socket.accept(listen_fd)
	if fd >= 0 then
		log.info('connection established with ', addr, ':', port)
		tasklet.start_task(function ()
			local ch = tasklet.create_stream_channel(fd)
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
			ch:close()
		end)
	end
end)

tasklet.loop()
