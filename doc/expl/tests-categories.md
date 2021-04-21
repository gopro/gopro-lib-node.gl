# Tests categories

## Simple tests

In the tests suite, you will find a few straightforward tests written as simple
Python functions, calling `assert` directly. These tests usually cover
top-level API usage or anything non-visual. Some examples can be found in
`tests/api.py`.

`libnodegl` also includes dedicated programs for unit-tests (see
`libnodegl/test_*`) to cover code which is usually harder to access from a
top-level interface like the one provided by the Python binding.


## Cue-points

Cue-points based tests are one of the most basic visual tests: these tests are
specified with one or more pixel coordinates to extract the color from, and
compare it to a reference.

These tests are using the `@test_cuepoints()` decorator.

A few options are available such as the export resolution and threshold
tolerance.


## Fingerprint

Fingerprint based tests are a more advanced visual tests. For these tests, the
visual output is scaled down to a timber scaled resolution (`9x9` pixels) and a
hash (or fingerprint) of each color component (R, G, B and A) is derived from
it. The hash reflects the variation in intensity.

This means the fingerprint is a very good candidate for testing visual shapes
and patterns but not so much if you want test the intensity of a color
typically.

**Note**: The algorithm is pretty much the same as [dhash][dhash] with a
different interleaving and split components (instead of just gray level)

Hashes will typically look like this: `4F442D7138513A1C8B548E028A822180
45542C86288228C26D1029C428202820 00000514288045546D9629D228822880
00000000000000000000000000000000`. The space separates RGBA components, so we
can already see that the alpha component has no variation (likely full of `0`,
but it could as well be full of `0x2a` or any other value).

In case of hash mismatch, you will get a rough estimation of what part of the
image actually changed:

```
transform_animated_camera frame #0 Component R: Diff too high (18% > 1%)
 . . . . . . . .
 . . . . . . . .
 . . . v . . . .
 . v . + > . . .
 > . v . . > . .
 v v v . v > . .
 > v . v . v + v
 v . . . . . . .
```

`v`, `>` and `+` respectively refer to a vertical (top-to-bottom), horizontal
(left-to-right) and vertical+horizontal difference while the `.` represents an
identical area.

These tests are using the `@test_fingerprint` decorator.

The reason we rely on this fingerprint (and cue-points) mechanism is to address two issues:

- Tests need to be resilient to slight visual changes (not all platforms and
  drivers produce the same output)
- Storage of image and video references can get big really quickly, which would
  require a separate storage and synchronization mechanism

[dhash]: https://pypi.org/project/dhash/


## Floats

Some tests will compare series of float values. This is typically the case for
the animations/easings.

These tests are using the `@test_floats` decorator.
