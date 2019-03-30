# Contributing to sort.h

`sort.h` is based on the hard work and fun times of several people, and I hope
you'll join us!

## Make your change

Step 1 is always check out the code, play with it, and make whatever
change you were planning to make.
Maybe adding a new algorithm, improving the performance of something,
or making something simpler?

Adding more documentation would also be wonderful!

## We use C89

To be compatible with as many C compilers as possible, we stick to the rather
old C89 standard.
In particular, some Microsoft compilers are very picky about this, and
we want that `sort.h` runs on all platforms.

## No dependencies

`sort.h` is pure C and macros, and should compile without any other libraries.
We don't want to create a dependency problem.

We want to keep it that way, because dependencies are an awful problem in C.

## No libraries

`sort.h` is almost entirely self-contained (`sort_common.h` is only
ever included once, and has some common functions for `sort.h`).
It doesn't build libraries

We want to keep it that way, because building libraries, versioning them, and
linking to them correctly are awful problems in C.

## static

All functions should be `static` in `sort.h` and `sort_common.h`.

Combined with the fact that we don't build any libraries, this allows
the compiler to make all sorts of optimizations it can never make otherwise,
such as inlining functions, using fast arithmetic, and avoiding pointers.

## __inline

The proper, C89 way to do inlineing is to mark a function as `__inline` --
this is useful for comparisons and other small functions that are used often.

## Make sure tests pass

Run `make`, and it should build `stresstest` and run it, which
will make sure nothing is broken.

## Add more tests, and make sure those pass too

If you are writing new code, make sure that it is tested in
`stresstest.c`.

## Run `make format`

Run `make format` to ensure that the source files all conform to some
standard style guidelines for C.

(On OS X, you can use `brew install astyle` to install `astyle`.)

## Push your branch to GitHub

On your fork, push your branch up with your changes, and create a pull request.

## We review!

Swenson should review the code shortly, and provide you feedback, or merge
if it looks great.

## Questions

If you have questions, problems, or just aren't even sure where to start,
please reach out!
Open an issue, or reach out on Twitter (https://twitter.com/chris_swenson)
or email (chris@caswenson.com), and we'd be happy to help.

<3
