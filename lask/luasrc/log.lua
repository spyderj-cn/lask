
--
-- Copyright (C) Spyderj
--


require 'std'

local TO_STDOUT	= 1
local TO_FILE	= 2
local TO_MEMORY	= 3
local TO_SOCKET	= 4
local TO_SYSLOG = TO_STDOUT -- TODO

local LEVEL_DEBUG = 1
local LEVEL_INFO = 2
local LEVEL_WARN = 3
local LEVEL_ERROR = 4
local LEVEL_FATAL = 5

local COLOR_SUFFIX = "\027[0m"
local LEVEL_COLOR_PREFIX = {
	"\027[37;2m",
	"\027[32m",
	"\027[33m",
	"\027[31m",
	"\027[41;37;1m",
}

local LEVEL_NAME = {
	"debug",
	"info",
	"warn",
	"error",
	"fatal",
}

local LEVEL_NAME_PREFIX = {
	'debug*** ',
	'info *** ',
	'warn *** ',
	'error*** ',
	'fatal*** ',
}

local log_type = os.isatty(1) and TO_STDOUT or TO_SYSLOG
local log_level = LEVEL_DEBUG
local log_path
local log_withcolor = true
local log_withtime = true
local log_fd = -1
local log_buf = buffer.new(1024)
local log_inited = false	

local ring = false
local ring_num = 0
local ring_max = 0
local ring_idx = 1

local bak_fd = -1
local bak_level
local bak_type

local log = {} 

local function parse_level(level)
	if type(level) == "string" then
		if tonumber(level) then
			level = tonumber(level)
		else
			level = string.lower(level)
			for i, v in ipairs(LEVEL_NAME) do
				if v:find(level) == 1 then
					level = i
					break
				end
			end
			if type(level) == "string" then
				level = LEVEL_INFO
			end
		end
	end
	if level < LEVEL_DEBUG or level > LEVEL_FATAL then
		level = LEVEL_INFO
	end
	return level
end

-- log system will fallback to the default type if type is not specified or 
-- we failed to initialize.
-- 
-- what the default type is depends how the application is running.
-- if daemonized(os.isatty(1) returns false), we use 'memory', otherwise
-- we use 'stdout'
--
-- thus, log.init must be called AFTER daemonized.
function log.init(conf)
	if log_inited then
		return 
	end
	
	if conf then
		if conf.level ~= nil then
			log_level = parse_level(conf.level)
		end
		if conf.withcolor ~= nil then
			log_withcolor = conf.withcolor
		end
		if conf.withtime ~= nil then
			log_withtime = conf.withtime
		end
		if conf.path ~= nil then
			log_path = string.lower(conf.path)
		end
	end

	if log_path then
		if log_path == 'stdout' then
			log_type = TO_STDOUT
		elseif log_path == 'syslog' then
			log_type = TO_SYSLOG
		elseif log_path:find('memory') then
			ring_max = tonumber(log_path:match('memory:(%d+)')) or 200
			ring = table.array(ring_max)
			log_type = TO_MEMORY 
		else
			log_type = TO_FILE
			log_fd = os.open(log_path, os.O_WRONLY + os.O_APPEND)
			if log_fd < 0 then
				log_fd = os.creat(log_path, math.oct(644))
			end
		end
	end
	
	if not log_path or (log_type == TO_FILE and log_fd < 0) then
		if os.isatty(1) then
			log_type = TO_STDOUT
		else
			ring_max = 200
			ring = table.array(ring_max)
			log_type = TO_MEMORY
		end
	end
	
	log_inited = true
end

function log.read(num)
	if not ring then return NULL end
	
	local start 
	
	if not num or num > 0 then
		if not num or num > ring_num then
			num = ring_num
		end

		if ring_num == ring_max then
			start = ring_idx
		else
			start = 1
		end
	elseif num < 0 then
		if num < -ring_num then
			num = -ring_num
		end
		
		num = -num
		start = ring_idx - num
		if start < 1 then
			start = start + ring_max
		end
	else
		return NULL
	end
	
	local ret = table.array(num)
	local idx
	for i = 1, num do 
		idx = start + i - 1
		if idx > ring_max then
			idx = idx - ring_max
		end
		ret[i] = ring[idx]
	end
	return ret
end

-- reopen the logging file (for log-splitting)
function log.reopen()
	if log_type == TO_FILE then
		os.close(log_fd)
		log_fd = os.open(log_path, os.O_WRONLY + os.O_APPEND)
		if log_fd < 0 then
			log_fd = os.creat(log_path, math.oct(644))
		end
	end
end

function log.capture(sockpath, level)
	if log_type ~= TO_SOCKET then
		local fd = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
		if socket.connect(fd, sockpath) == 0 then
			bak_fd = log_fd
			bak_type = log_type
			bak_level = log_level
			log_fd = fd
			log_level = parse_level(level)
			log_type = TO_SOCKET
			return true
		else
			os.close(fd)
		end
	end
end

function log.release()
	if log_type == TO_SOCKET then
		os.close(log_fd)
		log_fd = bak_fd
		log_level = bak_level
		log_type = bak_type
	end
end

local buf_putstr = buffer.putstr
local time_strftime = time.strftime
local time = time.time
local os = os

local function _log(level, ...)
	local withcolor = false
	local buf = log_buf

	if (log_type == TO_STDOUT or log_type == TO_SOCKET) and log_withcolor then
		withcolor = true
	end
	
	if withcolor then
		buf_putstr(buf, LEVEL_COLOR_PREFIX[level])
	end
	if log_withtime then
		buf_putstr(buf, time_strftime('%m-%d %H:%M:%S ', time()))
	end
	
	buf_putstr(buf, LEVEL_NAME_PREFIX[level])
	buf_putstr(buf, ...)
	if withcolor then
		buf_putstr(buf, COLOR_SUFFIX)
	end
	buf_putstr(buf, '\n')
	
	if log_type == TO_STDOUT then
		os.writeb(1, buf)
	elseif log_type == TO_MEMORY then
		ring[ring_idx] = buf:str()
		ring_idx = ring_idx + 1
		if ring_idx > ring_max then
			ring_idx = 1
		elseif ring_num < ring_max then
			ring_num = ring_num + 1
		end
	elseif log_fd >= 0 then
		os.writeb(log_fd, buf)
	end
	buf:rewind()
end

function log.debug(...)
	if log_level <= LEVEL_DEBUG then
		_log(LEVEL_DEBUG, ...)
	end
end

function log.info(...)
	if log_level <= LEVEL_INFO then
		_log(LEVEL_INFO, ...)
	end
end

function log.warn(...)
	if log_level <= LEVEL_WARN then
		_log(LEVEL_WARN, ...)
	end
end

function log.error(...)
	if log_level <= LEVEL_ERROR then
		_log(LEVEL_ERROR, ...)
	end
end

function log.fatal(...)
	_log(LEVEL_FATAL, ...)
	os.exit(1)
end

return log
