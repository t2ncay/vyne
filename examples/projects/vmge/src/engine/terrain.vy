use "world.vy";

fn se_terrain_validate(side :: Int64,spacing :: Float64,amplitude :: Float64,seed :: Int64,preset :: Int64) -> Bool {
    if side < 1 || side > 64 || !(spacing >= 0.125 && spacing <= 1024.0) || !(amplitude >= 0.0 && amplitude <= 1024.0) || seed < 0 || seed > 1000000 || preset < 0 || preset > 3 { throw "Invalid bounded terrain settings"; } return true;
}
# Integer hash: deterministic across CPU implementations; bounded arithmetic.
fn se_terrain_noise(x :: Int64,z :: Int64,seed :: Int64) -> Float64 {
    n :: Int64 := (x*374761+z*668265+seed*69069+17)%1000003;
    n = (n*n+137631)%1000003; return float64(n)/1000003.0;
}
# presets 0 hills,1 island,2 plateau,3 flat. Generate only into a preview owner.
fn se_terrain_generate(world :: SEWorld,side :: Int64,spacing :: Float64,amplitude :: Float64,seed :: Int64,preset :: Int64) -> Bool {
    se_terrain_validate(side,spacing,amplitude,seed,preset);
    world.data[1] = float64(side); world.data[2] = spacing; world.data[6] = float64(seed); world.data[7] = float64(preset); world.data[8] = amplitude;
    x :: Int64 := 0; while x <= side { z :: Int64 := 0; while z <= side {
        nx :: Float64 := float64(x)*2.0/float64(side)-1.0; nz :: Float64 := float64(z)*2.0/float64(side)-1.0;
        radius :: Float64 := vmath.sqrt(nx*nx+nz*nz);
        wave :: Float64 := (vmath.sin(float64(x)*0.23+float64(seed%97))+vmath.cos(float64(z)*0.19+float64(seed%71))+2.0)*0.2+se_terrain_noise(x/4,z/4,seed)*0.2;
        h :: Float64 := wave*amplitude;
        if preset == 1 { h = vmath.max(0.0,1.0-radius)*h; }
        if preset == 2 { h = vmath.clamp((0.8-radius)*5.0,0.0,1.0)*amplitude; }
        if preset == 3 { h = 0.0; }
        world.data[20496+z*65+x] = h; z = z+1;
    } x = x+1; } return true;
}
fn se_terrain_contains(world :: SEWorld,x :: Float64,z :: Float64) -> Bool {
    side :: Int64 := int64(world.data[1]); extent :: Float64 := float64(side)*world.data[2];
    return side > 0 && x >= world.data[3] && z >= world.data[5] && x <= world.data[3]+extent && z <= world.data[5]+extent;
}
# Exact piecewise planar interpolation for rendered split a-c-b / b-c-d.
fn se_terrain_height(world :: SEWorld,x :: Float64,z :: Float64) -> Float64 {
    if !se_terrain_contains(world,x,z) { return -1000000.0; }
    gx :: Float64 := (x-world.data[3])/world.data[2]; gz :: Float64 := (z-world.data[5])/world.data[2];
    side :: Int64 := int64(world.data[1]);
    ix :: Int64 := int64(vmath.min(vmath.floor(gx),float64(side-1))); iz :: Int64 := int64(vmath.min(vmath.floor(gz),float64(side-1)));
    u :: Float64 := gx-float64(ix); v :: Float64 := gz-float64(iz); p :: Int64 := 20496+iz*65+ix;
    a :: Float64 := world.data[p]; b :: Float64 := world.data[p+1]; c :: Float64 := world.data[p+65]; d :: Float64 := world.data[p+66];
    if u+v <= 1.0 { return world.data[4]+a+(b-a)*u+(c-a)*v; }
    return world.data[4]+d+(c-d)*(1.0-u)+(b-d)*(1.0-v);
}
# scratch 0..4224 snapshots heights before each dab: smoothing is order independent.
# mode 0 raise,1 lower,2 flatten,3 smooth; radial linear falloff.
fn se_terrain_sculpt(world :: SEWorld,x :: Float64,z :: Float64,radius :: Float64,strength :: Float64,target :: Float64,mode :: Int64) -> Bool {
    if !(radius >= 0.125 && radius <= 1024.0 && strength >= 0.0 && strength <= 1024.0 && vmath.abs(target) <= 1024.0) || mode < 0 || mode > 3 { throw "Invalid sculpt brush"; }
    side :: Int64 := int64(world.data[1]); if side == 0 { return false; }
    i :: Int64 := 0; while i < 4225 { world.work[i] = world.data[20496+i]; i = i+1; }
    changed :: Bool := false; ix :: Int64 := 0;
    while ix <= side { iz :: Int64 := 0; while iz <= side {
        dx :: Float64 := world.data[3]+float64(ix)*world.data[2]-x; dz :: Float64 := world.data[5]+float64(iz)*world.data[2]-z;
        weight :: Float64 := vmath.max(0.0,1.0-vmath.sqrt(dx*dx+dz*dz)/radius);
        p :: Int64 := iz*65+ix; old :: Float64 := world.work[p]; next :: Float64 := old;
        if weight > 0.0 {
            if mode == 0 { next = old+strength*weight; } if mode == 1 { next = old-strength*weight; }
            if mode == 2 { next = old+(target-world.data[4]-old)*vmath.min(1.0,strength*weight); }
            if mode == 3 {
                sum :: Float64 := old; count :: Float64 := 1.0;
                if ix > 0 { sum = sum+world.work[p-1]; count = count+1.0; } if ix < side { sum = sum+world.work[p+1]; count = count+1.0; }
                if iz > 0 { sum = sum+world.work[p-65]; count = count+1.0; } if iz < side { sum = sum+world.work[p+65]; count = count+1.0; }
                next = old+(sum/count-old)*vmath.min(1.0,strength*weight);
            }
            next = vmath.clamp(next,-1024.0,1024.0); if next != old { changed = true; world.data[20496+p] = next; }
        } iz = iz+1;
    } ix = ix+1; } return changed;
}
