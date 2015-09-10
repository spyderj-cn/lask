
--
-- Copyright (C) Spyderj
--

local string, table, os = string, table, os
local tostring, tonumber, type = tostring, tonumber, type
local ipairs, pairs = ipairs, pairs

local log = require 'log'
local tasklet = require 'tasklet'

local app = {}

-- 
function app.start_tcpserver_task(addr, port, cb)
	addr = addr or '0.0.0.0'
	require 'tasklet.channel.streamserver'
	local ch_server = tasklet.create_tcpserver_channel(addr, port) 
					or log.fatal('failed to start tcp server on ', addr, ':', port)
	
	os.setcloexec(ch_server.ch_fd)
	return ch_server, tasklet.start_task(function ()
		while true do 
			local fd, addr, port, err = ch_server:accept(1)
			if fd >= 0 then
				os.setcloexec(fd)
				cb(fd, addr, port)
			end
		end
	end)
end

function app.start_unserver_task(path, cb)
	require 'tasklet.channel.streamserver'
	local ch_server = tasklet.create_unserver_channel(path) 
					or log.fatal('failed to start unix-domain server on ', path)
	
	os.setcloexec(ch_server.ch_fd)
	return ch_server, tasklet.start_task(function ()
		while true do 
			local fd = ch_server:accept(1)
			if fd >= 0 then
				os.setcloexec(fd)
				cb(fd)
			end
		end
	end)
end

-- there is a simple text protocol between the ctlserver and the ctltool
-- 
-- REQUEST = 
-- 		command \r\n
-- 		number of parameters \r\n
--		length of parameter1 \r\n
--		parameter1 \r\n
--		length of parameter2 \r\n 
--		...
--		parameterN
--
-- RESPONSE = 
--		errcode \r\n
--		number of return values \r\n
--		length of value1 \r\n
--		value1 \r\n
--		length of value2 \r\n
--		...
--		valueN \r\n
-- 
-- take ADD(1 + 20 + 300) for example 
--	SEND >>>
-- 		add \r\n
--		3 \r\n
--		1 \r\n
--		1 \r\n
--		2 \r\n
--		20 \r\n
--		3 \r\n
--		300 \r\n
--  RECV <<<
--		0 \r\n
--		1 \r\n
--		3 \r\n
--		321 \r\n
function app.start_ctlserver_task(path, commands)
	require 'tasklet.channel.streamserver'
	require 'tasklet.channel.stream'
	
	local fd = -1
	
	-- XXX: there may be some risk to share one write-buffer among all the connections
	local tmpbuf = tmpbuf
	
	local builtin_commands = {
		logread = function (argv)
			return 0, log.read(tonumber(argv[1]))
		end,
		
		logreopen = function (argv)
			log.reopen()
			return 0
		end,
		
		logcapture = function (argv)
			local sockpath = argv[1]
			if sockpath then
				return log.capture(sockpath, argv[2] or 'debug') and 0 or errno.EAGAIN
			else
				return errno.EINVAL
			end
		end,
		
		logrelease = function ()
			log.release()
			return 0
		end,
	}
	
	-- {}  					-> logread enabled (defaulted)
	-- {logread=false}  	-> logread disabled
	-- {logread=true/...}	-> logread enabled (defaulted)
	-- {logread=function}	-> logread enabled (customized)
	for k, v in pairs(builtin_commands) do 
		if commands[k] == nil or (commands[k] and type(commands[k]) ~= 'function') then
			commands[k] = v
		end
	end
	
	local function getreq(ch)
		local cmd = ch:read()
		local argc = ch:read()
		argc = tonumber(argc)
		
		if not cmd or not argc then
			return
		end
		
		local argv = {}
		for i = 1, argc do 
			local len = ch:read()
			len = tonumber(len)
			if not len then return end
			
			local rd = ch:read(len)
			if not rd then return end
			
			table.insert(argv, rd:str())
			ch:read()
		end
		return cmd, argv
	end
	
	local function putresp(ch, err, retv)
		tmpbuf:rewind():putstr(tostring(err), '\r\n')
		local rtype = type(retv)
		if rtype == 'nil' then
			tmpbuf:putstr('0\r\n')
		elseif rtype ~= 'table' then
			retv = tostring(retv)
			tmpbuf:putstr('1\r\n', #retv, '\r\n', retv, '\r\n')
		else
			tmpbuf:putstr(#retv, '\r\n')
			for _, v in ipairs(retv) do 
				v = tostring(v)
				tmpbuf:putstr(#v, '\r\n', v, '\r\n')
			end
		end
		
		-- The following is a little bit UGLY.
		-- 'tmpbuf' is used so we can't block the task here(and this is unlike to happen when data is transfered
		-- through unix-domain socket), thus  we use os.writeb instread of ch:write.
		local all = #tmpbuf
		local done = 0
		local count = 0
		while done < all and count < 3 do
			local n, err = os.writeb(ch.ch_fd, tmpbuf, done)
			if err ~= 0 then
				break
			end
			done = done + n
			if err ~= 0 then
				time.sleep(0.1)
				count = count + 1
			end
		end
	end
	
	local function ctl_loop(fd)
		local ch = tasklet.create_stream_channel(fd, {rbufmax=1024})
		local captured = false
		while true do 
			local cmd, argv = getreq(ch)
			if not cmd then
				break
			end
			
			local err, retv = errno.ENOSYS
			if commands[cmd] then
				err, retv = commands[cmd](argv)
			end

			if err == 0 then
				if cmd == 'logcapture' then
					captured = true
				elseif cmd == 'logrelease' then
					captured = false
				end
			end
			
			putresp(ch, err, retv)
		end
		ch:close()
		if captured then
			log.release()
		end
	end
	
	if not path:find('/') then
		path = '/tmp/' .. path .. '.sock'
	end
	return app.start_unserver_task(path, ctl_loop)
end

function app.run()
	local appname = app.APPNAME
	local pidfile = appname and '/tmp/' .. appname .. '.pid'

	if pidfile then 
		local pid = os.getpid()
		local file = io.open(pidfile, 'w')
		if file then
			file:write(pid)
			file:close()
		end
	end
	
	local function on_sigquit() tasklet.quit() end
	local function on_sigterm() tasklet.term() end
	signal.signal(signal.SIGINT, on_sigterm)
	signal.signal(signal.SIGTERM, on_sigterm)
	signal.signal(signal.SIGQUIT, on_sigquit)
	
	local ok, msg = xpcall(tasklet.loop, debug.traceback)
	local exitcode = 0
	if not ok then
		if app.DEBUG then
			print(msg)
		elseif appname then
			local TRACEBACK_FILE = '/tmp/' .. appname .. '.traceback'
			local LOGDUMP_FILE = '/tmp/' .. appname .. '.logdump'
			local file = io.open(TRACEBACK_FILE, 'w')
			if file then
				file:write(msg)
				file:close()
			end
			
			local logs = log.read()
			if logs and #logs > 0 then
				file = io.open(LOGDUMP_FILE, 'w')
				if file then
					for _, v in ipairs(logs) do 
						file:write(v)
					end
					file:close()
				end
			end
		
			if app.APPCTL then
				time.sleep(15)
				if os.fork() == 0 then
					time.sleep(0.5)  -- Let the parent go first.
					os.execute(app.APPCTL .. ' start >/dev/null 2>&1 &')
					os.exit(0)
				end
			end
		end
		exitcode = 1
	end

	if pidfile then
		fs.unlink(pidfile)
	end
	return exitcode
end

return app

