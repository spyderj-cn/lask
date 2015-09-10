#!/usr/bin/lua

require 'std'
local cjson = require 'cjson'
local tasklet = require 'tasklet'
local http = require 'httpx'

local ok, settings = pcall(cjson.decode, io.stdin:read('*a'))
if ok then
	local opts = getopt(arg, 'o:', {'--output='})
	local output = opts.o or opts.output
	local file = io.stdout
	
	if output then
		file = io.open(output, 'w') or io.stdout
	end
	
	settings.error = function (resp, err)
		if err < 0 then
			io.stderr:write(errno.strerror(-err), '\n')
		else
			io.stderr:write(tostring(err), ' ', resp.reason, '\n')
		end
		tasklet.term()
	end
	
	settings.success = function (data)
		file:write(tostring(data))
		file:flush()
		tasklet.term()
	end

	http.ajax(settings)
	tasklet.loop()
else
	io.stderr:write(settings, '\n')
end
