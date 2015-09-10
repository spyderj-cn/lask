
require 'std'

local as_server = not arg[1] or arg[1] == 'server'

if as_server then
	local tasklet = require 'tasklet.channel.server'
	local tcp_server = tl.create_tcpserver_channel('0.0.0.0', 65432)
	local un_server = tl.create_unserver_channel('/tmp/test-tasklet-server.sock')

	tasklet.start_task(function ()
		while true do 
			local fd, addr, port = ch_server:accept()
			if fd >= 0 then
				io.stdout:write('connection established with ', addr, ':',  port, '\n')
				os.close(fd)
			end
		end
	end)

	tasklet.start_task(function ()
		while true do 
			local fd, addr, port = ch_server:accept()
			if fd >= 0 then
				io.stdout:write('connection established with ', addr, ':',  port, '\n')
				os.close(fd)
			end
		end
	end)

	tasklet.loop()
else
	
end

