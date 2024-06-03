--[[ Custom HTTP request example using Lua.
  This script demonstrates how to define a custom
  HTTP POST request with headers and chunked body.
]]
http.method = "POST"
http.host = "example.com"
http.path = "/foo"
http.headers["Host"] = "another.com" -- override http.host
http.headers["X-Foo"] = "foo"
http.headers["X-BAZ"] = "baz"
http.headers["Transfer-Encoding"] = "chunked"
http.body = "hello world"
