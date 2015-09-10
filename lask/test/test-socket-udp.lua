require 'std'

local AS_SERVER = arg[1] == 'server'
local PORT = 60001

local fd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

if AS_SERVER then
	socket.bind(fd, '127.0.0.1', PORT)
	while true do
		if poll.waitfd(fd) then 
			local str, addr, port, err = socket.recvfrom(fd)
			if str then
				print(str, addr, port)
			else
				print(errno.strerror(err))
			end
		end
	end
else
	while true do 
		local str = os.read(0)
		if not str then break end
		print('send -> ', str)
		print(socket.sendto(fd, '127.0.0.1', PORT, str))
	end
end

