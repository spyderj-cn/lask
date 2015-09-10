
require 'std'



local function wcache_flush(self)
	local wbuf = self.wbuf
	if wbuf and #wbuf > 0 then
		local nwrite, err = self.dowrite(wbuf)
		if nwrite > 0 then
			wbuf:shift(nwrite)
		else
			self.err = err
		end
	end
end
	
local function wcache_close(self)
	local fd = self.fd
	if fd and fd >= 0 then
		wcache_flush(self)
		os.close(fd)
		self.fd = -1
	end
end

local wcache_meta = {}
wcache_meta.__index = function (self, key)
	local buf_method = self.wbuf[key]
	if buf_method then
		local func = function (self, ...)
			local wbuf = self.wbuf
			buf_method(wbuf, ...)
			if #wbuf >= self.wbufsiz then
				local nwrite, err = self.dowrite(wbuf)
				if nwrite > 0 then
					wbuf:shift(nwrite)
				else
					self.err = err
				end
			end
			return self.err
		end
		rawset(self, key, func)
		return func
	end
end

function io.create_wcache(dowrite, wbufsiz)
	wbufsiz = wbufsiz or 4096
	local wcache = setmetatable({
		wbufsiz = wbufsiz,
		wbuf = buffer.new(wbufsiz),
		dowrite = dowrite,
		err = 0,
		fd = false,
		flush = wcache_flush,
		close = wcache_close,
	}, wcache_meta)
	
	if type(dowrite) == 'string' then
		local filepath = dowrite
		fs.unlink(filepath)
		local fd = os.creat(filepath, math.oct(644))
		if fd >= 0 then
			wcache.fd = fd
			wcache.dowrite = function (data)
				return os.writeb(fd, data)
			end
		else
			return nil
		end
	end
	
	return wcache
end

local wcache = io.create_wcache('test-os.tmp')
for i = 1, 1000000 do 
	wcache:putstr(tostring(i), ':', 'ddddddddddddddddddddddddddddddd\r\n')
end
dump(wcache)
wcache:close()
