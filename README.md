# c-http

A simple HTTP server for static files. Uses single-threaded async/io on top of kqueue. Not truly
spec compliant at all -- my short-term goal is to implement some subset of HTTP 1.1. This whole
project is just for fun -- I gotta scratch my coding itch on paternity leave somehow.

## Getting Started

Execute `make` in the root directory to compile everything. Then run `./build/http` to start the
server. Execute `./build/http --help` for more options.

## Benchmarks

(Executed with logging turned off, and `-O2` optimization levels):

```
rewrk -h http://localhost:8080/ -t 12 -c 60 -d 1s
Beginning round 1...
Benchmarking 60 connections @ http://localhost:8080/ for 1 second(s)
  Latencies:
    Avg      Stdev    Min      Max
    0.50ms   0.20ms   0.02ms   4.46ms
  Requests:
    Total: 119775  Req/Sec: 120572.69
  Transfer:
    Total: 5.83 MB Transfer Rate: 5.86 MB/Sec
```

## References

- [MDN HTTP Resources & Specifications](https://developer.mozilla.org/en-US/docs/Web/HTTP/Resources_and_specifications)
