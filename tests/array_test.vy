ruleset {
    warnings = all,
    warnings_ignore = [unused_variable, implicit_type],
    type_check = hybrid,
    implicit_casting = warn,
    profiling = on,
    debug = on,
    trace = on
};

x = [];
a = 5;
b = 7;

zirt = 1..5;

x.push(a,b);
out(x.size());
out(zirt);