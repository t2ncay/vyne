use "terrain.vy";

# Four retained Vector3 records are allocated with the resource, then overwritten
# numerically. DrawTriangle3D sees those records, not new boxes for every vertex.
fn se_world_draw(world :: SEWorld) -> Bool {
    data :: Array<Float64> := world.data;
    a := world.points[0]; b := world.points[1]; c := world.points[2]; d := world.points[3];
    count :: Int64 := int64(data[0]); side :: Int64 := int64(data[1]); spacing :: Float64 := data[2];
    i :: Int64 := 0; while i < count {
        p :: Int64 := 16+i*5; palette :: Int64 := int64(data[p+4]);
        a.x = data[p]; a.y = data[p+1]; a.z = data[p+2];
        vraylib.draw_cube(a,data[p+3],data[p+3],data[p+3],world.colors[palette]); i = i+1;
    }
    x :: Int64 := 0; while x < side { z :: Int64 := 0; while z < side {
        p :: Int64 := 20496+z*65+x; px :: Float64 := data[3]+float64(x)*spacing; pz :: Float64 := data[5]+float64(z)*spacing;
        a.x = px; a.y = data[4]+data[p]; a.z = pz;
        b.x = px+spacing; b.y = data[4]+data[p+1]; b.z = pz;
        c.x = px; c.y = data[4]+data[p+65]; c.z = pz+spacing;
        d.x = px+spacing; d.y = data[4]+data[p+66]; d.z = pz+spacing;
        vraylib.draw_triangle_3d(a,c,b,world.colors[8]); vraylib.draw_triangle_3d(b,c,d,world.colors[9]); z = z+1;
    } x = x+1; } return true;
}
# Scratch 4225 distance,4226..28 point,4229..31 normal,4232 cell (-1 terrain),
# 4233 hit. Scalar slab/triangle queries allocate no result/vertex boxes per cell.
fn se_world_ray(world :: SEWorld,ray :: vraylib.Ray) -> Bool {
    data :: Array<Float64> := world.data; work :: Array<Float64> := world.work;
    ox :: Float64 := ray.position.x; oy :: Float64 := ray.position.y; oz :: Float64 := ray.position.z;
    dx :: Float64 := ray.direction.x; dy :: Float64 := ray.direction.y; dz :: Float64 := ray.direction.z;
    work[4225] = 1000000000.0; work[4233] = 0.0;
    count :: Int64 := int64(data[0]); side :: Int64 := int64(data[1]); spacing :: Float64 := data[2];
    i :: Int64 := 0; while i < count {
        p :: Int64 := 16+i*5; r :: Float64 := data[p+3]*0.5; near :: Float64 := -1000000000.0; far :: Float64 := 1000000000.0;
        normal_axis :: Int64 := 0; normal_sign :: Float64 := -1.0; valid :: Bool := true;
        axis :: Int64 := 0; while axis < 3 {
            origin :: Float64 := ox; direction :: Float64 := dx;
            if axis == 1 { origin = oy; direction = dy; } if axis == 2 { origin = oz; direction = dz; }
            low :: Float64 := data[p+axis]-r; high :: Float64 := data[p+axis]+r;
            if vmath.abs(direction) < 0.0000001 { if origin < low || origin > high { valid = false; } }
            else {
                t0 :: Float64 := (low-origin)/direction; t1 :: Float64 := (high-origin)/direction; sign :: Float64 := -1.0;
                if t0 > t1 { temp :: Float64 := t0; t0 = t1; t1 = temp; sign = 1.0; }
                if t0 > near { near = t0; normal_axis = axis; normal_sign = sign; } far = vmath.min(far,t1);
            } axis = axis+1;
        }
        if near < 0.0 { near = 0.0; }
        if valid && near <= far && far >= 0.0 && near < work[4225] {
            work[4225] = near; work[4226] = ox+dx*near; work[4227] = oy+dy*near; work[4228] = oz+dz*near;
            work[4229] = 0.0; work[4230] = 0.0; work[4231] = 0.0; work[4229+normal_axis] = normal_sign; work[4232] = float64(i); work[4233] = 1.0;
        } i = i+1;
    }
    x :: Int64 := 0; while x < side { z :: Int64 := 0; while z < side {
        p :: Int64 := 20496+z*65+x; px :: Float64 := data[3]+float64(x)*spacing; pz :: Float64 := data[5]+float64(z)*spacing;
        triangle :: Int64 := 0; while triangle < 2 {
            ax :: Float64 := (data[p+1]-data[p])/spacing; az :: Float64 := (data[p+65]-data[p])/spacing; base :: Float64 := data[4]+data[p];
            if triangle == 1 { ax = (data[p+66]-data[p+65])/spacing; az = (data[p+66]-data[p+1])/spacing; base = data[4]+data[p+1]+data[p+65]-data[p+66]; }
            divisor :: Float64 := dy-ax*dx-az*dz;
            if vmath.abs(divisor) > 0.0000001 {
                t :: Float64 := (base+ax*(ox-px)+az*(oz-pz)-oy)/divisor;
                if t >= 0.0 && t < work[4225] {
                    u :: Float64 := (ox+dx*t-px)/spacing; v :: Float64 := (oz+dz*t-pz)/spacing;
                    if u >= -0.0000001 && v >= -0.0000001 && u <= 1.0000001 && v <= 1.0000001 && ((triangle == 0 && u+v <= 1.0000001) || (triangle == 1 && u+v >= 0.9999999)) {
                        length :: Float64 := vmath.sqrt(ax*ax+az*az+1.0); work[4225] = t; work[4226] = ox+dx*t; work[4227] = oy+dy*t; work[4228] = oz+dz*t;
                        work[4229] = -ax/length; work[4230] = 1.0/length; work[4231] = -az/length; work[4232] = -1.0; work[4233] = 1.0;
                    }
                }
            } triangle = triangle+1;
        } z = z+1;
    } x = x+1; } return work[4233] != 0.0;
}
