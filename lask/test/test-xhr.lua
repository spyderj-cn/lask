
require 'std'
local reactor = require 'reactor'
local xhr = require 'xhr'

reactor.init()

xhr.ajax({
	type = 'get',
	url = '172.16.21.35:9527/ajax/status/floating',
})

reactor.loop()

