sort.h
======

<a href="https://travis-ci.org/swenson/sort"><img alt="build status" src="https://api.travis-ci.org/swenson/sort.png" /></a>

Overview
--------

`sort.h` is an implementation a ton of sorting algorithms in C with a
user-defined type that is defined at include time.

This means you don't have to pay the function call overhead of using
a standard library routine. This also gives us the power of higher-level
language generics.

In addition, you don't have to link in a library:
the entirety of this sorting library is contained in the `sort.h` header file.

You get the choice of many sorting routines, including:

* Shellsort
* Binary insertion sort
* Heapsort
* Quicksort
* Merge sort (stable)
* In-place merge sort (*not* stable)
* Selection sort (ugh -- this is really only here for comparison)
* Timsort (stable)
* Grail sort (stable)
  * Based on [`B-C. Huang and M. A. Langston, *Fast Stable Merging and Sorting in
  Constant Extra Space* (1989-1992)`](http://comjnl.oxfordjournals.org/content/35/6/643.full.pdf).

    Thanks to Andrey Astrelin for the implementation.
* Sqrt Sort (stable, based on Grail sort, also by Andrey Astrelin).

If you don't know which one to use, you should probably use Timsort.

If you have a lot data that is semi-structured, then you should definitely use Timsort.

If you have data that is really and truly random, quicksort is probably fastest.


Usage
-----

To use this library, you need to do three things:

* `#define SORT_TYPE` to be the type of the elements of the array you
  want to sort. (For pointers, you should declare this like: `#define SORT_TYPE int*`)
* `#define SORT_NAME` to be a unique name that will be prepended to all
  the routines, i.e., `#define SORT_NAME mine` would give you routines
  named `mine_heap_sort`, and so forth.
* `#include "sort.h"`.  Make sure that `sort.h` is in your include path.

Then, enjoy using the sorting routines.

Quick example:

```c
#define SORT_NAME int64
#define SORT_TYPE int64_t
#define SORT_CMP(x, y) ((x) - (y))
#include "sort.h"
```

You would now have access to `int64_quick_sort`, `int64_tim_sort`, etc.,
which you can use like

```c
/* Assumes you have some int64_t *arr or int64_t arr[128]; */
int64_quick_sort(arr, 128);
```

See `demo.c` for a more detailed example usage.

If you are going to use your own custom type, you must redefine
`SORT_CMP(x, y)` with your comparison function, so that it returns
a value less than zero if `x < y`, equal to zero if `x == y`, and
greater than 0 if `x > y`.

The default just uses the builtin `<`, `==`, and `>` operators:

```c
#define SORT_CMP(x, y)  ((x) < (y) ? -1 : ((x) == (y) ? 0 : 1))
```

It is often just fine to just subtract the arguments as well (though
this can cause some stability problems with floating-point types):

```c
#define SORT_CMP(x, y) ((x) - (y))
```

You can also redefine `TIM_SORT_STACK_SIZE` (default 128) to control
the size of the tim sort stack (which can be used to reduce memory).
Reducing it too far can cause tim sort to overflow the stack though.

Speed of routines
-----------------

The speed of each routine is highly dependent on your computer and the
structure of your data.

If your data has a lot of partially sorted sequences, then Tim sort
will beat the kilt off of anything else.

Timsort is not as good if memory movement is many orders of magnitude more
expensive than comparisons (like, many more than for normal int and double).
If so, then quick sort is probably your routine.  On the other hand, Timsort
does extremely well if the comparison operator is very expensive,
since it strives hard to minimize comparisons.

Here is the output of `demo.c`, which will give you the timings for a run of
10,000 `int64_t`s on 2014-era MacBook Pro:

```
Running tests
stdlib qsort time:             1285.00 us per iteration
stdlib heapsort time:          2109.00 us per iteration
stdlib mergesort time:         1299.00 us per iteration
quick sort time:                579.00 us per iteration
selection sort time:         127176.00 us per iteration
merge sort time:                999.00 us per iteration
binary insertion sort time:   13443.00 us per iteration
heap sort time:                 592.00 us per iteration
shell sort time:               1054.00 us per iteration
tim sort time:                 1005.00 us per iteration
in-place merge sort time:       903.00 us per iteration
grail sort time:               1220.00 us per iteration
sqrt sort time:                1095.00 us per iteration
```

Quicksort is the winner here. Heapsort, in-place merge sort,
and timsort also often tend to be quite fast.

Contributing
------------

See [CONTRIBUTING.md](CONTRIBUTING.md).

References
----------

* [Wikipedia: Timsort](https://en.wikipedia.org/wiki/Timsort)
* [`timsort.md`](doc/timsort.txt)

License
-------

Available under the MIT License. See [LICENSE.md](LICENSE.md) for details.
