
local tl = require 'tasklet.service'
local log = require 'log'
local assert = assert
local request = tl.request
local ETIMEDOUT = errno.ETIMEDOUT
local random = math.random

local failed = 0
local sum = 0

local svc = tl.create_multi_service('sum', function (reqarr, resparr, errar, n)
		print('handling')
		tl.sleep(0.1)
        sum = 0
		for i = 1, n do 
			sum = sum + reqarr[i]
		end
		for i = 1, n do 
			resparr[i] = sum
		end	
end, 10, 1)

local function new_client(id)
        tl.start_task(function ()
                while true do
                        local start = tl.now
                        local interval = random() + 0.1
                        local ret, err = tl.request(svc, id, interval)
                        assert(err == ETIMEDOUT or ret == sum)
                        if err == ETIMEDOUT then
                                local diff = tl.now - start
                                if diff > (interval + 0.1) or diff < (interval - 0.1) then
                                        log.error('*** diff - interval ->', diff - interval)
                                end
                                log.warn('timedout', id)
								failed = failed + 1
								if failed > 100 then
									log.fatal('failed > 100')
								end
                        else
                                log.info('succeed ', id)
								failed = 0
                        end
                end
        end)
end

for i = 1, 30 do
        new_client(i)
end

tl.loop()

