
--
-- Copyright (C) Spyderj
--


local tasklet = require 'tasklet.channel._stream'
local ssl = require 'ssl'
local os, errno, socket = os, errno, socket

local ETIMEDOUT = errno.ETIMEDOUT
local btest = bit32.btest
local error = error

local block_task, resume_task, current_task = tasklet._block_task, tasklet._resume_task, tasklet.current_task
local add_handler, mod_handler, del_handler = tasklet.add_handler, tasklet.mod_handler, tasklet.del_handler
local READ, WRITE, EDGE = tasklet.EVT_READ, tasklet.EVT_WRITE, tasklet.EVT_EDGE

local EWANTR, EWANTW, EZERORET = ssl.ERROR_WANT_READ, ssl.ERROR_WANT_WRITE, ssl.ERROR_ZEROR_RETURN


local CONF_RBUFSIZ = 4096

-------------------------------------------------------------------------------
-- channel states {

-- Channel is closed (ch:error() is called, and the internal file descriptor is closed).
-- In this state, no more I/O operations are allowed, although reduplicated close is not 
-- regarded as an error(but will be ignored)
local CH_CLOSED = -2

-- I/O error occured
local CH_ERRORED = -1

-- The other side has stopped reading.
-- Note that sslstream channel 
local CH_HALFCLOSED = 0

-- TCP connection is in progress
local CH_CONNECTING = 1

-- TCP connection established
local CH_TCP = 2

-- SSL handshake is in progress
local CH_HANDSHAKING = 3

-- SSL connection established
local CH_SSL = 4

-- Reading is in progress (so that read() must be called again on the next I/O event)
local CH_READING = 5

-- Writing is in progress (so that write() must be called again on the next I/O event)
local CH_WRITING = 6

-- Shutdown is in progress(so that shutdown() must be called again on the next I/O event)
local CH_SHUTDOWN = 7

-- Generally, values > 0 means that the connection is GOOD, <= 0 means BAD or at 
-- least not something unnormal happened and you have a good reason to close it.


-- }
------------------------------------------------------------------------------

-- {[state] = handler} 
local ch_evthandlers = {}

local function ch_addwrite(self)
	local events = self.ch_events
	if not btest(events, WRITE) then
		events = events + WRITE
		mod_handler(self.ch_fd, events)
		self.ch_events = events
	end
end

local function ch_delwrite(self)
	local events = self.ch_events
	if btest(events, WRITE) then
		events = events - WRITE
		mod_handler(self.ch_fd, events)
		self.ch_events = events
	end
end

local function ch_onevents(self, fd, revents)
	local handler = ch_evthandlers[self.ch_state]
	if handler then
		local ready = handler(self, fd, revents)
		local task = self.ch_task
		
		if ready and task then
			resume_task(task)
			self.ch_task = false
		end
	elseif btest(revents, tasklet.EVT_WRITE) then
		ch_delwrite(self)
	end
end

local sslstream_channel = tasklet._stream_channel_prototype()
local sslstream_channel_meta = {
	__index = sslstream_channel,
	__call = ch_onevents,
}
tasklet.sslstream_channel = sslstream_channel

-- Create an sslstream channel
-- 
-- If 'fd' is valid, then the channel runs in accept-mode, otherwise in connect-mode.
-- 
-- 'conf.ctx' is used as the SSL context, if absent, fall back to the default SSL context 
-- specified in sslstream_channel.ctx which you have to init by yourself.
function sslstream_channel.new(fd, conf)
	fd = fd or -1
	conf = conf or NULL
	
	local state = fd >= 0 and CH_TCP or CH_CLOSED
	local ch = setmetatable({
		ch_fd = fd,
		ch_state = state,
		ch_err = 0,
		ch_rbufsiz = conf.rbufsiz or tasklet.conf.stream_rbufsiz or CONF_RBUFSIZ,
		ch_rbuf = buffer.new(),
		ch_rsiz = 0,
		ch_wbuf = false,
		ch_woffset = 0,
		ch_wsiz = 0,
		ch_nshift = 0,
		ch_task = false,
		ch_events = READ + EDGE,
		ch_line = false,
	}, sslstream_channel_meta)
	
	if fd >= 0 then
		local ctx = conf.ctx or sslstream_channel.ctx
		if not ctx then
			error('please speficied ssl context in conf.ctx or set sslstream_channel.ctx to the default global one')
		end
		os.setnonblock(fd)
		add_handler(fd, ch.ch_events, ch)
		ssl.attach(fd, ctx)
		ssl.set_accept_state(fd)
	end	
	return ch
end

-- DEPRECATED
tasklet.create_sslstream_channel = sslstream_channel.new

ch_evthandlers[CH_HANDSHAKING] = function (self, fd, revents)
	local err = ssl.do_handshake(fd)
	
	if err == EWANTW then
		ch_addwrite(self)
	elseif err ~= EWANTR then
		ch_delwrite(self)
		self.ch_err = err
		return true
	end
