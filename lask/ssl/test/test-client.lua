require 'std'
local log = require 'log'
local ssl = require 'ssl'

local ctx = ssl.context.new('sslv23') or log.fatal('failed to create ssl context ', ssl.error_string())

local baidu = netdb.getaddrbyname('mail.aliyun.com')[1] or log.fatal('DNS error')
local fd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
if socket.connect(fd, baidu, 443) ~= 0 then
	log.fatal('unable to connect www.github.com:443')
end

if not ssl.attach(fd, ctx) then
	log.fatal('ssl.attach failed')
end

assert(ssl.isattached(fd))

ssl.set_connect_state(fd)

err = ssl.do_handshake(fd)
if err == ssl.ERROR_SYSCALL then
	log.fatal(ssl.error_string(), ', ', errno.strerror())
end

local buf = buffer.new()
local http = require 'http'
local req = http.new_request()
req.host = 'github.com'
req.urlpath = '/'
req:serialize(nil, buf)
os.writeb(1, buf)
print(ssl.writeb(fd, buf))

buf:rewind()
ssl.readb(fd, buf)
ssl.readb(fd, buf)

os.writeb(1, buf)
ssl.shutdown(fd)

ssl.detach(fd)


