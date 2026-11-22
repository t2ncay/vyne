use "pipeline.vy";

# Allocate this registry before frames. Sources/paths live only in SSDocument
# retained buffers; IDs are resource IDs, never native shader tokens.

# Fixed starter-gallery owner containing shader documents, stable IDs, and the reusable bloom
# pipeline.
#
# Fields:
#     documents: Preallocated shader-document registry parallel to document_ids.
#     ids: Stable shader-document IDs parallel to the candidate document bank.
#     pipeline: Retained pipeline payload.
interface SSGallery {
    documents :: Array,
    ids :: Array<Int64>,
    pipeline :: SSPipeline
}

# Return the starter gallery's display names in registry order.
#
# Returns:
#     Array containing the resulting retained state or payload.
fn ss_gallery_names() -> Array {
    return [
        "lit-material",
        "plasma-grid",
        "exposure-tonemap",
        "bloom-extract",
        "blur-horizontal",
        "blur-vertical",
        "bloom-composite"
    ];
}

# Return the starter gallery's stable document IDs in registry order.
#
# Returns:
#     Array<Int64> containing the resulting retained state or payload.
fn ss_gallery_ids() -> Array<Int64> {
    return [101, 102, 201, 202, 203, 204, 205];
}

# Resolve a gallery shader document by stable ID, rejecting unknown entries.
#
# Parameters:
#     g (SSGallery): Typed shader graph with stable node IDs and retained values.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#
# Returns:
#     The existing gallery document matching the requested stable ID.
#
# Errors:
#     Unknown gallery document ID.
fn ss_gallery_document(g :: SSGallery, id :: Int64) -> SSDocument {
    i :: Int64 := 0;
    while i < g.ids.size() {
        if g.ids[i] == id {
            return g.documents[i];
        }
        i = i + 1;
    }
    throw "Unknown gallery document ID";
}

# Define a bounded exposed scalar parameter in a gallery shader schema.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#     name (String): Display/resource/uniform name or environment-variable key, according to this API.
#     value (Float64): Candidate property/parameter value or display text, as typed below.
#     low (Float64): Inclusive lower limit.
#     high (Float64): Inclusive upper limit.
#     group_name (String): UI group label stored with the parameter schema.
#     units (String): Parameter units label for display.
#     fit (Bool): Whether the continuous scalar parameter is eligible for fitting.
#
# Returns:
#     The Boolean result of ss_parameter_define(d, id, name, 5126, 1, value, low, high, group_name,
#     units, true, fit).
fn ss_gallery_scalar(
    d :: SSDocument,
    id :: Int64,
    name :: String,
    value :: Float64,
    low :: Float64,
    high :: Float64,
    group_name :: String,
    units :: String,
    fit :: Bool
) -> Bool {
    return ss_parameter_define(d, id, name, 5126, 1, value, low, high, group_name, units, true, fit);
}

# Define a gallery vector parameter and set its individual component values.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#     name (String): Display/resource/uniform name or environment-variable key, according to this API.
#     x (Float64): World-space X coordinate.
#     y (Float64): World-space Y coordinate.
#     z (Float64): World-space Z coordinate.
#     low (Float64): Inclusive lower limit.
#     high (Float64): Inclusive upper limit.
#     group_name (String): UI group label stored with the parameter schema.
#     units (String): Parameter units label for display.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_gallery_vector(
    d :: SSDocument,
    id :: Int64,
    name :: String,
    x :: Float64,
    y :: Float64,
    z :: Float64,
    low :: Float64,
    high :: Float64,
    group_name :: String,
    units :: String
) -> Bool {
    ss_parameter_define(d, id, name, 35665, 1, x, low, high, group_name, units, true, false);
    ss_parameter_set(d, id, 1, y);
    ss_parameter_set(d, id, 2, z);
    ss_parameter_default_set(d, id, 1, y);
    ss_parameter_default_set(d, id, 2, z);
    return true;
}

