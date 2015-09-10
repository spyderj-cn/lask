require 'std'

local buf = buffer.new()
buf:loadfile('/proc/net/route')
print(buf)


local rd = buf:reader()

print(buf)

for i = 1, 100 do 
	rd:getline()	
end

buf:shift(rd)

print(buf)
os.writeb(1, buf)


local a = {
	x = 1,
	y = 2,
	z = false,
}

local b = {
	x = 1, 
	y = 2, 
	z = a,
	b = buf,
	r = rd,
}

a.z = b

dump(a)
dump(b)

local node = {
	
}

node.prev = node
node.next = node
dump(node)

