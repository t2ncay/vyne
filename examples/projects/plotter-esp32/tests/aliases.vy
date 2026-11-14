use native vplot as chart;
ruleset { dynamic_casting };
p :: Int64 := chart.new_plot("Alias", "x", "y");
s :: Int64 := chart.live_series(p,4,"Series",chart.rgb(10,200,100));
chart.append(s,0.0,1.0);
chart.append(s,1.0,0.0);
surface :: Int64 := chart.image_surface(200,160);
if (!chart.render(p,surface)) { throw "alias renderer failed"; }
chart.destroy(p);
chart.destroy(surface);
if (chart.memory_bytes()!=0) { throw "alias ownership leaked"; }
out("VPLOT_ALIASES_PASS");
