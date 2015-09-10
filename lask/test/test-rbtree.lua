require 'std'

local tree = rbtree.new()

--[[
for i = 100, 1000000  do 
	tree:insert(i, i)
end
--tree:dump()
--
stdstat()
print(collectgarbage('count'), 'KBytes')


for i = 1, 100 do
tree:offsetkey(1)
end
--tree:dump()
]]
print('init:', tree)


for i = 1, 10 do 
	tree:insert(i, i + 10000)
	tree:insert(i, i + 20000)
end

print('build:', tree)

tree:del(2, 30002)
print('del(2, 30002)', tree)

tree:del(2, 20002)
print('del(2, 20002)', tree)

tree:del(7)
print('del(7)', tree)


tree:del(100)
print('del(100)', tree)

while true do
	local key, value = tree:delmin(7)
	if not key or not value then break end
	print(key, value)
end

print('while true delmin(7)', tree)

