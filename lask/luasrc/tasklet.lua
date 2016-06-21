
--
-- Copyright (C) Spyderj
--


require 'std'

local os, io, string, table = os, io, string, table
local socket, time = socket, time

local pairs, type, setmetatable, error = pairs, type, setmetatable, error
local co_yield, co_resume, co_status = coroutine.yield, coroutine.resume, coroutine.status
local btest = bit32.btest
local ETIMEDOUT, EAGAIN, EINTR = errno.ETIMEDOUT, errno.EAGAIN, errno.EINTR
local tonumber2 = tonumber2
local mfloor = math.floor

local poll = poll
local READ = poll.IN
local WRITE = poll.OUT
local ERROR = poll.ERR
local EDGE = poll.ET

local NULL = NULL

local assert = assert

-------------------------------------------------------------------------------
-- variables {

local poll_fd = -1

-- {[fd]=handler} for all the registered file descriptors.
local handler_set = {}

-- {[tonumber2(co)] = task} mapping from thread to task. 
local tasks = {}

-- Key is an integer of timestamp, value is also an integer of the returned value of tonumber2(co).
-- The biggest problem is that we have to make each key unique in the rb_timer, to achieve this, 
-- we designed some rules:
-- 1. The key of rb_timer is calculated as '(tasklet.now - tm_baseline + sec_towait) * 1000000 + tm_salt'.
-- 2. If sec_towait > 1000, we split it into small pieces where each one is less than 1000.
-- 3. Now an iteration is devided into 3 stages: the non-event stage, I/O polling and dispatching stage, and 
--    timeout dispatching stage, at the beginning of each stage, tasklet.now is updated.
-- 4. If tasklet.now - tm_baseline > 999 after updated, increments tm_baseline by 999 and decrements all
--    keys of rb_timer(and task.t_timerkey) by 999000000.
-- 5. tm_salt makes every key unique even keys are inserted in the same stage 
--    by incrementing itself after every insertion to rb_timer.
--    When tasklet.now is updated, tm_salt will be reset to 0. 
-- 6. Time deviation is 0.01s.
local rb_timer = rbtree.new()
local tm_baseline = time.uptime()
local tm_salt = 0

-- Tasks that are ready to be resumed.
local ready_list = false	

-- Flag indicating whether we should exit from the loop
local quit = false

local cb_updatedtime = false

-- Modules not driven by I/O events.
-- Each element in the array is a callback function invoked before each I/O polling.
local nonevent_modules = {}

local tasklet = {
	-- seconds since the system booted 
	now = tm_baseline,

	-- seconds since the unix epoch
	now_unix = time.time(),
	
	-- TODO: general configuration of tasklet 
	conf = _G.TASKLET_CONFIG or NULL,
	
	-- non-event moudles
	_nonevent_modules = nonevent_modules,
}

-- } variables
------------------------------------------------------------------------------

-- Yield the current task for some time specified by 'sec'
-- or infinitely (if sec < 0) only if sched_ready() is invoked on the task.
-- 
-- Return 0 if waken up by sched_ready, or errno.ETIMEDOUT if sec > 0 and time is exhausted.
--
-- This function should only be used by internal tasklet modules
local function do_yield(sec)
	local task_id = tonumber2()
	local task = tasks[task_id]
	local err
	
	if task.t_sig ~= NULL then
		error('unable to block yourself in the notification handler')
	end
	
	if sec > 0 then
		err = ETIMEDOUT
		local sec_towait = sec
		
		while sec > 0 and err == ETIMEDOUT do 
			sec_towait = sec
			if sec_towait > 1000 then
				sec_towait = 1000
			end
			
			local diff = tasklet.now - tm_baseline + sec_towait
			if diff > 2000 or diff < 0 then
				error(string.format('tasklet iteration corrupted: now=%f, tm_baseline=%f, sec_towait=%d', 
					tasklet.now, tm_baseline, sec_towait))
			end
			local timestamp = mfloor(1000000 * diff) + tm_salt
			rb_timer:insert(timestamp, task_id)
			task.t_timerkey = timestamp 
			
			sec = sec - sec_towait
			tm_salt = tm_salt + 1
			
			-- The later 4 digits as salt to make each key in the rbtree unique 
			if tm_salt == 9999 then
				tasklet.now = time.uptime()
				tm_salt = 0
			end
			
			err = co_yield()
		end
	else
		err = co_yield()
	end
	return err or 0
