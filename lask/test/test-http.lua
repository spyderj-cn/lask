
require 'std'
local http = require 'http'

local req = http.new_request()
local buf = buffer.new()

req.method = 'POST'
req.host = 'www.baidu.com'
req.urlpath = '/ajax/status/floating'
req:serialize({
	zh = '中文', 
	en = 'english',
	number = 1,
	['$FILE$_file'] = '/home/jike/luastd/test.lua',
	['$FILE$_filelist'] = {
		'/home/jike/luastd/_std.so',
		'/home/jike/luastd/test.txt'
	},
	c = 3,
}, buf)
os.writeb(1, buf)

--[[
local ch = reactor.create_stream_channel(1)
local req = http.new_request()

req.method = 'POST'
req.urlpath = '/index'
req.headers['Host'] = 'www.baidu.com'

req:write(ch, {
	a = 1,
	b = 2,
})
]]

--[[
local function req_unittest()
	local client = require '_lisa.client'
	client = client.create()
	client:prepare({Host='www.qq.com'})
	local reqbuf = client:post({path='/'}, nil, {
		name1='值1', 
		names2='值2', 
		[mime.FILE_FIELD_PREFIX .. 'name3'] = '/home/jike/test/gentok', 
		[mime.FILE_FIELD_PREFIX .. 'name4'] = {'/home/jike/test/test.lua', '/home/jike/test/test.py'},
	})
	local wbuf = io.buffer()
	multipart.upload_dir = '/home/jike/test/upload'
	for i = 1, 1000 do 
		local req = req_new()
		wbuf:putreader(reqbuf:reader())
		assert(req_parse(req, wbuf:reader()) == 0)
		wbuf:rewind()
	end
end
]]
