--[[ Custom HTTP request example using Lua.
  This script demonstrates how to define a custom
  HTTP POST request with headers and chunked body.
]]

http.method = "POST"
http.path = "/foo"
http.headers["Host"] = "example.com"
http.headers["X-Foo"] = "foo"
http.headers["X-BAZ"] = "baz"
http.headers["Transfer-Encoding"] = "chunked"
http.body = "hello world"

local num = 0
http.request = function()
    num = num + 1
    local r = {}
    r.headers = {}
    r.headers["X-ID"] = num
    return r
end
