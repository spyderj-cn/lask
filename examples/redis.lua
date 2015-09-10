-------------------------------------------------------------------------------
--
-- async redis client
--
-------------------------------------------------------------------------------

local pairs, ipairs, rawget, rawset = pairs, ipairs, rawget, rawset
local tonumber, tostring, type = tonumber, tostring, type

local tasklet = require 'tasklet'
require 'tasklet.channel.stream'
require 'tasklet.service'

local ECONNREFUSED, EILSEQ = errno.ECONNREFUSED, errno.EILSEQ

local readresp

local resp_readers = {
	[string.byte('+')] = function (ch)
		return true, ch:read()
	end,
	
	[string.byte('-')] = function (ch)
		return false, ch:read()
	end,
	
	[string.byte(':')] = function (ch)
		local val = tonumber(ch:read())
		return val, val
	end,
	
	[string.byte('$')] = function (ch)
		local len = ch:read()
		local len = tonumber(len)
		if len then
			if len > 0 then
				local rd = ch:read(len) 
				if rd then
					return rd:str(), ch:read(2) -- XXX: use read(2) to skip '\r\n'
				end
			else
				return nil, true 
			end
		end
		return nil, false
	end,
	
	[string.byte('*')] = function (ch)
		local num = ch:read()
		num = tonumber(num)
		if not num or num < 0 then 
			return nil, false 
		end
		
		local retv
		if num > 0 then
			retv = table.array(num)
			for i = 1, num do
				local ret, err = readresp(ch)
				if err ~= 0 then
					return nil, false
				end
				retv[i] = ret
			end
		end
		return retv, true
	end,
}

readresp = function (ch)
	local rd = ch:read(1)
	local prefix = rd and rd:getc()
	local func = prefix and resp_readers[prefix]
	
	if func then
		return func(ch)
	else
		return nil, false 
	end
end

local function sendreq(ch, buf, argv)
	local argc = #argv
	
	buf:reset()
	buf:putstr('*', tostring(argc), "\r\n")
	for _, v in ipairs(argv) do
		if type(v) == 'userdata' then
			buf:putstr('$', #v, '\r\n')
			buf:putreader(v)
			buf:putstr('\r\n')
		else
			v = tostring(v)
			buf:putstr("$", tostring(#v), "\r\n", v, "\r\n")
		end
	end
	return ch:write(buf) == 0
end

local function connect(ip, port)
	ip = ip or '127.0.0.1'
	port = port or 6379
	
	local ch = tasklet.create_stream_channel()
	local buf = buffer.new(1024)	
	local ts_prevconn = 0
	
	return setmetatable(tasklet.create_service('redis', function (svc, argv)
			local retv, err = nil, ECONNREFUSED
			local fd = ch.ch_fd
			if fd < 0 then
				local diff = tasklet.now - ts_prevconn
				if diff < 0 then
					ts_prevconn = tasklet.now
				elseif diff > 1 then
					ts_prevconn = tasklet.now
					ch:connect(ip, port)
					fd = ch.ch_fd
				end
			end
			if fd >= 0 then
				if sendreq(ch, buf, argv) then
					retv, err = readresp(ch)
					err = err and 0 or EILSEQ
				end
				
				if ch.ch_state <= 0 then
					ch:close()
				end
			end
			return retv, err
		end), {
		__index = function (self, key)
			if commands[key] then
				local function caller(self, ...)
					return tasklet.request(self, {key, ...}, 3) -- 3 seconds before timed out
				end
				rawset(self, key, caller)
				return caller
			end
		end,
	})
end

return {
	connect = connect,
}
