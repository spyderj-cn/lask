
--
-- Copyright (C) Spyderj
--

local tasklet = require 'tasklet'
local block_task = tasklet._block_task
local resume_task = tasklet._resume_task
local current_task = tasklet.current_task

local message_channel = {}
message_channel.__index = message_channel

function message_channel.new(capacity)
	local msg = true
	capacity = capacity or 0
	if capacity > 0 then
		msg = table.array(capacity)
	end
	return setmetatable({
		ch_msg = msg,
		ch_ridx = 1,
		ch_widx = 1,
		ch_cap = capacity,
		ch_num = 0,
		ch_taskq = false, -- double-link list
		ch_rtask = false,
	}, message_channel)
end

local function push_task(ch, task)
	local head = ch.ch_taskq
	if head then
		local tail = head.t_svcprev
		task.t_svcprev = tail
		tail.t_svcnext = task
		head.t_svcprev = task
	else
		task.t_svcprev = task
		ch.ch_taskq = task
	end
	task.t_svcnext = false
end

local function shift_task(ch, task)
	local prev, next = task.t_svcprev, task.t_svcnext
	ch.ch_taskq = next
	if next then
		next.t_svcprev = prev
	end
end

local function unlink_task(ch, task)
	local prev, next = task.t_svcprev, task.t_svcnext
	if task == ch.ch_taskq then	-- in the head
		ch.ch_taskq = next
		if next then next.t_svcprev = prev end
	elseif next then		-- in the middle
		if next then next.t_svcprev = prev end
		if prev then prev.t_svcnext = next end
	else	-- in the tail
		prev.t_svcnext = false
		ch.ch_taskq.t_svcprev = prev
	end
end

function message_channel:read(sec)
	if self.ch_num == 0 then
		local rtask = self.ch_rtask
		if rtask then
			error('another task is reading-blocked on this message channel')
		end
		
		rtask = current_task()
		rtask.t_blockedby = self
		self.ch_rtask = rtask
		
		local err = block_task(sec or -1)
		self.ch_rtask = false
		if err ~= 0 then
			return nil
		end
	end
	
	local task = self.ch_taskq
	if task and task.t_blockedby == self then
		shift_task(self, task)
		resume_task(task)
	end
	
	local msg = self.ch_msg
	local cap = self.ch_cap
	if cap > 0 then
		local ridx = self.ch_ridx
		msg = msg[ridx]
		ridx = ridx + 1
		if ridx > cap then
			ridx = 1
		end
		self.ch_ridx = ridx
	end
	self.ch_num = self.ch_num - 1
	
	return msg
end
message_channel.recv = message_channel.read

local function push_msg(ch, msg)
	local cap = ch.ch_cap
	if cap > 0 then
		local widx = ch.ch_widx
		ch.ch_msg[widx] = msg
		if widx == cap then
			widx = 1
		else
			widx = widx + 1
		end
		ch.ch_widx = widx
		ch.ch_num = ch.ch_num + 1
	else
		ch.ch_msg = msg
		ch.ch_num = 1
	end
end

function message_channel:write(msg, sec)
	local task = current_task()
	local cap = self.ch_cap
	local num = self.ch_num

	if (cap > 0 and num == cap) or (cap == 0 and num == 1) then
		task.t_blockedby = self
		push_task(self, task)
		local err = block_task(sec or -1)
		if err ~= 0 then
			unlink_task(self, task)
			return err
		end
	end
	
	push_msg(self, msg)
	if self.ch_rtask then
		resume_task(self.ch_rtask)
	end
	return 0
end
message_channel.send = message_channel.write

function message_channel:post(msg)
	local num, cap = self.ch_num, self.ch_cap
	if (cap > 0 and num == cap) or (cap == 0 and num == 1) then
		push_msg(self, msg)
		if self.ch_rtask then
			resume_task(self.ch_rtask)
		end
		return true
	end
	return false
end

tasklet.message_channel = message_channel
return tasklet