end

-- Make a SSL handshake
--
-- Return 0 if succeed, ETIMEDOUT if timed out.
-- Otherwise return a negative value if an SSL error occured. In this case (-err) is the SSL error code
function sslstream_channel:handshake(sec)
	local state = self.ch_state
	if state ~= CH_TCP then
		error('illegal sslstream_channel:handshake(): required to be in CH_TCP state')
	end
	
	local fd = self.ch_fd
	local err = ssl.do_handshake(fd)
	if err == EWANTR or err == EWANTW then
		if err == EWANTW then
			ch_addwrite(self)
		end
		local task = current_task()
		self.ch_state = CH_HANDSHAKING	
		self.ch_task = task
		task.t_blockedby = self
		err = block_task(sec or -1)
		if err == 0 then
			err = -self.ch_err
		end
	else
		err = -err
	end
	
	if err == 0 then
		self.ch_state = CH_SSL
	else 
		ssl.detach(self.ch_fd)
		os.close(self.ch_fd)
		self.ch_fd = -1
		self.ch_state = CH_ERRORED
	end
	return err
end

ch_evthandlers[CH_CONNECTING] = function (self, fd, revents)
	self.ch_err = socket.getsocketopt(fd, socket.SO_ERROR)
	return true
end

function sslstream_channel:connect(addr, port, sec, ctx)
	local fd = self.ch_fd
	if fd >= 0 then
		error('illegal sslstream_channel:connect() : not a closed ssl channel')
	end
	
	ctx = ctx or sslstream_channel.ctx
	if not ctx then
		error('please speficied a valid ssl context or set sslstream_channel.ctx to the default global one')
	end
	
	local err
	local family = addr:find(':') and socket.AF_INET6 or socket.AF_INET
	local tm_start = tasklet.now
	fd, err = socket.async_connect(addr, port, family)
	if fd < 0 then
		return err
	end

	if err ~= 0 then
		self.ch_state = CH_CONNECTING
		self.ch_events = READ + WRITE + EDGE
		add_handler(fd, self.ch_events, self)
		
		local task = current_task()
		task.t_blockedby = self
		self.ch_task = task
		self.ch_state = CH_CONNECTING
		err = block_task(sec or -1)
		
		if err == 0 then
			err = self.ch_err
		end
		if err ~= 0 then
			tasklet.del_handler(fd)
			os.close(fd)
			self.ch_state = CH_CLOSED 
			return err
		end
	end
	
	sec = sec or -1
	if sec > 0 then
		local elapsed = tasklet.now - tm_start
		if elapsed >= sec then
			tasklet.del_handler(fd)
			os.close(fd)
			self.ch_state = CH_CLOSED 
			return ETIMEDOUT
		end
		sec = sec - elapsed
	end
	
	self.ch_fd = fd
	self.ch_state = CH_TCP
	ssl.attach(fd, ctx)
	ssl.set_connect_state(fd)
	return self:handshake(sec)
end