# Populate the selected gallery shader's typed parameter defaults/ranges.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#
# Returns:
#     The Boolean result of ss_parameters_validate(d.parameters).
#
# Errors:
#     Unknown gallery schema ID.
fn ss_gallery_schema(d :: SSDocument, id :: Int64) -> Bool {
    d.parameters.state[0] = 0;
    if id == 101 {
        ss_gallery_vector(d, 1, "baseColor", 0.24, 0.46, 0.78, 0.0, 1.0, "Surface", "linear RGB");
        ss_gallery_vector(d, 2, "accentColor", 0.95, 0.50, 0.16, 0.0, 1.0, "Pattern", "linear RGB");
        ss_gallery_vector(d, 3, "lightDirection", 0.35, 0.65, 1.0, -1.0, 1.0, "Light", "world direction");
        ss_gallery_vector(d, 4, "lightColor", 1.0, 0.92, 0.78, 0.0, 4.0, "Light", "linear RGB");
        ss_gallery_scalar(d, 5, "ambient", 0.16, 0.0, 1.0, "Light", "fraction", true);
        ss_gallery_scalar(d, 6, "contrast", 1.4, 0.2, 4.0, "Surface", "power", true);
        ss_gallery_scalar(d, 7, "patternScale", 6.0, 0.25, 32.0, "Pattern", "cycles/model unit", true);
        ss_gallery_scalar(d, 8, "patternStrength", 0.5, 0.0, 1.0, "Pattern", "blend", true);
        ss_gallery_scalar(d, 9, "fresnelStrength", 0.28, 0.0, 1.0, "Rim", "linear intensity", true);
        ss_gallery_scalar(d, 10, "fresnelPower", 3.0, 1.0, 8.0, "Rim", "power", true);
        ss_gallery_vector(
            d,
            11,
            "cameraPosition",
            0.0,
            0.0,
            4.0,
            -1000000.0,
            1000000.0,
            "View",
            "world units"
        );
        ss_gallery_scalar(d, 12, "normalView", 0.0, 0.0, 1.0, "Debug", "normal-view blend", false);
    } else {
        if id == 102 {
            ss_gallery_vector(d, 1, "lowColor", 0.06, 0.12, 0.30, 0.0, 4.0, "Palette", "linear RGB");
            ss_gallery_vector(d, 2, "highColor", 0.80, 0.32, 0.10, 0.0, 4.0, "Palette", "linear RGB");
            ss_gallery_vector(d, 3, "gridColor", 0.70, 0.90, 1.0, 0.0, 4.0, "Grid", "linear RGB");
            ss_gallery_scalar(d, 4, "time", 0.0, 0.0, 3600.0, "Motion", "seconds", false);
            ss_gallery_scalar(d, 5, "period", 8.0, 1.0, 60.0, "Motion", "seconds/loop", false);
            ss_gallery_scalar(d, 6, "scale", 2.0, 0.25, 16.0, "Pattern", "cycles/UV", true);
            ss_gallery_scalar(d, 7, "contrast", 1.2, 0.1, 4.0, "Palette", "contrast", true);
            ss_gallery_scalar(d, 8, "gridCount", 8.0, 1.0, 64.0, "Grid", "cells/UV", false);
            ss_gallery_scalar(d, 9, "lineWidth", 0.025, 0.001, 0.2, "Grid", "cell fraction", true);
            ss_gallery_scalar(d, 10, "gridMix", 0.35, 0.0, 1.0, "Grid", "blend", true);
            ss_gallery_scalar(d, 11, "intensity", 1.5, 0.0, 8.0, "Palette", "linear multiplier", true);
        } else {
            if id == 201 {
                ss_gallery_scalar(d, 1, "exposure", 0.0, -6.0, 6.0, "Tonemap", "stops", true);
                ss_gallery_scalar(d, 2, "whitePoint", 1.0, 0.1, 8.0, "Tonemap", "linear reference", true);
                ss_gallery_scalar(d, 3, "saturation", 1.0, 0.0, 2.0, "Tonemap", "multiplier", true);
                ss_gallery_scalar(d, 4, "displayGamma", 1.0, 1.0, 3.0, "Output", "gamma power", false);
            } else {
                if id == 202 {
                    ss_gallery_scalar(d, 1, "threshold", 0.6, 0.0, 4.0, "Bloom", "linear intensity", true);
                    ss_gallery_scalar(d, 2, "knee", 0.3, 0.01, 2.0, "Bloom", "linear gate width", true);
                    ss_gallery_scalar(d, 3, "gain", 1.0, 0.0, 4.0, "Bloom", "multiplier", true);
                } else {
                    if id == 203 || id == 204 {
                        ss_gallery_scalar(
                            d,
                            1,
                            "texelStep",
                            0.0625,
                            0.00006103515625,
                            1.0,
                            "Sampling",
                            "UV/pixel",
                            false
                        );
                        ss_gallery_scalar(d, 2, "radius", 1.0, 0.0, 4.0, "Kernel", "pixel multiplier", true);
                    } else {
                        if id == 205 {
                            ss_gallery_scalar(d, 1, "strength", 0.35, 0.0, 2.0, "Bloom", "multiplier", true);
                            ss_gallery_vector(
                                d,
                                2,
                                "bloomTint",
                                1.0,
                                0.90,
                                0.75,
                                0.0,
                                2.0,
                                "Bloom",
                                "linear RGB"
                            );
                        } else {
                            throw "Unknown gallery schema ID";
                        }
                    }
                }
            }
        }
    }
    return ss_parameters_validate(d.parameters);
}

