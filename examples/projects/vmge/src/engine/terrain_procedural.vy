use "terrain.vy";
use "world_numbers.vy";

# Header 9 generator version (0 legacy,1 smooth value-noise fBm),10 frequency,
# 11 octaves,12 persistence,13 spawn-pad radius,14/15 pad world X/Z.
fn se_generator_validate(frequency :: Float64,octaves :: Int64,persistence :: Float64,pad :: Float64) -> Bool {
    if !(frequency >= 0.001 && frequency <= 2.0 && persistence >= 0.0 && persistence <= 1.0 && pad >= 0.0 && pad <= 1024.0) || octaves < 1 || octaves > 8 { throw "Generator requires frequency .001..2, octaves 1..8, persistence 0..1, pad 0..1024"; } return true;
}
fn se_generator_lattice(x :: Int64,z :: Int64,seed :: Int64) -> Float64 {
    # Coordinates are nonnegative bounded local grid coordinates.
    n :: Int64 := (x*374761+z*668265+seed*69069+17)%1000003;
    n = (n*n+137631)%1000003; return float64(n)/1000003.0;
}
fn se_generator_noise(x :: Float64,z :: Float64,seed :: Int64) -> Float64 {
    ix :: Int64 := int64(vmath.floor(x)); iz :: Int64 := int64(vmath.floor(z));
    u :: Float64 := x-float64(ix); v :: Float64 := z-float64(iz); u = u*u*(3.0-2.0*u); v = v*v*(3.0-2.0*v);
    a :: Float64 := se_generator_lattice(ix,iz,seed); b :: Float64 := se_generator_lattice(ix+1,iz,seed);
    c :: Float64 := se_generator_lattice(ix,iz+1,seed); d :: Float64 := se_generator_lattice(ix+1,iz+1,seed);
    return (a+(b-a)*u)*(1.0-v)+(c+(d-c)*u)*v;
}
fn se_generator_fbm(x :: Float64,z :: Float64,seed :: Int64,frequency :: Float64,octaves :: Int64,persistence :: Float64) -> Float64 {
    sum :: Float64 := 0.0; total :: Float64 := 0.0; weight :: Float64 := 1.0; f :: Float64 := frequency; octave :: Int64 := 0;
    while octave < octaves { sum = sum+se_generator_noise(x*f,z*f,(seed+octave*7919)%1000001)*weight; total = total+weight; weight = weight*persistence; f = f*2.0; octave = octave+1; } return sum/total;
}
fn se_terrain_generate_procedural(world :: SEWorld,side :: Int64,spacing :: Float64,amplitude :: Float64,seed :: Int64,preset :: Int64,frequency :: Float64,octaves :: Int64,persistence :: Float64,pad_radius :: Float64) -> Bool {
    # All validation precedes mutation; callers publish a preview owner afterwards.
    se_terrain_validate(side,spacing,amplitude,seed,preset); se_generator_validate(frequency,octaves,persistence,pad_radius);
    if pad_radius > 0.0 && pad_radius < spacing*1.5 { throw "Spawn pad radius must cover at least 1.5 terrain spacings, or be zero"; }
    world.data[1] = float64(side); world.data[2] = spacing; world.data[6] = float64(seed); world.data[7] = float64(preset); world.data[8] = amplitude;
    world.data[9] = 1.0; world.data[10] = frequency; world.data[11] = float64(octaves); world.data[12] = persistence;
    world.data[13] = pad_radius; world.data[14] = world.data[3]+float64(side)*spacing*0.5; world.data[15] = world.data[5]+float64(side)*spacing*0.5;
    center :: Float64 := se_generator_fbm(float64(side)*spacing*0.5,float64(side)*spacing*0.5,seed,frequency,octaves,persistence)*amplitude;
    if preset == 2 { center = amplitude; } if preset == 3 { center = 0.0; }
    x :: Int64 := 0; while x <= side { z :: Int64 := 0; while z <= side {
        px :: Float64 := float64(x)*spacing; pz :: Float64 := float64(z)*spacing;
        nx :: Float64 := float64(x)*2.0/float64(side)-1.0; nz :: Float64 := float64(z)*2.0/float64(side)-1.0; radius :: Float64 := vmath.sqrt(nx*nx+nz*nz);
        h :: Float64 := se_generator_fbm(px,pz,seed,frequency,octaves,persistence)*amplitude;
        if preset == 1 { h = h*vmath.max(0.0,1.0-radius); } if preset == 2 { h = vmath.clamp((0.8-radius)*5.0,0.0,1.0)*amplitude; } if preset == 3 { h = 0.0; }
        if pad_radius > 0.0 {
            dx :: Float64 := px-float64(side)*spacing*0.5; dz :: Float64 := pz-float64(side)*spacing*0.5;
            distance :: Float64 := vmath.sqrt(dx*dx+dz*dz); blend :: Float64 := vmath.clamp((distance-pad_radius)/vmath.max(spacing,pad_radius*0.5),0.0,1.0); blend = blend*blend*(3.0-2.0*blend); h = center+(h-center)*blend;
        }
        world.data[20496+z*65+x] = h; z = z+1;
    } x = x+1; } return true;
}
# Optional scene extension, emitted only by a caller opting into G records.
# Existing W/node/H/C records are unchanged; applied heights remain authoritative.
fn se_generator_record(world :: SEWorld) -> String {
    if world.data[9] == 0.0 { return ""; }
    builder :: Int64 := vcore.sb_create(); vcore.sb_append(builder,"G");
    i :: Int64 := 9; while i < 16 { vcore.sb_append(builder,"|"+se_world_decimal(world,world.data[i])); i = i+1; } return vcore.sb_build(builder);
}
fn se_generator_parse_record(world :: SEWorld,line :: String) -> Bool {
    fields :: Array := vmge_split(line,"|"); if fields.size() != 8 || fields[0] != "G" || vmge_number(fields[1]) != 1.0 { throw "Unsupported generator record/version"; }
    frequency :: Float64 := vmge_number(fields[2]); raw_octaves :: Float64 := vmge_number(fields[3]); persistence :: Float64 := vmge_number(fields[4]); pad :: Float64 := vmge_number(fields[5]); x :: Float64 := se_world_coordinate(fields[6]); z :: Float64 := se_world_coordinate(fields[7]);
    if raw_octaves != vmath.floor(raw_octaves) { throw "Octaves must be an integer"; } se_generator_validate(frequency,int64(raw_octaves),persistence,pad);
    if pad > 0.0 && (pad < world.data[2]*1.5 || !se_terrain_contains(world,x,z)) { throw "Generator spawn pad is outside its terrain or too small"; }
    world.data[9] = 1.0; world.data[10] = frequency; world.data[11] = raw_octaves; world.data[12] = persistence; world.data[13] = pad; world.data[14] = x; world.data[15] = z; return true;
}
