---
id: enums
title: Enums
group: Language Reference
icon: fa-list-ol
order: 8
description: Named integer constants with explicit or automatic values.
---

# Enums

An `enum` gives names to related integer values. Enum members are `Int64` constants; the enum itself is a namespace.

```vyne
enum Color {
    RED = 0,
    GREEN,
    BLUE,
}

out(Color.RED);     # 0
out(Color.GREEN);   # 1
out(Color.BLUE);    # 2
```

An explicit value sets the counter; the following members auto-increment from it.

```vyne
enum Status {
    OK = 200,
    CREATED,
    ACCEPTED,
    BAD_REQUEST = 400,
    UNAUTHORIZED,
}
```

A trailing comma is allowed. Members can be used anywhere an `Int64` is expected:

```vyne
code :: Int64 := Status.CREATED;

if code == Status.OK {
    out("success");
}
```

Enums are a naming convenience, not a distinct runtime type. Store them in `Int64` fields and compare them as integers. For a closed set of values that also carries data, use an interface plus a discriminant field instead.

## See also

- [Values & Types](page:types)
- [Interfaces](page:iface)
- [Expressions](page:expr)
