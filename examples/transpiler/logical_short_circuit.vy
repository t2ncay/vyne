fn logical_check(ok,message :: String) -> Int64 { if !ok { throw message; } return 1; }
fn logical_forbidden() -> Int64 { throw "A short-circuited RHS was evaluated"; return 0; }
fn logical_touch(state :: Array,result :: Int64) -> Int64 { state[0] = state[0]+1; return result; }
interface LogicalCounter {
    values :: Array<Int64>,
    touch() -> Int64 { self.values[0] = self.values[0]+1; return 1; }
}
interface LogicalIntegerHeight { value :: Int64, height() -> Int64 { return self.value; } }
interface LogicalFloatHeight { value :: Float64, height() -> Float64 { return self.value; } }
state :: Array := [0]; counter :: LogicalCounter := LogicalCounter([0]);
a :: Int64 := int64(false && logical_forbidden()); b :: Int64 := int64(true || logical_forbidden());
logical_check(!a && b,"boxed assignments preserve short-circuit values");
if false && logical_forbidden() { throw "false AND branch entered"; }
if true || logical_forbidden() { state[0] = state[0]+1; }
logical_check(state[0] == 1,"conditions skip RHS calls");
logical_check(logical_touch(state,0) && logical_touch(state,1) == 0 || state[0] == 2,"left operand runs once before nested logical expression");
logical_check(logical_touch(state,1) || logical_touch(state,0),"true OR skips side effect");
logical_check(state[0] == 3,"side-effect count");
if counter.values[0] == 0 && counter.touch() { logical_check(counter.values[0] == 1,"conditional member mutation re-reads owned storage"); }
items :: Array := [7]; index :: Int64 := 1;
logical_check(!(index < items.size() && items[index] == 7),"array bounds guard skips invalid index");
i :: Int64 := 0; state[0] = 0;
while i < 3 && logical_touch(state,1) { i = i+1; }
logical_check(i == 3 && state[0] == 3,"loop guards evaluate RHS only on live iterations");
logical_check(0 || 2,"numeric truthiness");
integer_height :: LogicalIntegerHeight := LogicalIntegerHeight(64);
float_height :: LogicalFloatHeight := LogicalFloatHeight(64.5);
logical_check(integer_height.height() == 64 && float_height.height() > 64,"receiver-specific method return tags survive logical comparisons");
out("logical short circuit: PASS");