end

-- Resume a task 
-- return true if the task is still alive after the resumation
local function do_resume(co, errcode)
	local ok, msg = co_resume(co, errcode)
	local alive = true
	
	if co_status(co) == 'dead' then
		tasks[tonumber2(co)] = nil
		alive = false
	end
	
	if not ok then
		error(msg)
	end
	
	return alive
end

-- Resume all tasks in ready_list
local function resume_list()
	local task = ready_list
	ready_list = false

	local next, co
	while task do 
		while task and not quit do 
			co = task.t_co
			next = task.t_next
			task.t_next = false
	
			if tasks[tonumber2(co)] then
				do_resume(co, 0)
			end
			
			task = next
		end
		
		task = ready_list
		ready_list = false
	end
end

-- Push a task into ready_list (and will be resumed later)
local function push_ready(task)
	if ready_list then
		local tail = ready_list.t_prev
		task.t_prev = tail
		tail.t_next = task
		ready_list.t_prev = task
	else
		task.t_prev = task
		ready_list = task
	end
	task.t_blockedby = false
	task.t_next = false
	
	if task.t_timerkey then
		rb_timer:del(task.t_timerkey, tonumber2(task.t_co))
		task.t_timerkey = false
	end
end

function tasklet.init(conf)
	tasklet.conf = conf or NULL
end

-- Register an event handler
--
-- 'fd' should be a valid file descriptor.
-- 'events' is the combination of EVT_XXXX flags.
-- 'handler' is the callback function when event happens, the prototype is 'func(fd, revents)'.
--		the return value is ignored.
--  Note that 'revents' may not exactly equal to 'events'
function tasklet.add_handler(fd, events, handler)
	if poll_fd < 0 then
		poll_fd = poll.create()
	end
	poll.add(poll_fd, fd, events)
	handler_set[fd + 1] = handler
end

-- Modified an event handler
function tasklet.mod_handler(fd, events, handler)
	poll.mod(poll_fd, fd, events)
	if handler then
		handler_set[fd + 1] = handler
	end
end

-- Delete an event handler
function tasklet.del_handler(fd)
	poll.del(poll_fd, fd)
	handler_set[fd + 1] = false
end

tasklet._block_task = do_yield
tasklet._resume_task = push_ready

-- Get the current task
function tasklet.current_task()
	return tasks[tonumber2()]
end

-- Start a new task.
-- The new task is not resumed immediately, but will be delayed.
-- However, all tasks will be executed in the order they are created
--
-- 'cb' is the callback function.
-- 'obj' is the prototype object of task or nil
--
-- Return the modified/created task object.
function tasklet.start_task(cb, obj)
	local co = coroutine.create(cb)
	local task = obj or {}
	
	task.t_co = co
	task.t_prev = false
	task.t_next = false
	task.t_blockedby = false
	task.t_sig = NULL
	task.t_timerkey = false
	task.t_svcreq = false
	task.t_svcresp = false
	task.t_svcreqtime = false
	
	local slot = tonumber2(co)
	if tasks[slot] then
		error('result of tonumber2(co) conflicts with a existing one in tasklet')
	end
	
	tasks[slot] = task
	push_ready(task)
	return task
end

-- Reap a task
-- The target task will never be resumed again and the resource is released.
function tasklet.reap_task(task)
	if tasks[tonumber2()] == task then
		error('can\'t delete yourself')
	end
	
	local idx = tonumber2(task.t_co)
	if tasks[idx] then
		tasks[idx] = nil
	end
