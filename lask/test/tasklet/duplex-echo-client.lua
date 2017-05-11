
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

arg[1]   10x1  (number-of-connections x heartbeat-interval)
arg[2]   ip/port/ip:port, port defaulted to 60001, ip defaulted to 127.0.0.1
arg[3]   ssl/tcp, defaulted to tcp
]]
if arg[1] == 'help' then
    print(help)
    os.exit(0)
end

local tasklet = require 'tasklet.channel.message'
local log = require 'log'


local NUM_CONNS = 1
local HB_INTERVAL = 1
local IP, PORT = '127.0.0.1', 60002

if arg[1] then
    local a, b = arg[1]:match('(%d+)x(%d+)')
    if a and b then
        NUM_CONNS = tonumber(a)
        HB_INTERVAL = tonumber(b)
    end
end

if arg[2] then
    local opt = arg[2]
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
if arg[3] == 'ssl' then
    local ssl = require 'ssl'
    tasklet = require 'tasklet.channel.sslstream'
    tasklet.sslstream_channel.ctx = ssl.context.new('sslv23')
    channel_type = tasklet.sslstream_channel
else
    tasklet = require 'tasklet.channel.stream'
    channel_type = tasklet.stream_channel
end

local strerror = errno.strerror
local function conn_loop(id)
    local idstr = '#' .. id

    local ch_conn = channel_type.new()
    if ch_conn:connect(IP, PORT) ~= 0 then
        log.error(idstr, ': unable to connect server')
        return
    end

    local ch_msg = tasklet.message_channel.new(20)

    local function heartbeat_task()
        local msg = 'heartbeat_' .. idstr
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
        if msgid == 'server' then
            ch_msg:write(line)
            --log.debug(idstr, ': received server heartbeat request')
        else
            assert(tonumber(msgid) == id)
            --log.debug(idstr, ': received heartbeat response')
            dog_hungary = false
        end
    end

    tasklet.join_tasks()
    log.info(idstr, ': main task exited')
end

local ch_ctrlmgr = tasklet.message_channel.new(20)

local function conn_manager_task()
    local function start_conn_task(id, first)
        tasklet.start_task(function ()
            if not first then
                tasklet.sleep(5)
            end
            conn_loop(id)
            ch_ctrlmgr:write(id)
        end)
    end

    for i = 1, NUM_CONNS do
        start_conn_task(i, true)
    end

    while true do
        local id = ch_ctrlmgr:read()
        log.warn('#', tostring(id), ' exited, restart it')
        start_conn_task(id)
    end
end

tasklet.start_task(conn_manager_task)

tasklet.loop()
