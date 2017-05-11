
--[[

   producer_1                                   consumer_1
                       messages -->
   ...           --------------------             ....
                   message channel
                 --------------------

   producer_N                                   consumer_M


arg[1] mumber of messages, defaulted to 3000
arg[2] message channel's capacity, defaulted to 200
arg[3] sleep interval, defaulted to 1
arg[4] number of producers, defaulted to 100
arg[5] number of consumers, defaulted to 100

]]

local tasklet = require 'tasklet.channel.message'

local NUM_MESSAGES = tonumber(arg[1]) or 3000
local NUM_CAP = tonumber(arg[2]) or 200
local NUM_SLEEP_DIV = tonumber(arg[3]) or 1
local NUM_PRODUCER = tonumber(arg[4]) or 100
local NUM_CONSUMER = tonumber(arg[5]) or 100

local ch = tasklet.message_channel.new(NUM_CAP)
local cdone = 0
local pdone = 0

local pcounters = table.array(NUM_PRODUCER)
local ptotal = 0
local function producer(id)
    tasklet.start_task(function ())
        local count = 0
        while true do
            if ch:write(id) ~= 0 then
                break
            end
            count = count + 1
            tasklet.sleep(math.random(1) / NUM_SLEEP_DIV)
        end
        pcounters[id] = count
        ptotal = ptotal + count
        pdone = pdone + 1
        if pdone == NUM_PRODUCER then
            print('ptotal -> ', ptotal)
            print('producers: ')
            dump(pcounters)
            assert(ptotal >= NUM_MESSAGES and (NUM_MESSAGES + NUM_CAP >= ptotal))
            if cdone == NUM_CONSUMER then os.exit(0) end
        end
    end)
end

local ccounters = table.array(NUM_CONSUMER)
local ctotal = 0
local function consumer(id)
    tasklet.start_task(function ()
        local count = 0
        while true do
            local data, err = ch:read()
            if not data then
                break
            end
            count = count + 1
            ctotal = ctotal + 1
            if ctotal == NUM_MESSAGES then
                ch:close()
                break
            end
            tasklet.sleep(math.random() / NUM_SLEEP_DIV)
        end
        ccounters[id] = count
        cdone = cdone + 1
        if cdone == NUM_CONSUMER then
            print('consumers: ')
            dump(ccounters)
            if pdone == NUM_PRODUCER then os.exit(0) end
        end
    end)
end

for i = 1, NUM_PRODUCER do
    producer(i)
end

for i = 1, NUM_CONSUMER do
    consumer(i)
end

tasklet.loop()
