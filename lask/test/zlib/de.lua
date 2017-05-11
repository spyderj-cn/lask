
local help = [[

1. generate random data(of arg[2] bytes) and write it into 'de.old'
2. use zlib API to deflate the original data and write it into 'de.gz'
3. use 'gunzip' to inflate 'de.gz', write the result into 'de.new'
4. use 'diff' to compare 'de.new' and 'de.old', assert the result.

arg[1] deflating level, defaulted to -1
arg[2] original data size in bytes(unit 'k'), defaulted to 1024(=> 1M)
]]
if arg[1] == 'help' then
    print(help)
    os.exit(0)
end

local LEVEL = tonumber(arg[1]) or -1
local MAX_SIZE = tonumber(arg[2]) or 1024

local z = require 'zlib'

local input = buffer.new()
local output = buffer.new()
local fd = os.creat('de.old', math.oct(664))
local zdata = z.deflate_init(nil, LEVEL)

for i = 1, MAX_SIZE do
    local str = math.randstr(32)
    for j = 1, 32 do
        input:putstr(str)
    end
    os.writeb(fd, input)
    z.deflate(zdata, input, output, i == MAX_SIZE and 0 or 1)
    input:rewind()
end
os.close(fd)
z.deflate_end(zdata)

print(string.format('compression rate: %d%%', 100 - math.floor(#output / MAX_SIZE / 10.24)))

fd = os.creat('de.gz', math.oct(664))
os.writeb(fd, output)
os.close(fd)

local unzip_ok = os.execute('gunzip -c de.gz > de.new')
local diff_ok = os.execute('diff de.old de.new')

fs.unlink('de.gz')
fs.unlink('de.old')
fs.unlink('de.new')

assert(unzip_ok, 'gunzip error when uncompressing the deflated data')
assert(diff_ok, 'uncompressed data is different from the original')


--
local zstream = z.deflate_init()
print(z.deflate(zstream, buffer.new(), tmpbuf:rewind(), 0))
z.deflate_end(zstream)
print(tmpbuf)
