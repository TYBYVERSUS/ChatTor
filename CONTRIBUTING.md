1) I will remain the ultimate artiber of what makes it into this repo. I will probably touch your code, and I may not be gentle
2) Remember that this is a multi-threaded environment... Make sure your code is thread safe!
3) Loosely comment what you're doing (I will add more comments next time I work on this), and be a little more specific if you're doing something weird
4) Use NULL bytes ('\0', char 0) to delimit strings, not just terminate them (Strings with a known length don't need a NULL terminator, but adding one isn't a big deal to me)
5) Don't create tons of unecessary variables or do things like an idiot. If you do, I will molest your code, and I will not be gentle
6) Don't create functions because you want to break up long sections of code; only create functions to avoid duplicating code, to do recursion, etc
7) It is ok to break this code up into more files since I will no longer be the only one working on this, but please keep things sane
8) Keep the code performant, and don't use more resources than you need (don't use a long to count to 5, do use unsigned ints where appropriate, etc)
9) Try to keep things agnostic of OS/compiler. I've been trying to do this myself, but may have used something non-standard without realizing it. NOTE: SIGIO is not POSIX (yet?), but SIGPOLL is marked as obsolescent and doesn't seem to work? I don't know. I'm using SIGIO here
10) I didn't make a gitignore to ignore the contents of bin/ or meme files that don't matter. Feel free to add this
