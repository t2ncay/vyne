# Generated value types; resource types use Int64 handles.
ruleset { dynamic_casting };
module vraylib;
interface Vector2 {
    x :: Float64,
    y :: Float64,
}
interface Vector3 {
    x :: Float64,
    y :: Float64,
    z :: Float64,
}
interface Vector4 {
    x :: Float64,
    y :: Float64,
    z :: Float64,
    w :: Float64,
}
interface Matrix {
    m0 :: Float64,
    m4 :: Float64,
    m8 :: Float64,
    m12 :: Float64,
    m1 :: Float64,
    m5 :: Float64,
    m9 :: Float64,
    m13 :: Float64,
    m2 :: Float64,
    m6 :: Float64,
    m10 :: Float64,
    m14 :: Float64,
    m3 :: Float64,
    m7 :: Float64,
    m11 :: Float64,
    m15 :: Float64,
}
interface Color {
    r :: Int64,
    g :: Int64,
    b :: Int64,
    a :: Int64,
}
interface Rectangle {
    x :: Float64,
    y :: Float64,
    width :: Float64,
    height :: Float64,
}
interface NPatchInfo {
    source :: vraylib.Rectangle,
    left :: Int64,
    top :: Int64,
    right :: Int64,
    bottom :: Int64,
    layout :: Int64,
}
interface Camera3D {
    position :: vraylib.Vector3,
    target :: vraylib.Vector3,
    up :: vraylib.Vector3,
    fovy :: Float64,
    projection :: Int64,
}
interface Camera2D {
    offset :: vraylib.Vector2,
    target :: vraylib.Vector2,
    rotation :: Float64,
    zoom :: Float64,
}
interface MaterialMap {
    texture :: Int64,
    color :: vraylib.Color,
    value :: Float64,
}
interface Transform {
    translation :: vraylib.Vector3,
    rotation :: vraylib.Vector4,
    scale :: vraylib.Vector3,
}
interface BoneInfo {
    name :: Array<Int64>,
    parent :: Int64,
}
interface Ray {
    position :: vraylib.Vector3,
    direction :: vraylib.Vector3,
}
interface RayCollision {
    hit :: Bool,
    distance :: Float64,
    point :: vraylib.Vector3,
    normal :: vraylib.Vector3,
}
interface BoundingBox {
    min :: vraylib.Vector3,
    max :: vraylib.Vector3,
}
interface VrDeviceInfo {
    hResolution :: Int64,
    vResolution :: Int64,
    hScreenSize :: Float64,
    vScreenSize :: Float64,
    eyeToScreenDistance :: Float64,
    lensSeparationDistance :: Float64,
    interpupillaryDistance :: Float64,
    lensDistortionValues :: Array<Float64>,
    chromaAbCorrection :: Array<Float64>,
}
interface VrStereoConfig {
    projection :: Array,
    viewOffset :: Array,
    leftLensCenter :: Array<Float64>,
    rightLensCenter :: Array<Float64>,
    leftScreenCenter :: Array<Float64>,
    rightScreenCenter :: Array<Float64>,
    scale :: Array<Float64>,
    scaleIn :: Array<Float64>,
}
interface AutomationEvent {
    frame :: Int64,
    event_type :: Int64,
    params :: Array<Int64>,
}
interface float3 {
    v :: Array<Float64>,
}
interface float16 {
    v :: Array<Float64>,
}
interface rlDrawCall {
    mode :: Int64,
    vertexCount :: Int64,
    vertexAlignment :: Int64,
    textureId :: Int64,
}
