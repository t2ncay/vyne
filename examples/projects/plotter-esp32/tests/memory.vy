use native vplot;
use native vmem;
ruleset { dynamic_casting };
p :: Int64 := vplot.new_plot("Allocation regression", "Time", "Signal");
s :: Int64 := vplot.live_series(p,256,"Signal",vplot.rgb(56,189,248));
surface :: Int64 := vplot.image_surface(200,160);
vplot.render(p,surface);
arena_before :: Int64 := vmem.total_allocated();
allocations_before :: Int64 := vplot.allocation_count();
bytes_before :: Int64 := vplot.memory_bytes();
through frame :: 0..1000 -> loop {
    vplot.append(s,float64(frame),float64(frame % 10));
    vplot.render(p,surface);
};
if (vmem.total_allocated()!=arena_before) { throw "append/render allocated in the Vyne arena"; }
if (vplot.allocation_count()!=allocations_before || vplot.memory_bytes()!=bytes_before) { throw "append/render allocated native storage"; }
vplot.destroy(p);
vplot.destroy(surface);
if (vplot.memory_bytes()!=0) { throw "native objects leaked"; }
out("VPLOT_MEMORY_PASS");
