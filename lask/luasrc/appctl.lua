
--
-- Copyright (C) Spyderj
--

require 'std'

local appctl = {}

local fd = -1
local tmpbuf = tmpbuf 
local stdin = io.stdin
local stdout = io.stdout
local stderr = io.stderr

local function putreq(cmd, argv)
	tmpbuf:rewind():putstr(cmd, '\r\n')
	if argv == nil then
		tmpbuf:putstr('0\r\n')
	elseif type(argv) ~= 'table' then
		argv = tostring(argv)
		tmpbuf:putstr('1\r\n', #argv, '\r\n', argv, '\r\n')
	else
		tmpbuf:putstr(#argv, '\r\n')
		for _, v in ipairs(argv) do 
			v = tostring(v)
			tmpbuf:putstr(#v, '\r\n', v, '\r\n')
		end
	end
	
	local nwritten, err = os.writeb(fd, tmpbuf)
	return err
end

local function getresp()
	local nread, err = os.readb(fd, tmpbuf:rewind())
	if err ~= 0 then
		return
	end

	local err = tonumber(tmpbuf:getline())
	local retc = tonumber(tmpbuf:getline())

	if not err or not retc then
		return
	end
	
	local retv = {}
	for i = 1, retc do 
		local len = tonumber(tmpbuf:getline())
		if not len then return end
		
		local ret = tmpbuf:getlstr(len)
		if not ret then return end
		
		table.insert(retv, ret)
		tmpbuf:getline()
	end
	return err, retv
end

function appctl.invoke(cmd, argv)
	if fd < 0 then
		fd = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
		local err = socket.connect(fd, '/tmp/' .. appctl.APPNAME .. '.sock')
		if err ~= 0 then
			stderr:write('failed to connect ', appctl.APPNAME, ', abort\n')
			os.exit(1)
		end
	end
	
	putreq(cmd, argv)
	if not poll.waitrfd(fd, 3) then
		stderr:write('timed out for the response, abort\n')
		os.exit(1)
	end
	
	local err, retv = getresp()
	if not err then
		stderr:write('connection error, abort\n')
		os.exit(1)
	end
	
	return err, retv
end

local capturing = false
local builtin_commands = {
	help = function () 
		stdout:write(appctl.HELP, '\n') 
	end,
	
	exit = function () 
		os.exit(0) 
	end,
	
	logcapture = function (argv)
		local err
		local pid = os.getpid()
		local sockpath = '/tmp/' .. pid .. '.sock'
		
		local fd = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
		err = socket.bind(fd, sockpath)
		if err ~= 0 then
			stderr:write('unable to start unix-domani server on ', sockpath)
			os.exit(1)
		end
		socket.listen(fd)
		
		argv[2] = argv[1]
		argv[1] = sockpath
		err = appctl.invoke('logcapture', argv)
		if err ~= 0 then
			os.close(fd)
			stderr:write('[', errno.strerror(err), ']\n')
			os.exit(1)
		end
		
		if not poll.waitrfd(fd, 3) then
			stderr:write('[Unknown error]\n')
			os.close(fd)
			os.exit(1)
		end
		
		local peerfd = -1
		peerfd = socket.accept(fd)
		os.close(fd)
		fd = peerfd
		
		stdout:write('<<< Press Enter to quit the capture >>>\n')
		tmpbuf:rewind()
		local quit = false
		while not quit do 
			for _, xfd in ipairs(poll.select({0, fd}, nil, nil, 1) or NULL) do 
				if xfd == 0 then 
					stdin:read('*line')
					quit = true
					break
				elseif xfd == fd then
					local nread, err = os.readb(fd, tmpbuf)
					if nread > 0 then
						os.writeb(1, tmpbuf)
						tmpbuf:rewind()
					else
						quit = true
						break
					end
				end
			end
		end
		appctl.invoke('logrelease')
		os.close(fd)
	end,
	
	start = function ()
		local appname = appctl.APPNAME
		local bugreport_url = appctl.BUGREPORT_URL
		local pidfile = '/tmp/' .. appname .. '.pid'
		
		-- may be spawned by the app itself, wait a while for the app to gracefully exit.
		if fs.access(pidfile, fs.F_OK) == 0 then
			time.sleep(0.3)
			os.execute('/etc/init.d/' .. appname .. ' stop')
			time.sleep(0.3)
			fs.unlink(pidfile)
		end
		
		local TRACEBACK_FILE = '/tmp/' .. appname .. '.traceback'
		local LOGDUMP_FILE = '/tmp/' .. appname .. '.logdump'
		if fs.access(TRACEBACK_FILE, fs.R_OK) == 0 and bugreport_url then
			local file = io.popen(appname .. ' -v', 'r')
			if file then
				local version = file:read('*all')
				file:close()
				if version then
					version = version:match('(%S+)')
					local file = io.popen('ajax >/dev/null 2>&1', 'w')
					if file then
						local settings = {
							url = bugreport_url, 
							type = 'POST',
							data = {
								['$FILE$_traceback'] = TRACEBACK_FILE,
								version = version,
								appname = appname,
								['$FILE$_logdump'] = fs.access(LOGDUMP_FILE, fs.R_OK) == 0 and LOGDUMP_FILE or nil
							}
						}
						file:write(require('cjson').encode(settings))
						file:close()
					end
				end
			end
			fs.unlink(TRACEBACK_FILE)
			fs.unlink(LOGDUMP_FILE)
		end
		os.execute('/etc/init.d/' .. appname .. ' start')
	end
}

local no_nextline = {
	logread = true,
}

appctl.retv_printers = {}

function appctl.dispatch(argv)
	local cmd = argv[1]
	table.remove(argv, 1)
	
	if builtin_commands[cmd] then
		builtin_commands[cmd](argv)
	else
		local nextline = not no_nextline[cmd]
		local err, retv = appctl.invoke(cmd, argv)
		stderr:write('[', errno.strerror(err), ']\n')
		if appctl.retv_printers[cmd] then
			appctl.retv_printers[cmd](retv)
		else
			for _, v in ipairs(retv) do 
				stdout:write(v, nextline and '\n' or '')
			end
		end
	end
end


function appctl.interact()
	local function sigint()
		stdout:write('\nInput "exit" or Ctrl+D to quit the interaction\n')
		stdout:write(">>> ")
	end

	stdout:setvbuf("no")
	signal.signal(signal.SIGINT, sigint)
	while true do
		stdout:write(">>> ")
		local line = stdin:read("*line")
		if not line then 
			print("")
			break 
		end
		
		local argv = {}
		for matched in line:gmatch("(%S+)") do
			table.insert(argv, matched)
		end
		
		if #argv > 0 then
			appctl.dispatch(argv)
		end
	end
end

return appctl