-- Return true if successfully read some data or ch_state is changed
local function ch_read(self, siz)
	local fd = self.ch_fd
	local rbuf = self.ch_rbuf
	local updated = false
	
	siz = siz or (self.ch_rbufsiz - #rbuf)
	
	if siz > 0 then
		local nread, err = ssl.readb(fd, rbuf, siz)
		self.ch_err = err
		
		if nread > 0 then
			--os.writeb(1, rbuf, #rbuf - nread, nread)
			updated = true
		end
		
		if err == EWANTW then
			ch_addwrite(self)
		elseif err == EZERORET then
			self.ch_state = CH_HALFCLOSED
			updated = true
		elseif err ~= 0 and err ~= EWANTR then
			self.ch_rsiz = 0
			self.ch_nshift = 0
			self.ch_state = CH_ERRORED
			updated = true
		end
	end
	return updated
end

ch_evthandlers[CH_READING] = function (self, fd, revents)
	local ready = false
	if ch_read(self) then
		local rsiz = self.ch_rsiz
		local state = self.ch_state
		if state > 0 then
			local rbuf = self.ch_rbuf
			if rsiz > 0 then 
				if #rbuf >= rsiz then
					-- enough bytes
					ready = true		
				end
			else 
				local line = rbuf:getline()
				if line then
					-- got a line
					self.ch_line = line
					ready = true
				end
			end
		else
			-- must tell the task the channel is errored or half-closed
			ready = true
		end
	end
	return ready
end

-- Refer to stream_channel:read() for more details. They are twins.
function sslstream_channel:read(bytes, sec)
	local state = self.ch_state

	if state == CH_CLOSED then
		error('illegal reading on closed stream channel')
	end
	if state == CH_ERRORED then
		return nil, self.ch_err
	end
	if self.ch_task then
		error('another task is blocked on this channel: stream_channel is exclusively owned by one task')
	end

	local rbuf = self.ch_rbuf
	local rbufsiz = self.ch_rbufsiz	
	if bytes and bytes > rbufsiz then
		bytes = rbufsiz
	end
	if self.ch_nshift > 0 then
		rbuf:shift(self.ch_nshift)
		self.ch_nshift = 0
	end
	
	sec = sec or -1
	local task = current_task()
	local tm_start = tasklet.now
	while true do 
		if bytes then
			local rbufsiz = #rbuf
			if rbufsiz >= bytes then
				self.ch_nshift = bytes
				return rbuf:reader(nil, 0, bytes), 0
			elseif state == CH_HALFCLOSED then
				if rbufsiz > 0 then
					self.ch_nshift = rbufsiz
					return rbuf:reader()
				else
					return nil, 0
				end
			end
		else
			local line = self.ch_line
			if line then
				self.ch_line = false
				return line, 0
			else
				line = rbuf:getline()
				if line or state == CH_HALFCLOSED then 
					return line, 0
				end
			end
		end
		
		if not ch_read(self) then
			if sec >= 0 then
				sec = sec - (tasklet.now - tm_start)
				if sec <= 0 then
					return nil, ETIMEDOUT
				end
			end
			
			self.ch_rsiz = bytes or -1
			self.ch_task = task
			self.ch_state = CH_READING
			task.t_blockedby = self
			
			local err = block_task(sec)
			self.ch_task = false
			if err ~= 0 then
				return nil, err
			end
		end
	
		if self.ch_state == CH_ERRORED then
			return nil, -self.ch_err
		end
	end
end

ch_evthandlers[CH_WRITING] = function (self, fd, revents)
	local ready = true
	local nwritten, err = ssl.writeb(self.ch_fd, self.ch_wbuf, self.ch_woffset, self.ch_wsiz)
	
	if err == 0 then
		self.state = CH_SSL
	elseif err == EWANTR or err == EWANTW then
		ready = false
	elseif err == EZERORET then
		self.state = CH_HALFCLOSED
	else
		self.state = CH_ERRORED
		self.ch_err = err
	end
	return ready
end

-- Refer to stream_channel:write() for more details. They are twins.
function sslstream_channel:write(data, sec)
	local state = self.ch_state
	
	if state == CH_CLOSED then
		-- require('log').error(debug.traceback)
		error('illegal writing on closed stream channel')
	end
	if self.ch_task then
		error('another task is blocked on this channel: stream_channel is exclusively owned by one task')
	end
	if state == CH_ERRORED then
		return self.ch_err
	end
	
	sec = sec or -1
	local task = current_task()
	local tm_start = tasklet.now
	
	-- partial-write may be enabled
	local datasiz = #data
	local offset = 0
	local fd = self.ch_fd
	while datasiz > 0 do
		local nwritten, err = ssl.writeb(fd, data, offset, datasiz)
		if err == 0 then
			datasiz = datasiz - nwritten
			offset = offset + nwritten
		else
			if err ~= EWANTR and err ~= EWANTW then
				self.ch_state = CH_ERRORED --XXX: do we have to check EZERORET ?
				return -err
			end
			
			if sec >= 0 then
				sec = sec - (tasklet.now - tm_start)
				if sec <= 0 then
					return nil, ETIMEDOUT
				end
			end
			
			if err == EWANTW then
				ch_addwrite(self)
			end
			self.ch_task = task
			self.ch_state = CH_WRITING
			self.ch_wbuf = data
			self.ch_woffset = offset
			self.ch_wsiz = datasiz
			task.t_blockedby = self
			err = block_task(sec, task)
			
			self.ch_task = false
			if err ~= 0 then
				return err
			end
			
			if self.ch_state < 0 then
				return self.ch_err
			end
		end
	end
	return 0
end

ch_evthandlers[CH_SSL] = function (self, fd, revents)
	if btest(revents, READ) then
		ch_read(self)
	else
		-- XXX:
		ch_delwrite(self)
	end
	return false
end

ch_evthandlers[CH_SHUTDOWN] = function (self, fd, events)
	local _, err = ssl.shutdown(fd)
	return err ~= EWANTR and err ~= EWANTW
end

function sslstream_channel:close(cb_post_shutdown)
	local fd = self.ch_fd
	if fd >= 0 then
		if ssl.isattached(fd) then
			local _, err = ssl.shutdown(fd)
			if err == EWANTR or err == EWANTW then
				if err == EWANTW then
					ch_addwrite(self)
				end
				
				local task = current_task()
				self.ch_task = task
				task.t_blockedby = self
				self.ch_state = CH_SHUTDOWN
				block_task(sec)
			end
			ssl.detach(fd)
		end
		
		if cb_post_shutdown then
			cb_post_shutdown(self)
		end

		os.close(fd)
		self.ch_fd = -1
		if self.ch_rbuf then
			self.ch_rbuf:reset()
		end
		self.ch_nshift = 0
		self.ch_rsiz = 0
		self.ch_state = CH_CLOSED
	end
end

return tasklet