end

-- Make the current task sleep for a while.
-- 
-- 'sec' indicates how long should the task sleep in seconds.
-- 
-- The function takes no effect if sec <= 0.
function tasklet.sleep(sec)
	local task = tasks[tonumber2()]
	if sec and sec > 0 then
		do_yield(sec, task)
	end
end

function tasklet.quit()
	quit = 0
	if tasks[tonumber2()] then
		co_yield()
	end
end

function tasklet.term()
	quit = 1
	if tasks[tonumber2()] then
		co_yield()
	end
end

local function update_time()
	local now = time.uptime()
	if now - tasklet.now < 0.01 then
		time.sleep(0.01)
		now = time.uptime()
	end
	if now - tm_baseline > 999 then
		rb_timer:deckey(999000000)
		tm_baseline = tm_baseline + 999

		local timerkey
		for _, t in pairs(tasks) do 
			timerkey = t.t_timerkey
			if timerkey then
				if timerkey < 999000000 then
					timerkey = 0
				else
					timerkey = timerkey - 999000000
				end
				t.t_timerkey = timerkey
			end
		end
	end
	tasklet.now = now
	local now_unix = time.time()
	tasklet.now_unix = now_unix
	tasklet.now_4log = time.strftime('%m-%d %H:%M:%S ', now_unix)
	tm_salt = 0
	if cb_updatedtime then
		cb_updatedtime(now)
	end
end

function tasklet.set_cbupdatedtime(cb)
	cb_updatedtime = cb
end

function tasklet.loop()
	local wait_ret 
	local tm_nearest, tm_wait, tm_elapsed
	
	if poll_fd < 0 then
		poll_fd = poll.create()
	end
	
	if ready_list then
		update_time()
		resume_list()
	end

	local nemod = tasklet._nonevent_modules
	while not quit do 
		----------------------------------------------------------------------------{
		-- exhaust all nonevent-blocked tasks until they are events-blocked or timer-blocked.
		update_time()
		for _, module in ipairs(nemod) do 
			module()
			while ready_list do 
				resume_list()
				module()
			end
		end
		-----------------------------------------------------------------------------}
		
		
		if quit then break end
		
		
		----------------------------------------------------------------------------{
		-- schedule by events
		local now = time.uptime()
		tm_nearest = rb_timer:min()
		if tm_nearest then
			tm_wait = tm_nearest / 1000000 + tm_baseline - now + 0.01
			if tm_wait < 0.01 then
				tm_wait = 0.01
			elseif tm_wait > 0.1 then  -- XXX: why do I choose 0.1 ?
				tm_wait = 0.1
			end 
		else
			tm_wait = 0.1
		end
		wait_ret = poll.wait(poll_fd, tm_wait)
		update_time()
		if wait_ret then
			for fd, revents in pairs(wait_ret) do 
				local handler = handler_set[fd + 1]
				if handler then
					handler(fd, revents)
				end
			end
			resume_list()
		end
		-----------------------------------------------------------------------------}
		
		
		if quit then break end
		
		
		----------------------------------------------------------------------------{
		-- schedule by timers
		update_time()
		tm_elapsed = math.floor((tasklet.now - tm_baseline) * 1000000)
		local timerkey, task_id = rb_timer:delmin(tm_elapsed)
		while timerkey do
			local task = tasks[task_id]
			if task then
				task.t_timerkey = false
				do_resume(task.t_co, ETIMEDOUT)
			end
			timerkey, task_id = rb_timer:delmin(tm_elapsed)
		end
		if ready_list then
			resume_list()
		end
		-----------------------------------------------------------------------------}
	end
	
	if quit == 0 then
		for _, task in pairs(tasks) do
			if task.on_quit then
				task.on_quit(task)
			end
		end
	end
end

tasklet.EVT_READ = READ
tasklet.EVT_WRITE = WRITE
tasklet.EVT_ERROR = ERROR
tasklet.EVT_EDGE = EDGE

return tasklet
