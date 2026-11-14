# Existing text helper remains usable alongside the new universal module.
use external "vplot.vy";
ruleset { dynamic_casting };
values :: Array := [1,2];
vplot.bar_chart(values,2);
out("VPLOT_LEGACY_PASS");
