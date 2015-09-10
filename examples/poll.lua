
require 'std'

print([[
*******************************************************
This is a type-and-echo program, use three different ways
    poll.waitfd
    poll.select
    poll.wait
to receive your input.
*******************************************************
]])

local poll_fd = poll.create(0)

poll.add(poll_fd, 0, poll.IN)

local quit = false
local total = 0
while not quit do 
	local ret

	if total == 0 then
		print('use poll.wait')
		ret = poll.wait(poll_fd)
	elseif total == 1 then
		print('use poll.select')
		ret = poll.select({0}, nil, nil)
	else
		print('use poll.waitfd')
		ret = poll.waitfd(0, 'r')
	end

	if ret then
		local str = os.read(0)
		str = str and str:lower()
		if not str or str:match('^quit') then
			print('exiting ...')
			quit = true
		else
			print(str)
			total = total + 1
			if total == 3 then
				total = 0
			end
		end
	end
end

