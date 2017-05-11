
--[[

download from :

www.qq.com
www.163.com
www.sina.com

arg[1]   number of requests for each url, defaulted to 100
arg[2]   timedout seconds for each request, defaulted to 5

]]

-- asynchronously spawned wget to download homepage of www.qq.com

local tasklet = require 'tasklet.channel.stream'
local log = require 'log'

local NUM_REQUESTS = tonumber(arg[1]) or 100
local SEC_WAIT = tonumber(arg[2]) or 5

local urls = {
	['www.qq.com'] = 30000,
	['www.163.com'] = 10000,
	['www.sina.com'] = 10000
}
local total = 3 * NUM_REQUESTS
local count = 0
local ETIMEDOUT = errno.ETIMEDOUT

local function download(url, refsiz)
	local buf = buffer.new()
    local failed = 0

	for i = 1, NUM_REQUESTS do
		local ch = tasklet.create_execl_channel('wget -O- http://' .. url)
        local all_sec = SEC_WAIT
		while all_sec > 0 do
            local ts = tasklet.now
			local rd, err = ch:read(1000, all_sec)
			if not rd then
				assert(err == ETIMEDOUT or err == 0)
				break
			end
			buf:putreader(rd)
            all_sec = all_sec - tasklet.now + ts
		end

        if all_sec > 0 then
		    assert(#buf > refsiz, string.format('%s: refsiz = %d, #buf = %d', url, refsiz, #buf))
            log.info(url, ' #', i, '  -> ', #buf)
        else
            failed = failed + 1
            log.warn(url, ' #', i, '  -> failed')
        end
        buf:rewind()
        ch:close()

		count = count + 1
	end

    log.warn(url, '  failed = ', failed)

    if count == total then
        os.exit(0)
    end
end


for url, refsiz in pairs(urls) do
	tasklet.start_task(function ()
		download(url, refsiz)
	end)
end

tasklet.loop()
