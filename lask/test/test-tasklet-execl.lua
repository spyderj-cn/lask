
-- asynchronously spawned wget to download homepage of www.qq.com 

local tasklet = require 'tasklet.channel.stream'
local log = require 'log'

tasklet.init()

tasklet.start_task(function ()
	local ch = tasklet.create_execl_channel('wget -O- http://www.qq.com')
	dump(ch)
	while true do 
		local rd, err = ch:read(1000)
		if not rd then
			log.info(errno.strerror(err))
			break
		else
			os.writeb(1, rd)
		end
	end
	ch:close()
	os.exit(0)
end)

tasklet.start_task(function ()
	local total = 0
	while true do 
		tasklet.sleep(0.1)
		total = total + 0.1
		print(total, 'seconds ...')
	end
end)

tasklet.loop()
