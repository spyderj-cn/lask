
local tasklet = require 'tasklet'
local app = require 'app'

local http = require 'httpd'

print(fs.getcwd())

local t = {}
local val = string.rep('a', 100)
for i = 1, 1000 do
	t[math.randstr(100)] = val
end

app.APPNAME = 'test-httpd'
app.run({d=true}, function ()
	dump(http.start_server({
		port = 8088,
		doc_root = fs.getcwd() .. '/',
		doc_pattern = '/static/(.+)',
		follow_link = true,
		handler = function (req)
			local path = req.urlinfo.path:match('^/([^/]*)')
			if path == 'plaintext' then
				http.set_content_type('text/plain')
				for i = 1, 1000 do
					http.echo(math.randstr(100))
				end
				http.die()
			elseif path == 'json' then
				http.echo_json(t)
			elseif path == 'redirect' then
				http.redirect('http://www.qq.com')
			else
				return true
			end
		end,
	}))
end)
