# mini-http-server

### A tiny HTTP Server with Winsock written in C.

## Description

This is a simple HTTP Server that responds to `GET` requests only. The `HTTP Response` header may contain some of status code below:

* 200 OK
* 404 Not Found
* 405 Method Not Allowed
* 500 Internal Server Error

To attend many clients simultaneously, the server handles multiple socket connections (`select` method).

## Platforms

* Windows 10