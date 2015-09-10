
require 'std'
local urlparse = require 'urlparse'


local urls = {
	'ftp://root:admin@ftp.ba%69du.com:8080/%68%69/index.html?x=hello+world',
	'http://www.ba%69du.com:8080/%2f%69/index/?x=hello+world',
}
local buf = buffer.new()
for _, v in ipairs(urls) do
	print(v, '=>', '{')
	local t = urlparse.split(v)
	t.segments = urlparse.split_path(t.path)
	t.params = urlparse.split_query(t.query)
	dump(t)
	print(urlparse.build_path(t.segments))
	print('}', '<=', urlparse.build(t, buf):str())
	print('\n')
	buf:rewind()
end
print(urlparse.build_path({'节点1', '节点2', '叶子'}))
print(urlparse.build_query({x='值1', y='值2', z='/'}))
