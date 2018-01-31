# Contributing to Tlog
The Tlog project welcomes new contributors. This document will guide you
through the process.

## Pull Requests
We actively welcome your pull requests. There are a few guidelines that
we need contributors to follow so that we can keep the codebase consistent
and clean.

* Fork the main repository and clone it to your local machine.
* Create a topic branch from master. Please avoid working directly on
the ```master``` branch.
* Make commits of logical units, and make sure to follow the
coding style guidelines.
* Submit the pull request (PR) on Github.

## Issues
We use GitHub [issues tracker](https://github.com/Scribery/tlog/issues)
to track bugs.
Please ensure your description is clear and has sufficient instructions;
so we are able to reproduce the issue.

## Coding Style guidelines

* Use four spaces for indentation, please don't use tabs except where
required, such as in Makefiles.

* Lines should be at most 78 chars.

* Please don't leave trailing whitespace at the ends of lines.

* Braces open on the same line as the `for`/`while`/`if`/`else`
/`switch`/etc. Closing braces are put on a line of their own, except
in the `else` of an `if` statement or before a `while` of a `do`-`while`
statement. Always use braces for `if` and `while`.

```c
if (a == b) {
    ...
}

if (a == b) {
    ...
} else if (a > b) {
    ...
}

if (a == b) {
    do_something();
} else {
    do_something_else();
}

do {
    do_something();
} while (cond);

while (cond) {
    do_something();   
}

switch (expression) {
case constant-expression:
    ...
case constant-expression:
    ...
default:
    ...
}

```

* For function, `static` and return type are put on the same line.
Function's name is put on a line of its own.

```c
static tlog_grc
tlog_play_run(struct tlog_errs **perrs, int *psignal)
{
              ...
}
```

* For regular comments, use C style (`/*` `*/`) comments, even for one-line.

* For doc-comments (Doxygen comments), use the `/**` `*/` style featured.

* For wrapping (`\`) of macro, be sure to follow:

```c
#define CMP_BOOL(_name) \
    do {                                                        \
        if (_name != t._name##_out) {                           \
            FAIL(#_name " %s != %s",                            \
                 BOOL_STR(_name), BOOL_STR(t._name##_out));     \
        }                                                       \
    } while (0)
```

## Directories
Tlog source is organized as follows.

```
.                        <- Top-level build and documentation files.
├── doc                  <- Documentation.
├── include              <- C headers.
│   ├── tlog             <- Tlog library headers.
│   ├── tltest           <- Tlog test library headers.
├── lib                  <- C library sources.
│   ├── tlog             <- Tlog library sources.
│   ├── tltest           <- Tlog test library sources.
├── src                  <- Program sources.
│   ├── tlog             <- Tlog sources.
│   ├── tltest           <- Tlog test sources.
├── m4                   <- M4 macros.
│   ├── autotools        <- Autotools macros.
│   ├── tlog             <- Tlog macros
└── man                  <- Manpages.
```

Follow instructions in [README](README.md) for how to compile and run
test code.

## License
By contributing to Tlog, you agree that your contributions will be licensed
under the [GNU GPL v2 or later](COPYING).
