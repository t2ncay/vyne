# vtensor/Types.vy - metadata wrappers for region-local device handles.

ruleset { dynamic_casting };

module vtensor;

interface DeviceTensor {
    handle   :: Int64,
    height   :: Int64,
    width    :: Int64,
    channels :: Int64,
    n        :: Int64,
}
