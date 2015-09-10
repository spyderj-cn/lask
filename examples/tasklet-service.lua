
local tl = require 'tasklet.service'
local log = require 'log'
local assert = assert
local request = tl.request
local ETIMEDOUT = errno.ETIMEDOUT
local random = math.random
local count = 0

local svc = tl.create_service('double', function (svc, x)
        tl.sleep(0.1)
		print('handling')
        return x * 2
end)

local function new_client(id)
        tl.start_task(function ()
                while true do
                        local start = tl.now
                        local interval = random()
                        local ret, err = tl.request(svc, id, interval)
                        assert(err == ETIMEDOUT or ret == id * 2)
                        if err == ETIMEDOUT then
                                local diff = tl.now - start
                                if diff > (interval + 0.1) or diff < (interval - 0.1) then
                                        log.error('*** diff - interval ->', diff - interval)
                                end
                                log.warn('timedout', id)
								count = count + 1
								if count > 100 then
									log.fatal('service starved to death')
								end
                        else
                                log.info('succeed ', id)
								count = 0
                        end
                end
        end)
end

for i = 1, 11 do
        new_client(i)
end

tl.loop()

