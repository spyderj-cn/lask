
local help = [[
    +--------+      x            +---------+
    |        |    ----->         |         |
	| CLIENT |                   | SERVER  |
	|        |    <----          |         |
	+--------+     xx...x    +---------+
	            (1,000,000 times)


arg[1]   port, defaulted to 60001
arg[2]   ssl/tcp, defaulted to tcp
]]
if arg[1] == 'help' then
    print(help)
    os.exit(0)
end


local log = require 'log'

local PORT = tonumber(arg[1]) or 60001

local tasklet
local channel_type
local SSL
if arg[2] == 'ssl' then
	local ssl = require 'ssl'
    tasklet = require 'tasklet.channel.sslstream'
    local ctx = ssl.context.new('sslv23')
	tasklet.sslstream_channel.ctx = ctx
    channel_type = tasklet.sslstream_channel
	SSL = true

	local conf = {
		protocol = 'sslv23',
		certfile = 'serverA.pem',
		keyfile = 'serverAkey.pem',
		cafile = 'rootA.pem'
	}

	if ctx:load_verify_locations(conf.cafile) ~= 0 then
		log.fatal('ssl.context.load_verify_locations failed')
	end
	if ctx:use_certfile(conf.certfile) ~= 0 then
		log.fatal('ssl.context.use_certfile failed')
	end
	if ctx:use_keyfile(conf.keyfile, 'foobar') ~= 0 then
		log.fatal('ssl.context.use_keyfile failed')
	end
else
    tasklet = require 'tasklet.channel.stream'
    channel_type = tasklet.stream_channel
end


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
			local ch = channel_type.new(fd)
			local buf = buffer.new()
			if not SSL or ch:handshake(-1) == 0 then
				while true do
					local line = ch:read()
					if not line then
						break
					end

					local nreq = #line
					local nresp = 1000000 * nreq
					local left = 1000000

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
						assert(err == 0, errno.strerror(err))
						buf:rewind()
					end
				end
			end
			log.info('connection off with ', addr, ':', port)
			ch:close()
		end)
	end
end)

tasklet.loop()
