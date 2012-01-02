The Voidcaster
==============

What?
-----

The Voidcaster is a static analysis tool which tells you when to cast a function
call to `void` because you are ignoring the return value, or not to cast a
function which doesn't return a value to `void`.

Currently, it outputs a warning for every missing/superfluous cast encountered.
Alternatively, it can run in *interactive mode*, where it's sufficient to answer
a set of *you're not casting this to `void`; should I cast it for you?* prompts
and the source files are modified accordingly.

Report bugs on github! http://github.com/RavuAlHemio/voidcaster

How?
----

One word: [libclang](http://clang.llvm.org/doxygen/group__CINDEX.html). Which is
a bit unfortunate, because LLVM is quite a big dependency.

Why?
----

If your organization prescribes casting ignored return values to `void`, you can
benefit from this tool in three ways:

1. If you disagree with the rule, you can spend more time programming and
debugging and less time hunting down cases of not-explicitly ignored return
values.

2. If you agree with the rule, you can quickly obtain a concise list of cases
where you forgot, and think again whether you care about the return value or
not.

3. If you enforce the rule, you can obtain an at-a-glance list of where the rule
was violated.

Who?
----

I am a student and junior teaching assistant at the Vienna University of
Technology (also known as *Technische Universität Wien* and to locals as *die
TU*).
