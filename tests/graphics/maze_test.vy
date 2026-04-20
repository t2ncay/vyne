ruleset { dynamic_casting };
module vglib; module vmath;

# Settings-i birbaşa rəqəmlərlə yazırıq ki, interpretator yorulmasın
const CELL_SIZE :: Int64 = 20;
const COLS :: Int64 = 40;

interface Cell {
    x :: Int64, y :: Int64, visited :: Int64, walls :: Array
}

fn get_idx(x :: Int64, y :: Int64) -> Int64 {
    if x < 0 || y < 0 || x >= 40 || y >= 40 { return -1; }
    return y * 40 + x;
}

# VACİB: Obyektləri deyil, indeksləri ötürürük
fn remove_walls_by_idx(grid :: Array&, i1 :: Int64, i2 :: Int64) {
    c1 :: Cell = grid[i1];
    c2 :: Cell = grid[i2];

    dx :: Int64 = (c1.x / 20) - (c2.x / 20);
    if dx == 1 {
        grid[i1].walls[3] = 0; # Left wall of c1
        grid[i2].walls[1] = 0; # Right wall of c2
    } else if dx == -1 {
        grid[i1].walls[1] = 0; # Right wall of c1
        grid[i2].walls[3] = 0; # Left wall of c2
    }
    
    dy :: Int64 = (c1.y / 20) - (c2.y / 20);
    if dy == 1 {
        grid[i1].walls[0] = 0;
        grid[i2].walls[2] = 0;
    } else if dy == -1 {
        grid[i1].walls[2] = 0;
        grid[i2].walls[0] = 0;
    }
}

fn get_neighbors(grid :: Array&, idx :: Int64) -> Array {
    ns :: Array = [];
    c :: Cell = grid[idx];
    cx :: Int64 = c.x / 20;
    cy :: Int64 = c.y / 20;

    indices :: Array = [
        get_idx(cx, cy - 1), get_idx(cx + 1, cy),
        get_idx(cx, cy + 1), get_idx(cx - 1, cy)
    ];

    through i :: indices -> loop {
        if i != -1 {
            if grid[i].visited == 0 { ns.push(i); }
        }
    };
    return ns;
}

fn main() {
    vglib.init(800, 800, 60, "Vyne Stable Maze", 64);
    cells :: Array = [];
    
    through y :: 0..39 -> loop {
        through x :: 0..39 -> loop {
            c :: Cell = Cell();
            c.x = x * 20; c.y = y * 20;
            c.visited = 0; c.walls = [1, 1, 1, 1];
            cells.push(c);
        }
    };

    stack :: Array = [];
    curr :: Int64 = 0;
    cells[curr].visited = 1;
    stack.push(curr);

    while vglib.running() {
        if stack.size() > 0 {
            through s :: 0..10 -> loop {
                if stack.size() > 0 {
                    idx :: Int64 = stack.back();
                    ns :: Array = get_neighbors(cells, idx);

                    if ns.size() > 0 {
                        next_idx :: Int64 = ns[vmath.random(0, ns.size() - 1)];
                        
                        # DIVARLARI SİL (Bura kritikdir!)
                        remove_walls_by_idx(cells, idx, next_idx);
                        
                        cells[next_idx].visited = 1;
                        stack.push(next_idx);
                    } else {
                        stack.pop();
                    }
                }
            };
        }

        vglib.begin();
        vglib.clear(vglib.WHITE);
        
        through c :: cells -> loop {
            if c.walls[0] == 1 { vglib.line(c.x, c.y, c.x + 20, c.y, 4294967295); }
            if c.walls[1] == 1 { vglib.line(c.x + 20, c.y, c.x + 20, c.y + 20, 4294967295); }
            if c.walls[2] == 1 { vglib.line(c.x, c.y + 20, c.x + 20, c.y + 20, 4294967295); }
            if c.walls[3] == 1 { vglib.line(c.x, c.y, c.x, c.y + 20, 4294967295); }
        };
        
        # Qırmızı nöqtə (Cari hüceyrə)
        if stack.size() > 0 {
            head :: Cell = cells[stack.back()];
            vglib.rect(head.x + 4, head.y + 4, 12, 12, vglib.RED);
        }
        
        vglib.end();
    }
}
main();