# Create preframe gallery document owners, stable registry IDs, and pipeline state.
#
# Parameters:
#     root (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#     from_resources (Bool): Whether to load serialized gallery resources instead of authoring from
#         source helpers.
#
# Returns:
#     SSGallery containing the resulting retained state or payload.
#
# Errors:
#     Missing gallery fragment source.
#     Missing gallery lit vertex source.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn ss_gallery_create(root :: String, from_resources :: Bool) -> SSGallery {
    docs :: Array := [];
    docs.reserve(7);
    ids :: Array<Int64> := ss_gallery_ids();
    names :: Array := ss_gallery_names();
    i :: Int64 := 0;
    while i < ids.size() {
        docs.push(ss_document());
        d :: SSDocument := docs[i];
        base :: String := vfs.join(root, names[i]);
        if from_resources {
            ss_document_load(d, base + ".vshader");
        } else {
            raw := vfs.read(base + ".fs");
            if raw == null {
                throw "Missing gallery fragment source";
            }
            se_text_write(d.source, raw);
            if ids[i] == 101 {
                vertex := vfs.read(base + ".vs");
                if vertex == null {
                    throw "Missing gallery lit vertex source";
                }
                se_text_write(d.vertex, vertex);
            }
            se_text_write(d.path, base + ".vshader");
            ss_document_source_change(d);
            ss_gallery_schema(d, ids[i]);
        }
        i = i + 1;
    }
    pipeline :: SSPipeline := ss_pipeline();
    ss_pipeline_load(pipeline, vfs.join(root, "bloom.vpipeline"));
    return SSGallery(docs, ids, pipeline);
}

# Create gallery owners and load the bundled self-contained shader resources from a project root.
#
# Parameters:
#     root (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     SSGallery containing the resulting retained state or payload.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn ss_gallery_load(root :: String) -> SSGallery {
    return ss_gallery_create(root, true);
}

# Authoring helper: rebuild documents from readable companion GLSL + schema.
# Save explicitly to desired .vshader paths; construction itself writes no files.

# Create gallery documents from the bundled GLSL sources and authored schema helpers.
#
# Parameters:
#     root (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     SSGallery containing the resulting retained state or payload.
fn ss_gallery_sources(root :: String) -> SSGallery {
    return ss_gallery_create(root, false);
}

# Compile every gallery shader through the common candidate/reflection path and report unsuccessful
# builds.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     g (SSGallery): Typed shader graph with stable node IDs and retained values.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Gallery shader.
fn ss_gallery_compile(app :: vraylib.App, g :: SSGallery) -> Bool {
    i :: Int64 := 0;
    while i < g.ids.size() {
        d :: SSDocument := g.documents[i];
        if !ss_compile(app, d) {
            throw "Gallery shader " + string(g.ids[i]) + ": " + se_text_read(d.log);
        }
        i = i + 1;
    }
    return true;
}

# Prepare gallery render targets and its bright-extract/blur/composite bloom pipeline at the requested
# resolution.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     g (SSGallery): Typed shader graph with stable node IDs and retained values.
#     width (Int64): Width or component count; rendering dimensions are measured in pixels.
#     height (Int64): Height; player geometry uses world units and render/UI dimensions use pixels.
#
# Returns:
#     The Boolean result of changed.
#
# Errors:
#     Invalid gallery resolution.
fn ss_gallery_prepare(app :: vraylib.App, g :: SSGallery, width :: Int64, height :: Int64) -> Bool {
    if width < 1 || height < 1 || width > 4096 || height > 4096 {
        throw "Invalid gallery resolution";
    }
    horizontal :: SSDocument := ss_gallery_document(g, 203);
    vertical :: SSDocument := ss_gallery_document(g, 204);
    x :: Float64 := 1.0 / float64(width);
    y :: Float64 := 1.0 / float64(height);
    ss_parameter_validate(horizontal, 1, 0, x);
    ss_parameter_validate(vertical, 1, 0, y);
    changed :: Bool := ss_pipeline_prepare(app, g.pipeline, width, height);
    if ss_parameter_set(horizontal, 1, 0, x) {
        changed = true;
    }
    if ss_parameter_set(vertical, 1, 0, y) {
        changed = true;
    }
    return changed;
}

# Release gallery pipeline targets and shader programs before the application closes.
#
# Parameters:
#     g (SSGallery): Typed shader graph with stable node IDs and retained values.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_gallery_close(g :: SSGallery) -> Bool {
    ss_pipeline_close(g.pipeline);
    i :: Int64 := 0;
    while i < g.documents.size() {
        ss_document_close(g.documents[i]);
        i = i + 1;
    }
    return true;
}
