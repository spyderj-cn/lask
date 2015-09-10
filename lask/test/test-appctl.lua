require 'std'

local appctl = require 'appctl'

appctl.HELP = [[
this is HELP ~~~~
]]

appctl.APP_NAME = 'test-app'

if #arg > 0 then
	appctl.dispatch(arg)
else
	appctl.interact()
end
--[[
local total = 0
while true do 
	assert(appctl.invoke('ok') == 0)
	total = total + 1
	if total == 10000 then
		print(time.time(), '10000')
		total = 0
	end	
end
]]
