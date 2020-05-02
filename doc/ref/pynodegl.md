pynodegl
========

[pynodegl][pynodegl] is the Python binding for [libnodegl][libnodegl]. It is
automatically generated from to the `libnodegl` node specifications file.  For
detailed information on how the `pynodegl` binding is created, refer to the
corresponding [pynodegl explanation][expl-pynodegl].

In order to use the `pynodegl`, you will have to rely on the [reference node
specifications][ref-libnodegl]. The Python binding classes and parameters are
directly named after the names found in the specifications. For example, to get
an instance of a [Group][ref-libnodegl-group], one has to instantiate a `Group`
such as `g = Group(children=(r1, r2, r3))`.

## Parameters in constructors

In the constructors, the expected Python parameter types follow this table:

Parameter type          | Python type                         | Example
----------------------- | ----------------------------------- | ------------
`*Dict`                 | `dict`                              | `render = Render(geometry, frag_resources={'tex0': t0, 'tex1': t1})`
`vec*`, `mat*`, `*List` | Iterable such as `tuple` or `list`  | `scale = Scale(child, factors=(0.3, 1.2, 1.0))`
All the others          | `-`                                 | `circle = Circle(radius=0.5, npoints=128)`

**Note**: Every parameter marked as a *Ctor.* (*Constructor*) will need to be
passed as arguments to the constructor.

## Parameter type to method correspondence

Every non constructor parameter has a setter method associated, named according
to the parameter name and its type.

Parameter type | Node method                      | Python <param> type        | Example
-------------- | -------------------------------- | -------------------------- | --------
`*Dict`        | `update_<param>(self, <param>)`  | `dict` or named arguments  | `render.update_frag_resources(tex0=t0, tex1=t1)`
`*List`        | `add_<param>(self, *<param>)`    | positional arguments       | `group.add_children(r1, r2, r3)`
All the others | `set_<param>(self, <param>)`     | positional arguments       | `camera.set_center(1.0, -1.0, 0.5)`

[pynodegl]: /pynodegl
[libnodegl]: /libnodegl
[expl-pynodegl]: /doc/expl/pynodegl.md
[ref-libnodegl]: /libnodegl/doc/libnodegl.md
[ref-libnodegl-group]: /libnodegl/doc/libnodegl.md#group
