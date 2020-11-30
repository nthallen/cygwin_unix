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
