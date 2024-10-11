# c-http

A simple HTTP server for static files. Uses single-threaded async/io on top of kqueue. Not
spec compliant at all -- my short-term goal is to implement some subset of HTTP 1.1. This whole
project is just for fun -- I gotta scratch my coding itch on paternity leave somehow.

## Getting Started

Execute `make` in the root directory to compile everything. Then run `./build/http` to start the
server. Execute `./build/http --help` for more options.

## References

- [MDN HTTP Resources & Specifications](https://developer.mozilla.org/en-US/docs/Web/HTTP/Resources_and_specifications)
