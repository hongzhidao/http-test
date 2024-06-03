# HTTP Test Tool

[![License](https://img.shields.io/badge/license-BSD%202--Clause-blue.svg)](LICENSE)

HTTP Test Tool is a tool for testing the performance of HTTP servers, supporting Linux's epoll. It allows users to set the number of threads, connections, and test duration, and outputs detailed test results.

## Features

- Supports customizing request through Lua script
- Supports multi-threaded and multi-connection testing
- Uses epoll to improve performance
- Outputs detailed test results (requests, transfer rate, latency, etc.)

## Requirements

- Linux operating system
- `make` tool

## Installation

Follow these steps to install HTTP Test Tool:

```bash
git clone https://github.com/hongzhidao/http-test.git
cd http-test && make
```

## Scripting

The `http` table is pre-populated with the values from command line arguments.
- `http.method`: Set the HTTP method.
- `http.host`: Set the target host.
- `http.path`: Set the request path.
- `http.headers`: Set the request headers, which can overfide `http.host`.
- `http.body`: Set the request body.

Note:
- `http.headers` can override `http.host`, so if `Host` is set in the headres, it will override the value of `http.host`.
- You can enable chunked transfer encoding by setting `http.headers["Transfer-Encoding"] = "chunked"`.

## Usage

Run HTTP Test Tool and view the usage help:

```bash
./test -h
```

Output:

```plaintext
Usage: ./test [-t value] [-c value] [-d value] [-H header] [-s script] [-v] [-h] url
Options:
 -t value   Set the value of threads
 -c value   Set the value of connections
 -d value   Set the value of duration
 -H header  Set the request header
 -s file    Set the script file
 -v         print the version information
 -h         print this usage message
 url        The required URL to test
```

Example:

```bash
./test http://127.0.0.1/50k.html
```

Output example:

```plaintext
Testing 2 threads and 10 connections
@ http://127.0.0.1/50k.html for 10s

260849 requests and 233.83M bytes in 10.00s
  Requests/sec  26084
  Transfer/sec  23.38M

Latency:
  Mean      353.00us
  Stdev     73.00us
  Max       2.85ms
  +/-Stdev  81.23%

Latency Distribution:
  50%  341.00us
  75%  382.00us
  90%  433.00us
  99%  601.00us
```

## Contributing

If you would like to contribute to this project, please follow these steps:

1. Fork the repository.
2. Create a new branch (`git checkout -b feature-branch`).
3. Commit your changes (`git commit -m 'Add some feature'`).
4. Push to the branch (`git push origin feature-branch`).
5. Open a Pull Request.

## License

This project is licensed under the BSD 2-Clause License. See the [LICENSE](LICENSE) file for details.

---

If you have any questions or suggestions, feel free to contact me (hongzhidao@gmail.com).
