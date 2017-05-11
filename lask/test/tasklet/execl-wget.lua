
--[[

download from :

www.qq.com
www.163.com
www.sina.com

arg[1]   number of requests for each url

]]

-- asynchronously spawned wget to download homepage of www.qq.com

local tasklet = require 'tasklet.channel.stream'
local log = require 'log'

local NUM_REQUESTS = tonumber(arg[1]) or 100

local urls = {
	['www.qq.com'] = 20000,
	['www.163.com'] = 10000,
	['www.sina.com'] = 10000
}
local total = 3 * NUM_REQUESTS
local count = 0

local function download(url, refsiz)
	local buf = buffer.new()

	for i = 1, NUM_REQUESTS do
		local ch = tasklet.create_execl_channel('wget -O- http://' .. url)
		while true do
			local rd, err = ch:read(1000)
			if not rd then
				assert(err == 0)
				break
			end
			buf:putreader(rd)
		end

		assert(#buf > refsiz, string.format('%s: refsiz = %d, #buf = %d', url, refsiz, #buf))

		log.info(url, ' #', i, '  -> ', #buf)
		count = count + 1
		if count == total then
			os.exit(0)
		end
		buf:rewind()
		ch:close()
	end
end

for url, refsiz in pairs(urls) do
	tasklet.start_task(function ()
		download(url, refsiz)
	end)
end

tasklet.loop()
