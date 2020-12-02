# cygwin_unix
Attempt to narrow down bug that I suspect lies with Unix Domain sockets under Cygwin.

Under certain circumstances, a server will block in accept() when it should not.

Defining "certain circumstances" is part of the challenge. The problem seems to
be deterministic for any particular program, but seemingly innocuous changes in
the client can alter the behavior of the server.

So far, this problem has only been isolated when using non-blocking sockets on
both the server and the client.

The program two_unix_sockets includes both the server and client code. You should
first run the server in one terminal and then run the client in another:

$ ./two_unix_sockets server

$ ./two_unix_sockets client

If both programs run to completion, the bug was not observed. If they both hang,
the bug has been tripped.

As of cc684aa, this code consistently trips the bug on my system. The server creates
a non-blocking listening socket and then calls select(), which should report the
socket as "read-ready" when a connection request arrives.

When the client is started, it creates its own non-blocking socket, and attempts to
connect() to the server. The server's select() call returns, indicating that the
listening socket is "read-ready". Then the server calls accept(), which then blocks.

With some variations, accept() will return if the client is terminated (e.g. via ctrl-C).

### Update 12/1/2020
As of [fdbe7ed](https://github.com/nthallen/cygwin_unix/commit/fbde7ed94c85a7ef569f4b24f06db03b99a955a9),
this no longer trips the bug on my system. Unfortunately the bugs fixed here are not
present in my much larger application. I will try to further replecate the key pieces
of that application here.

### Update 12/2/2020
After paring the main application down and back up, I finally narrowed in on the condition
that was causing this blocking behavior. The issue arises when a client connect()s twice
to the same server with non-blocking unix-domain sockets before calling select().

There are a few pieces to this. With the client configured to connect() just once, I can
see that the server's select() returns as soon connect() is called, but then accept()
blocks until the client calls select(). That is not proper non-blocking behavior, but it
appears that the implementation under Cygwin does require that client and server
both be communicating synchronously to accomplish the connect() operation.

I tried running this under Ubuntu 16.04 and found that connect() succeeded immediately, so
no subsequent select() is required, and there does not appear to be a possibility for this
collision. Perhaps that is because Linux ignores the non-blocking flag for the connect.

I will attempt to force the question by delaying calling select() on the server.

The behavior under Linux (Ubuntu 16.04) is that connect() returns with no error
immediately, regardless of whether the server is in select() or accept() or not.

A workaround for this issue may be to keep the socket blocking until after connect().

