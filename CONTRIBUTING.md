1) I will remain the ultimate artiber of what makes it into this repo. I will probably touch your code, and I may not be gentle
2) Remember that this is a multi-threaded environment... Make sure your code is thread safe!
3) Loosely comment what you're doing, and be a little more specific if you're doing something weird
4) Use NULL bytes ('\0', char 0) to delimit strings, not just terminate them. Note that the NULL terminator for a message being sent to a client should not be sent
5) Don't create tons of unecessary variables or do things like an idiot. If you do, I will molest your code, and I will not be gentle
6) Don't create functions because you want to break up long sections of code; only create functions to avoid duplicating code, to do recursion, etc
7) It is ok to break this code up into more files since I will no longer be the only one working on this, but please keep things sane
8) Keep the code performant, and don't use more resources than you need (don't use a long to count to 5, do use unsigned ints where appropriate, etc)
9) Try to keep things agnostic of OS/compiler. I've been trying to do this myself, but may have used something non-standard without realizing it. NOTE: SIGIO is not POSIX (yet?), but SIGPOLL is marked as obsolescent and doesn't seem to work? I don't know. I'm using SIGIO here
10) I didn't make a gitignore to ignore the contents of bin/ or meme files that don't matter. Feel free to add this
11) Since there are many, many variables, I think it would be nice to CamelCase global variables and functions, also Capitalize global variables (but not functions), and reserve the lower_case syntax for local variables. I'll format everything to this eventually (unless someone wants to beat me to it). If this is stupid, tell me

Resources:

RFC - https://tools.ietf.org/html/rfc6455 (The RFC is fairly short compared to other RFCs)

Quick Summary - https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_servers
