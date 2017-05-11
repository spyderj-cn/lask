
local help = [[
                             req:x
    +--------------+    ------------>             +--------------+
    |              |    <------------             |              |
    |              |         resp:x               |              |
    |    conn    |                              |    server    |
    |              |        req:x                 |              |
    |              |    <------------             |              |
    +--------------+    ------------->            +--------------+
                            resp:x

    conn -> server:  'heartbeat_#<id>'
    server -> conn:  'heartbeat_#server'

arg[1]   port, defaulted to 60002
arg[2]   heartbeat interval, defaulted to 10
arg[3]   ssl/tcp, defaulted to tcp
]]
if arg[1] == 'help' then
    print(help)
    os.exit(0)
end


local log = require 'log'

local PORT = tonumber(arg[1]) or 60002
local HB_INTERVAL = tonumber(arg[2]) or 10

local tasklet = require 'tasklet.channel.message'
local channel_type
local SSL
if arg[3] == 'ssl' then
	local ssl = require 'ssl'
    require 'tasklet.channel.sslstream'
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
    require 'tasklet.channel.stream'
    channel_type = tasklet.stream_channel
end

local listen_fd = socket.tcpserver('0.0.0.0', PORT)
if listen_fd < 0 then
	log.fatal('unable to bind at port ', PORT)
end

local idx = 1
local strerror = errno.strerror

local function conn_loop(ch_conn)
    local id = idx
    idx = idx + 1
    local idstr = '#' .. id
    local ch_msg = tasklet.message_channel.new(20)

    local function heartbeat_task()
        local msg = 'heartbeat_#server'
        log.info(idstr, ': heartbeat task started')
        while true do
            tasklet.sleep(HB_INTERVAL)
            if ch_msg:write(msg) ~= 0 then
                -- must be someone closed it, don't make any logs
                break
            end
        end
        log.info(idstr, ': hearbeat task exited')
    end

    local function send_msg_task()
        log.info(idstr, ': send message task started')
        local buf = buffer.new(256)
        local err = 0
        while true do
            local msg = ch_msg:read()
            if not msg then
                ch_conn:close()
                break
            end

            buf:rewind():putstr(msg, '\n')
            err = ch_conn:write(buf)
            if err ~= 0 then
                log.error(idstr, ': failed to send heartbeat, err -> ', strerror(err))
                ch_conn:close()
                ch_msg:close()
                break
            end
        end
        log.info(idstr, ': send message task exited')
    end

    local dog_hungary = true
    local watchdog = false

    local function watchdog_task()
        log.info(idstr, ': watchdog task started')
        while true do
            tasklet.sleep(HB_INTERVAL * 2)
            if dog_hungary then
                log.error(idstr, ': heartbeat timed out')
                ch_msg:close()
                watchdog = false
                break
            end
            dog_hungary = true
        end
        log.info(idstr, ': watchdog task exited')
    end

    tasklet.start_task(send_msg_task, nil, true)
    tasklet.start_task(heartbeat_task, nil, true)
    watchdog = tasklet.start_task(watchdog_task, nil, true)

    -- recv message loop
    while true do
        local line, err = ch_conn:read()
        if not line then
            ch_conn:close()
            if watchdog then
                tasklet.reap_task(watchdog)
                watchdog = false
            end
            break
        end
        local msgid = line:match('^heartbeat_#(.+)$')
        if tonumber(msgid) then
            ch_msg:write(line)
            --log.debug(idstr, ': received client heartbeat request')
        else
            assert(msgid == 'server')
            --log.debug(idstr, ': received heartbeat response')
            dog_hungary = false
        end
    end

    tasklet.join_tasks()
    log.info(idstr, ': main task exited')
end

tasklet.add_handler(listen_fd, tasklet.EVT_READ, function ()
	local fd, addr, port = socket.accept(listen_fd)
	if fd >= 0 then
		log.info('connection established with ', addr, ':', port)
		tasklet.start_task(function ()
			local ch = channel_type.new(fd)
            if not SSL or ch:handshake() == 0 then
                conn_loop(ch)
            end
			log.info('connection off with ', addr, ':', port)
			ch:close()
		end)
	end
end)

tasklet.loop()
