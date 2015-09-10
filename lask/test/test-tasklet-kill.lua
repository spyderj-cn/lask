
--[[
task 'source' reads a string from stdin and deliver it to task 'A'.
task 'A' filters any string with word 'freedom' or add a signature and deliver it to task 'B'
task 'B' filters any string with word 'democracy' or add a signature and deliver it to task 'sink'
task 'sink' writes the string to the stdout
--]]

local tl = require 'tasklet'
require 'tasklet.channel.stream'

-- task 'sink'
local sink = tl.start_task(function ()
	local task = tl.current_task() 
	while true do 
		tl.sleep()
		print(task.t_sig)
	end
end)

-- task 'B'
local B = tl.start_task(function ()
	local task = tl.current_task()
	while true do
		tl.sleep()
		if not task.t_sig:find('democracy') then
			tl.kill_task(sink, task.t_sig .. ' (B approved)')
		end
	end
end)

-- task 'A'
local A = tl.start_task(function ()
	local task = tl.current_task()
	while true do 
		tl.sleep()
		if not task.t_sig:find('freedom') then
			tl.kill_task(B, task.t_sig .. ' (A approved)')
		end
	end
end)

-- task 'source'
local source = tl.start_task(function ()
	local task = tl.current_task()
	local ch = tl.create_stream_channel(0)
	while true do 
		local line = ch:read()
		if line then
			tl.kill_task(A, line)
		else
			break
		end
	end
	tl.term()
end)

tl.loop()

