
--[[

arg[1]  server/client
arg[2]  number of clients, if arg[1] == 'client', defaulted to 1000
]]

local NUM_CLIENTS = tonumber(arg[2]) or 1000

local SERVER_ADDR = '/tmp/test-streamserver.sock'

local tasklet

if arg[1] == 'server' then
    tasklet = require 'tasklet.channel.streamserver'
    local ch_server = tasklet.create_unserver_channel(SERVER_ADDR)
    tasklet.start_task(function ()
        while true do
            local fd, addr = ch_server:accept()
            assert(fd >= 0)
            os.close(fd)
        end
    end)
else
    tasklet = require 'tasklet.channel.stream'
    for i = 1, NUM_CLIENTS do
        tasklet.start_task(function ()
            local ch = tasklet.stream_channel.new()
            local err
            while true do
                err = ch:connect(SERVER_ADDR)
                if err ~= 0 then
                    print(errno.strerror(err))
                end
                ch:close()
                tasklet.sleep(math.random(1))
            end
        end)
    end
end

tasklet.loop()
