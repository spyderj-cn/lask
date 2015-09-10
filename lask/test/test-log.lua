local log = require 'log'

local time = time.time 

local total = 0

log.init({path='memory'})

while true do 
	log.info('dddddddddddddddddddddddd')
	total = total + 1
	if total == 100000 then
		print(time(), '100000')
		total = 0	
	end
end
