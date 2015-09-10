require 'std'
local app = require 'app'
local log = require 'log'
local tl = require 'tasklet'

tl.init()

app.start_ctlserver_task('test-app', {
	ok = function () 
		return 0, {1,2,3,4}
	end,

	error = function ()
		return errno.EFAULT
	end,
})

log.init({path='memory'})
tl.start_task(function ()
	while true do 
		log.info(time.time())
		tl.sleep(1)
	end
end)

tl.loop()

