# device_tensor.vy - minimal metadata wrapper for @device handles.
#
# Ownership remains with the enclosing @device region. This wrapper exists to
# keep shape/length metadata beside the Int64 handle in explicit GPU training
# code without pretending handles can safely escape their device frame.

interface DeviceTensor {
    handle   :: Int64,
    height   :: Int64,
    width    :: Int64,
    channels :: Int64,
    n        :: Int64,
}
