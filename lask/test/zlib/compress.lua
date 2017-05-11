
local help = [[
 1. 'infile' -> out.gz
 2. gunzip out.gz -> out
 3. assert out is the same as 'infile'

 4. out.gz -> out
 5. assert out is the same as 'infile'

 arg[1]  input file path, defaulted to /usr/include/unistd.h
]]
if arg[1] == 'help' then
    print(help)
    os.exit(0)
end

require 'std'
local zlib = require 'zlib'
local infile = arg[1] or '/usr/include/unistd.h'
local buf = buffer.new()

-- step 1-3, test compress
local fd = os.open(infile, os.O_RDONLY)
assert(fd >= 0, 'failed to open ' .. infile)
os.readb(fd, buf)
os.close(fd)
assert(zlib.compress(buf) == 0)
file_put_content('out.gz', buf:str())
os.execute('gunzip -c out.gz > out')  -- don't remote out.gz
local the_same = os.execute('diff out ' .. infile)
assert(the_same)
fs.unlink('out')


-- step 4-5, test uncompress
local fd = os.open('out.gz', os.O_RDONLY)
assert(fd >= 0, 'failed to open out.gz')
os.readb(fd, buf:rewind())
os.close(fd)
zlib.uncompress(buf)
file_put_content('out', buf:str())
local the_same = os.execute('diff out ' .. infile)
assert(the_same)
fs.unlink('out')
fs.unlink('out.gz')
