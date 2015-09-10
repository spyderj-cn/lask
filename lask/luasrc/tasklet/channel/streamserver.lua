
--
-- Copyright (C) Spyderj
--


local tasklet = require 'tasklet'
local os, socket = os, socket

local block_task, resume_task = tasklet._block_task, tasklet._resume_task

local ETIMEDOUT = errno.ETIMEDOUT

local streamserver_channel = {
	__call = function (self, fd, revents)
		if self.ch_peerfd < 0 then
			local peerfd, peeraddr, peerport, err = socket.accept(fd)
			if err == 0 then
				self.ch_peerfd = peerfd
				self.ch_peeraddr = peeraddr
				self.ch_peerport = peerport
			end
		end
		
		local task = self.ch_task
		if task then
			self.ch_task = false
			resume_task(task)
		end
	end,
}
streamserver_channel.__index = streamserver_channel

function streamserver_channel:accept(sec)
	while true do 
		local fd  = self.ch_peerfd
		if fd >= 0 then
			self.ch_peerfd = -1
			return fd, self.ch_peeraddr, self.ch_peerport, 0
		else
			local err 
			local task = tasklet.current_task()
			
			sec = sec or -1
			if sec ~= 0 then
				task.t_blockedby = self
				self.ch_task = task
				err = block_task(sec)
				self.ch_task = false
			elseif sec == 0 then
				return -1, nil, nil, ETIMEDOUT
			end
			
			if err ~= 0 then
				return -1, nil, nil, err
			end
		end
	end
end

function streamserver_channel:close()
	local fd = self.ch_fd
	if fd >= 0 then
		tasklet.del_handler(fd)
		os.close(fd)
		self.ch_fd = -1
	end
end
	
local function create_streamserver_channel(addr, port, backlog)
	local family = socket.AF_INET
	if not port or port == 0 then
		family = socket.AF_UNIX
		fs.unlink(addr)
	elseif addr and addr:find(':') then
		family = socket.AF_INET6
	end
	
	local fd, err = socket.socket(family, socket.SOCK_STREAM)
	if fd < 0 then
		return nil, err
	end
	
	if family ~= socket.AF_UNIX then
		socket.setsocketopt(fd, socket.SO_REUSEADDR, true)
	end
	err = socket.bind(fd, addr, port)
	if err ~= 0 then
		os.close(fd)
		return nil, err
	end
	socket.listen(fd, backlog)
	os.setnonblock(fd)
	
	local ch = setmetatable({
		ch_fd = fd,
		ch_family = family,
		ch_events = tasklet.EVT_READ,  -- level trigger for listen-fd
		ch_peerfd = -1,
		ch_peeraddr = false,
		ch_peerport = 0,
		ch_task = false,
	}, streamserver_channel)
	tasklet.add_handler(fd, ch.ch_events, ch)
	return ch, 0
end

function tasklet.create_tcpserver_channel(addr, port, backlog)
	return create_streamserver_channel(addr, port, backlog)
end

function tasklet.create_unserver_channel(path)
	return create_streamserver_channel(path, 0)
end

return tasklet
