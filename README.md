# lask
Lua is a promising language on embedded systems of intelligent devices. The language is elegant, clean, and it costs reasonable resources but can be programmed very fast and easily.

Lask provides a foundation library wrapping the posix API and an async I/O communication framework for the lua programing language.

The main goal is to relieve embedded C programmers(like myself) of endless cross-compiling, and help them to finish their job as quickly as their web development fellows.


###An ECHO-SERVER for example.

####**Multi Process Style**

```lua
require 'std'

local listen_fd = socket.tcpserver('127.0.0.1', 9988)

local function serve_the_client(fd)
    while true do
        local input = os.read(fd)
        if not input then
            break
        end
        os.write(fd, input)
    end
    os.close(fd)
end

while true do
    local fd = socket.accept(listen_fd)
    if fd >= 0 then
        if os.fork() == 0 then
            serve_the_client(fd)
            os.exit(0)
        else
            os.close(fd)
        end
    end
end

```

####**IO-Multiplexing Style**
```lua
require 'std'

local listen_fd = socket.tcpserver('127.0.0.1', 9988)
local fd_set = {listen_fd}

while true do
    for _, fd in pairs(poll.select(fd_set) or NULL) do
        if fd == listen_fd then
            local peer_fd = socket.accept(listen_fd)
            if peer_fd >= 0 then
                table.insert(fd_set, peer_fd)
            end
        else
            local input = os.read(fd)
            if input then
                os.write(fd, input)
            else
                os.close(fd)
                table.remove(fd_set, table.find(fd_set, fd))
            end
        end
    end
end
```


####**Using the Async I/O Framework (tasklet)**
At a glance, the tasklet way uses more code than the other two, but the truth is it's far more powerful and flexiable.

```lua
local tasklet = require 'tasklet'
require 'tasklet.channel.streamserver'
require 'tasklet.channel.stream'

local ch_server = tasklet.create_tcpserver_channel('127.0.0.1', 9988)
if not ch_server then
    io.stderr:write('failed to start echo-server, socket error')
    os.exit(1)
end

local function serve_the_client(fd)
    local ch = tasklet.stream_channel.new(fd)
    local buf = buffer.new()
    tasklet.start_task(function ()
        while true do
            local input = ch:read(nil, 5) -- read the input in 5 seconds or we'll just quit
            if not input then
                break
            end
            buf:rewind():putstr(input, '\n')
            ch:write(buf)
        end
        ch:close()
    end)
end


tasklet.start_task(function ()
    while true do
        local fd = ch_server:accept()
        if fd >= 0 then
            serve_the_client(fd)
        end
    end
end)

tasklet.loop()
```
