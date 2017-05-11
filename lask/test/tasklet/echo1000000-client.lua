local help = [[
    +--------+      x            +---------+
	|        |    ----->         |         |
	| CLIENT |                   | SERVER  |
	|        |    <----          |         |
	+--------+     x, x ... x    +---------+
	               (1,000,000 times)


arg[1]   10x10  (number-of-connections x queries-for-each-connection)
arg[2]   lower_bound,upper_bound
arg[3]   ip/port/ip:port, port defaulted to 60001, ip defaulted to 127.0.0.1
arg[4]   ssl/tcp, defaulted to tcp
]]
if arg[1] == 'help' then
    print(help)
    os.exit(0)
end

local log = require 'log'

local NUM_CONNS = 10
local NUM_QUERIES = 10
local LOWER_BOUND = 1
local UPPER_BOUND = 10
local IP, PORT = '127.0.0.1', 60001

if arg[1] then
    local a, b = arg[1]:match('(%d+)x(%d+)')
    if a and b then
        NUM_CONNS = tonumber(a)
        NUM_QUERIES = tonumber(b)
    end
end

if arg[2] then
    local a, b = arg[2]:match('(%d+),(%d+)')
    if a and b then
        LOWER_BOUND = tonumber(a)
        UPPER_BOUND = tonumber(b)
    end
end

if arg[3] then
    local opt = arg[3]
    if tonumber(opt) then
        PORT = tonumber(opt)
    elseif opt:find(':') then
        IP, PORT = opt:match('(.+):(%d+)')
        PORT = tonumber(PORT)
    elseif opt ~= '_' then
        IP = opt
    end
end

local tasklet
local channel_type
if arg[4] == 'ssl' then
    local ssl = require 'ssl'
    tasklet = require 'tasklet.channel.sslstream'
    tasklet.sslstream_channel.ctx = ssl.context.new('sslv23')
    channel_type = tasklet.sslstream_channel
else
    tasklet = require 'tasklet.channel.stream'
    channel_type = tasklet.stream_channel
end

local log = require 'log'

local index = 1
local done = 0

io.stdout:setvbuf('no')

local function client_task()
    local id = index
	local ch = channel_type.new()
	local buf = buffer.new()

    index = index + 1

	if ch:connect(IP, PORT) ~= 0 then
		log.fatal('server not started')
	end

	for i = 1, NUM_QUERIES do
        tasklet.sleep(math.random())

		local nreq = math.random(LOWER_BOUND, UPPER_BOUND)
        local line = math.randstr(nreq)
		local nresp = nreq * 1000000
		local left = nresp
		ch:write(buf:rewind():putstr(line, '\r\n'))

		while left > 0 do
			local rd, err = ch:read(left)
            assert(err == 0, errno.strerror(err))
			left = left - #rd
            io.stdout:write('\r', nresp - left, '/', nresp)
		end

        log.info('connectoin_', id, ', query #', i, ' done')
	end
	ch:close()
	done = done + 1
    if done == NUM_CONNS then
        os.exit(0)
    end
end

for i = 1, NUM_CONNS do
    tasklet.start_task(client_task)
end

tasklet.loop()
