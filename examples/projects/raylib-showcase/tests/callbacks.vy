use "types_native.vy";
use "types_boxed.vy";
ruleset { dynamic_casting };
fn check(ok :: Bool, message :: String) -> Bool { if !ok { throw message; } return true; }
interface Dispatcher {
    invoke(callback, value :: Int64) -> Int64 { return callback(value); }
}
fn increment(value :: Int64) -> Int64 { return value + 1; }
fn wrong_arity() -> Int64 { return 0; }
dispatcher := Dispatcher();
function_value := increment;
check(function_value(41) == 42, "named function value call");
check(dispatcher.invoke(increment, 41) == 42, "boxed method callback parameter");
failed :: Bool := false;
try { dispatcher.invoke(wrong_arity, 41); } catch (error) { failed = true; }
check(failed, "callback arity mismatch is catchable");
failed = false;
try { dispatcher.invoke(123, 41); } catch (error) { failed = true; }
check(failed, "non-callable value is catchable");
flat := NativeTypes.Item(42);
boxed := BoxedTypes.Item([1.0, 2.0]);
check(flat.value == 42 && boxed.values.size() == 2, "same-name interfaces keep distinct layouts");
out("VRAYLIB_CALLBACKS_OK");
