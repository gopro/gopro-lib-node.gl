# Tests

While we have a few dedicated tests in the components themselves, most of the
tests are located in the `tests` root directory (and referenced in
`tests/meson.build`). Since the Meson build system includes a test system,
that's what we rely on.


## Basic Usage

Assuming node.gl has been configured through the standard procedure (using
`configure.py`), executing `make tests` is enough to trigger all the tests
(including the component specific tests). For more information on the bootstrap
procedure, see the [installation documentation](/doc/howto/installation.md).

In case of failure, a log will be written and its path indicated on the
standard output by meson.


## Debugging

To run specific tests, you will need to activate the environment (usually
through `. venv/bin/activate`) and execute `meson test -C builddir/tests
YOUR_TEST_NAME`. You can also limit the tests to a specific test suite using
the `--suite` option. For example: `meson test -v -C builddir/tests translate
--suite opengles`.

For a complete list of tests and their respective suite, `meson test --list -C
builddir/tests` can be used (from within the `venv` and after running `make
tests-setup` or `make tests` at least once).

Many tests are based on scenes constructed in Python, sometimes available with
specific debugging options. So in order to get a visual feedback, you can
usually test them in `ngl-control`/`ngl-desktop` (or `ngl-python`), for
example: `ngl-control -m ./tests/blending.py`. For more information on the
controller usage, check out the [starter tutorial](/doc/tuto/start.md).

For cue-points or fingerprint based tests, the `TESTS_OPTIONS=dump` environment
toggle can be used to dump pictures (in `<tempdir>/nodegl/tests`).

Additional tooling is available, such as GPU capture and code coverage (see
associated sections below).


## Updating references

Sometimes the tests need to be updated and therefore their associated
references too. The `REFGEN` environment variable makes this operation less
painful. Following are the possible values and their effects:

- `REFGEN=no`: Run the test normally without changing the reference (default)
- `REFGEN=create`: Create the reference file if not present
- `REFGEN=update`: Same as "create" and update the reference if the test fails
- `REFGEN=force`: Same as "create" and always replace the reference file. This
  may change the references even if the tests are passing due to the
  threshold/tolerance mechanism.

Under the hood, these controls affect how `ngl-test` will behave.


## GPU Capture

GPU capture is currently available through [RenderDoc](https://renderdoc.org).
To enable it, you need to build with the `gpu_capture` debug option
(`./configure.py --debug-opts gpu_capture`). The `configure.py` script will
automatically download RenderDoc, it is not required to have it installed on
your system.

On Linux, a typical workflow will look like this:

```sh
# Serialize a scene
ngl-serialize pynodegl_utils.examples.misc fibo /tmp/fibo.ngl

# Execute a command with librenderdoc hook and capture enabled
# A renderdoc capture path will be indicated on stdout
LD_PRELOAD=external/renderdoc/lib/librenderdoc.so NGL_GPU_CAPTURE=yes ngl-render -t 0:30:60 -i /tmp/fibo.ngl

# Study the capture with QRenderDoc
external/renderdoc/bin/qrenderdoc /tmp/RenderDoc/ngl-render_*_capture.rdc
```


## Code coverage

Code coverage can be enabled using `./configure.py --coverage`. To study the
coverage of the tests, we can execute the tests with `make tests` and the run
`make coverage-html` to generate an HTML report. The `coverage-xml` rule is
also available for a report meant to be parsed by other tools.


## Memory leaks

For the native tests, meson test wrapper can be used in combination with
[Valgrind](https://www.valgrind.org/). After activating the `venv`, they can be
executed with Valgrind using: `meson test -C builddir/libnodegl --wrap
'valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1'`

Running the Python tests with that method is not well supported currently. It
can be experimented with `PYTHONMALLOC=malloc` to silence Python errors related
to their internal allocator, but then drivers issues would need to be silenced
manually as well. This is still a work in progress.

Beware that Valgrind only covers heap allocation, nothing GPU related.


## Memory failure simulation

Sometimes we want to test that error codepaths are properly handled and do not
lead to memory leaks. To achieve that, node.gl includes a memory failure
simulation that can be enabled at configure time with `--buildtype debug
--debug-opt mem`.

When this option is enabled, we can create artificial memory failures using the
`NGL_MEM_ALLOC_FAIL` environment variable. A typical workflow will be to count
how many allocations happen for a given execution (using
`NGL_MEM_ALLOC_FAIL=count`), and make them fail one by one.

For example:
```sh
# Serialize a scene
ngl-serialize pynodegl_utils.examples.misc fibo /tmp/fibo.ngl

# Count the number of mallocs (the last lines will contain something like
# MEMCOUNT: 858)
NGL_MEM_ALLOC_FAIL=count ngl-render -t 0:30:60 /tmp/fibo.ngl

# Make them fail one by one
for i in $(seq 0 858); do
    NGL_MEM_ALLOC_FAIL=$i valgrind --error-exitcode=50 --leak-check=full ngl-render -t 0:30:60 -i /tmp/fibo.ngl
    [ $? -eq 50 ] && break
done
```

If `valgrind` detects an error (not `ngl-render` but `valgrind` itself), the
loop will stop.
