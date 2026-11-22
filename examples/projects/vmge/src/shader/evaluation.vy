use "material.vy";
use external "voptim/voptim.vy";

# Captures are top-left RGBA linear Float64. RGB loss excludes alpha.
# Freeze context: time, position[3], target[3], up[3], fovy, projection.

# Frozen shader/material evaluation owner retaining camera/time, linear reference/difference pixels,
# RGB loss metrics, scalar fit IDs/gradients, SGD state, and checkpoint resource text.
#
# Fields:
#     state: Mutable numeric state/counters used by this operation.
#     context: Frozen time, eye XYZ, target XYZ, up XYZ, FOV, and projection.
#     metrics: RGB MSE, RGB MAE, last finite-difference epsilon, and learning rate.
#     reference: Retained top-left linear RGBA reference samples.
#     difference: Retained signed RGBA candidate-minus-reference samples.
#     ids: Up to sixteen eligible stable scalar fit-parameter IDs.
#     gradients: Finite-difference gradients parallel to selected fit IDs.
#     optimizer: SGD parameter state independent of authored material values.
#     material: Retained material payload.
#     document: Retained serialized frozen shader/schema with effective stage roots.
interface SSEvaluation {
    state :: Array<Int64>,
    context :: Array<Float64>,
    metrics :: Array<Float64>,
    reference :: Array<Float64>,
    difference :: Array<Float64>,
    ids :: Array<Int64>,
    gradients :: Array<Float64>,
    optimizer :: voptim.SGDState,
    material :: MaterialInstance,
    document :: Array<Int64>
}

# Create fixed matching reference/difference buffers and fitting storage for at most 256-by-256 RGBA
# samples and sixteen scalar parameters.
#
# Parameters:
#     width (Int64): Width or component count; rendering dimensions are measured in pixels.
#     height (Int64): Height; player geometry uses world units and render/UI dimensions use pixels.
#
# Returns:
#     SSEvaluation containing the resulting retained state or payload.
#
# Errors:
#     Evaluation resolution budget (256x256).
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn ss_evaluation(width :: Int64, height :: Int64) -> SSEvaluation {
    if width < 1 || height < 1 || width > 256 || height > 256 {
        throw "Evaluation resolution budget (256x256)";
    }
    e :: SSEvaluation := SSEvaluation(
        ss_ints(10),
        ss_floats(12),
        ss_floats(4),
        ss_floats(width * height * 4),
        ss_floats(width * height * 4),
        ss_ints(16),
        ss_floats(16),
        voptim.sgd_new(ss_floats(16)),
        ss_material(1, 1),
        ss_text(600000)
    );
    e.state[0] = width;
    e.state[1] = height;
    return e;
}

# Resolve a document stage's effective include root into an absolute normalized path for reproducible
# checkpoints.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     use_vertex (Bool): True selects vertex include-root paths; false selects fragment paths.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn ss_evaluation_stage_path(d :: SSDocument, use_vertex :: Bool) -> String {
    path :: String := se_text_read(d.fragment_path);
    if use_vertex {
        path = se_text_read(d.vertex_path);
    }
    if path == "" {
        path = se_text_read(d.path);
    }
    if path == "" {
        path = "__shader__.vshader";
    }
    if !vraylib.is_path_absolute(path) {
        path = vfs.join(vraylib.get_working_directory(), path);
    }
    return ss_normal_path(path);
}

# Serialize a staged shader copy with effective absolute vertex/fragment include roots.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn ss_evaluation_document_text(d :: SSDocument) -> String {
    staged :: SSDocument := ss_document();
    se_text_copy(staged.source, d.source);
    se_text_copy(staged.vertex, d.vertex);
    ss_parameters_copy(staged.parameters, d.parameters);
    se_text_write(staged.fragment_path, ss_evaluation_stage_path(d, false));
    se_text_write(staged.vertex_path, ss_evaluation_stage_path(d, true));
    return ss_document_text(staged);
}

# Require any existing GPU build to match current source/schema/include resources before evaluation;
# allow uncompiled CPU objectives.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     GPU evaluation requires current successfully compiled draft source and includes.
fn ss_evaluation_gpu_current(d :: SSDocument) -> Bool {
    if d.state[2] != 0 || d.state[4] != 0 {
        ss_document_dependencies_changed(d);
        if d.state[2] == 0
            || !ss_program(d).valid()
            || ss_document_compile_dirty(d)
            || ss_hash(d.source) != d.state[13]
            || ss_hash(d.vertex) != d.state[14] {
            throw "GPU evaluation requires current successfully compiled draft source and includes";
        }
    }
    return true;
}

# Hash full shader/schema/value text and resolved include paths/content to identify the frozen
# evaluation workload.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#
# Returns:
#     The computed Int64 value: result.
fn ss_evaluation_fingerprint(d :: SSDocument) -> Int64 {
    result :: Int64 := 0;
    @bounded<67108864> region evaluation_fingerprint {
        text :: Array<Int64> := ss_text(600000);
        se_text_write(text, ss_evaluation_document_text(d));
        result = ss_hash(text);
        prep :: SSPreprocessor := ss_preprocessor();
        fp :: String := ss_evaluation_stage_path(d, false);
        vp :: String := ss_evaluation_stage_path(d, true);
        if d.vertex[0] > 0 {
            ss_expand(prep, se_text_read(d.vertex), vp, 0, 0);
        }
        ss_expand(prep, se_text_read(d.source), fp, 0, 0);
        i :: Int64 := 0;
        while i < prep.state[0] {
            result = (result * 131 + ss_hash(prep.paths[i]) + prep.hashes[i]) % 2147483647;
            i = i + 1;
        }
    };
    return result;
}

# Validate and retain an immutable evaluation context, copy material/source/schema, and select
# eligible continuous scalar fit parameters.
#
# Parameters:
#     e (SSEvaluation): Frozen camera/time/material evaluation, linear pixel buffers, metrics, and
#         fitting/checkpoint state.
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     time (Float64): Shader/evaluation time in seconds.
#     camera (vraylib.Camera3D): Camera snapshot with position/target/up/FOV/projection values.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Invalid frozen time.
#     Fitting budget: 16 scalar continuous parameters.
#     Nonfinite frozen camera.
fn ss_evaluation_freeze(
    e :: SSEvaluation,
    d :: SSDocument,
    m :: MaterialInstance,
    time :: Float64,
    camera :: vraylib.Camera3D
) -> Bool {
    if !(vmath.abs(time) <= 1000000.0) {
        throw "Invalid frozen time";
    }
    ss_evaluation_gpu_current(d);
    ss_parameters_validate(d.parameters);
    count :: Int64 := 0;
    p :: SSParameters := d.parameters;
    i :: Int64 := 0;
    while i < p.state[0] {
        if p.meta[i * 8 + 4] == 1 && p.meta[i * 8 + 5] == 1 && p.meta[i * 8 + 3] == 1 {
            count = count + 1;
        }
        i = i + 1;
    }
    if count > 16 {
        throw "Fitting budget: 16 scalar continuous parameters";
    }
    fingerprint :: Int64 := ss_evaluation_fingerprint(d);
    @bounded<33554432> region evaluation_freeze {
        staged :: Array<Int64> := ss_text(600000);
        se_text_write(staged, ss_evaluation_document_text(d));
        candidate :: MaterialInstance := ss_material_like(e.material);
        ss_material_copy(candidate, m);
        ss_material_migrate(candidate, d);
        context :: Array<Float64> := [
            time,
            camera.position.x,
            camera.position.y,
            camera.position.z,
            camera.target.x,
            camera.target.y,
            camera.target.z,
            camera.up.x,
            camera.up.y,
            camera.up.z,
            camera.fovy,
            float64(camera.projection)
        ];
        i = 0;
        while i < 12 {
            if !(vmath.abs(context[i]) <= 1000000.0) {
                throw "Nonfinite frozen camera";
            }
            i = i + 1;
        }
        ss_material_copy(e.material, candidate);
        se_text_copy(e.document, staged);
        i = 0;
        while i < 12 {
            e.context[i] = context[i];
            i = i + 1;
        }
    };
    e.state[2] = 0;
    e.state[3] = 0;
    e.state[4] = 0;
    e.state[5] = 1;
    e.state[6] = ss_hash(d.source);
    e.state[7] = d.state[2];
    e.state[8] = ss_hash(d.vertex);
    e.state[9] = fingerprint;
    i = 0;
    while i < 16 {
        e.ids[i] = 0;
        e.gradients[i] = 0.0;
        e.optimizer.params[i] = 0.0;
        i = i + 1;
    }
    i = 0;
    while i < p.state[0] {
        if p.meta[i * 8 + 4] == 1 && p.meta[i * 8 + 5] == 1 && p.meta[i * 8 + 3] == 1 {
            n :: Int64 := e.state[4];
            e.ids[n] = p.meta[i * 8];
            e.optimizer.params[n] = ss_material_get(e.material, e.ids[n], 0);
            e.state[4] = n + 1;
        }
        i = i + 1;
    }
    return true;
}

# Attach a reopened evaluation checkpoint to a new successful GPU compilation only when
# source/schema/includes still match.
#
# Parameters:
#     e (SSEvaluation): Frozen camera/time/material evaluation, linear pixel buffers, metrics, and
#         fitting/checkpoint state.
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Checkpoint shader/schema/include resources differ.
fn ss_evaluation_rebind(e :: SSEvaluation, d :: SSDocument) -> Bool {
    ss_evaluation_gpu_current(d);
    if e.state[5] != 1
        || ss_hash(d.source) != e.state[6]
        || ss_hash(d.vertex) != e.state[8]
        || ss_evaluation_fingerprint(d) != e.state[9] {
        throw "Checkpoint shader/schema/include resources differ";
    }
    e.state[7] = d.state[2];
    return true;
}

# Reconstruct a temporary camera snapshot from the frozen numeric evaluation context.
#
# Parameters:
#     e (SSEvaluation): Frozen camera/time/material evaluation, linear pixel buffers, metrics, and
#         fitting/checkpoint state.
#
# Returns:
#     The computed vraylib.Camera3D value: vraylib.Camera3D(vraylib.vec3(e.context[1], e.context[2],
#     e.context[3]), vraylib.vec3(e.context[4], e.context[5], e.context[6]),
#     vraylib.vec3(e.context[7], e.context[8], e.context[9]), e.context[10], int64(e.context[11])).
fn ss_evaluation_camera(e :: SSEvaluation) -> vraylib.Camera3D {
    return vraylib.Camera3D(
        vraylib.vec3(e.context[1], e.context[2], e.context[3]),
        vraylib.vec3(e.context[4], e.context[5], e.context[6]),
        vraylib.vec3(e.context[7], e.context[8], e.context[9]),
        e.context[10],
        int64(e.context[11])
    );
}

# Validate and copy matching finite linear RGBA pixels into the retained reference buffer.
#
# Parameters:
#     e (SSEvaluation): Frozen camera/time/material evaluation, linear pixel buffers, metrics, and
#         fitting/checkpoint state.
#     pixels (Array<Float64>): Top-left, linear RGBA Float64 samples matching the evaluation
#         resolution.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Reference requires frozen matching RGBA capture.
#     Nonfinite reference capture.
fn ss_evaluation_reference(e :: SSEvaluation, pixels :: Array<Float64>) -> Bool {
    if e.state[5] != 1 || pixels.size() != e.reference.size() {
        throw "Reference requires frozen matching RGBA capture";
    }
    i :: Int64 := 0;
    while i < pixels.size() {
        if !(vmath.abs(pixels[i]) <= 1000000.0) {
            throw "Nonfinite reference capture";
        }
        i = i + 1;
    }
    i = 0;
    while i < pixels.size() {
        e.reference[i] = pixels[i];
        i = i + 1;
    }
    e.state[2] = 1;
    return true;
}

# Compute RGB mean-squared/mean-absolute error and retain signed RGBA differences; alpha is excluded
# from loss.
#
# Parameters:
#     e (SSEvaluation): Frozen camera/time/material evaluation, linear pixel buffers, metrics, and
#         fitting/checkpoint state.
#     pixels (Array<Float64>): Top-left, linear RGBA Float64 samples matching the evaluation
#         resolution.
#
# Returns:
#     RGB mean-squared error; RGB MAE and signed RGBA differences are also updated.
#
# Errors:
#     Loss requires a matching frozen reference.
#     Nonfinite evaluation capture.
fn ss_evaluation_compare(e :: SSEvaluation, pixels :: Array<Float64>) -> Float64 {
    if e.state[2] != 1 || pixels.size() != e.reference.size() {
        throw "Loss requires a matching frozen reference";
    }
    i :: Int64 := 0;
    while i < pixels.size() {
        if !(vmath.abs(pixels[i]) <= 1000000.0) {
            throw "Nonfinite evaluation capture";
        }
        i = i + 1;
    }
    mse :: Float64 := 0.0;
    mae :: Float64 := 0.0;
    i = 0;
    while i < pixels.size() {
        delta :: Float64 := pixels[i] - e.reference[i];
        e.difference[i] = delta;
        if i % 4 != 3 {
            mse = mse + delta * delta;
            mae = mae + vmath.abs(delta);
        }
        i = i + 1;
    }
    count :: Float64 := float64(e.state[0] * e.state[1] * 3);
    e.metrics[0] = mse / count;
    e.metrics[1] = mae / count;
    return e.metrics[0];
}

# Read matching render-target pixels and either retain a reference or compute comparison loss in
# bounded temporary storage.
#
# Parameters:
#     e (SSEvaluation): Frozen camera/time/material evaluation, linear pixel buffers, metrics, and
#         fitting/checkpoint state.
#     target (vraylib.RenderTarget): Managed render target owned by the current application session.
#     reference (Bool): True stores a new reference; false computes RGB loss against the retained
#         reference.
#
# Returns:
#     The computed Float64 value: result.
#
# Errors:
#     Evaluation target resolution mismatch.
fn ss_evaluation_capture(e :: SSEvaluation, target :: vraylib.RenderTarget, reference :: Bool) -> Float64 {
    if target.width() != e.state[0] || target.height() != e.state[1] {
        throw "Evaluation target resolution mismatch";
    }
    result :: Float64 := 0.0;
    @bounded<16777216> region evaluation_capture {
        pixels :: Array<Float64> := target.read_pixels();
        if reference {
            ss_evaluation_reference(e, pixels);
        } else {
            result = ss_evaluation_compare(e, pixels);
        }
    };
    return result;
}

# Copy complete evaluation/material/context/optimizer/pixel state into existing matching-capacity
# owners.
#
# Parameters:
#     to (SSEvaluation): Existing destination owner; its preallocated capacity must fit the copied
#         payload.
#     from (SSEvaluation): Source owner whose live payload is copied.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Evaluation copy capacity mismatch.
fn ss_evaluation_copy(to :: SSEvaluation, from :: SSEvaluation) -> Bool {
    if to.reference.size() != from.reference.size()
        || to.difference.size() != from.difference.size()
        || from.material.state[2] > ss_material_rows(to.material)
        || from.material.binding_state[0] > ss_material_binding_capacity(to.material) {
        throw "Evaluation copy capacity mismatch";
    }
    ss_material_copy(to.material, from.material);
    se_text_copy(to.document, from.document);
    i :: Int64 := 0;
    while i < 10 {
        to.state[i] = from.state[i];
        i = i + 1;
    }
    i = 0;
    while i < 12 {
        to.context[i] = from.context[i];
        i = i + 1;
    }
    i = 0;
    while i < 4 {
        to.metrics[i] = from.metrics[i];
        i = i + 1;
    }
    i = 0;
    while i < 16 {
        to.ids[i] = from.ids[i];
        to.gradients[i] = from.gradients[i];
        to.optimizer.params[i] = from.optimizer.params[i];
        i = i + 1;
    }
    i = 0;
    while i < from.reference.size() {
        to.reference[i] = from.reference[i];
        to.difference[i] = from.difference[i];
        i = i + 1;
    }
    return true;
}

# Estimate scalar gradients by finite differences, run SGD, clamp schema ranges, and publish only a
# finite final objective; restore the snapshot on failure.
#
# Parameters:
#     e (SSEvaluation): Frozen camera/time/material evaluation, linear pixel buffers, metrics, and
#         fitting/checkpoint state.
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     epsilon (Float64): Positive finite-difference perturbation, at most 1.
#     rate (Float64): Positive SGD learning rate, at most 1.
#     objective (callable): Synchronous callable accepting a candidate MaterialInstance and returning
#         finite loss.
#
# Returns:
#     The validated final objective loss after publishing the candidate fitting step.
#
# Errors:
#     Fitting requires unchanged frozen shader and scalar fit parameters.
#     Frozen shader schema/include resources changed.
#     Fit schema changed.
#     Fit parameter has no continuous interval.
#     Nonfinite fit objective.
#     Nonfinite/unbounded fit gradient.
#     Nonfinite final candidate objective.
#     Nonfinite final candidate metrics.
#     Nonfinite final candidate difference.
#
# Lifecycle:
#     The objective must synchronously render the supplied candidate under the frozen context. GPU
#     objectives require an active managed frame with balanced passes; failures restore the evaluation
#     snapshot.
fn ss_evaluation_step(
    e :: SSEvaluation,
    d :: SSDocument,
    epsilon :: Float64,
    rate :: Float64,
    objective
) -> Float64 {
    ss_evaluation_gpu_current(d);
    if e.state[5] != 1
        || e.state[4] == 0
        || !(epsilon > 0.0 && epsilon <= 1.0 && rate > 0.0 && rate <= 1.0)
        || ss_hash(d.source) != e.state[6]
        || ss_hash(d.vertex) != e.state[8]
        || d.state[2] != e.state[7] {
        throw "Fitting requires unchanged frozen shader and scalar fit parameters";
    }
    if ss_evaluation_fingerprint(d) != e.state[9] {
        throw "Frozen shader schema/include resources changed";
    }
    @bounded<33554432> region finite_difference {
        result :: Float64 := 0.0;
        snapshot :: SSEvaluation := ss_evaluation(e.state[0], e.state[1]);
        ss_evaluation_copy(snapshot, e);
        candidate :: MaterialInstance := ss_material_like(e.material);
        ss_material_copy(candidate, e.material);
        optimizer :: voptim.SGDState := voptim.sgd_new(ss_floats(16));
        gradients :: Array<Float64> := ss_floats(16);
        final_difference :: Array<Float64> := ss_floats(e.difference.size());
        final_metrics :: Array<Float64> := ss_floats(4);
        try {
            i :: Int64 := 0;
            while i < snapshot.state[4] {
                id :: Int64 := snapshot.ids[i];
                s :: Int64 := ss_parameter_slot(d.parameters, id);
                if s < 0
                    || d.parameters.meta[s * 8 + 3] != 1
                    || d.parameters.meta[s * 8 + 5] != 1
                    || d.parameters.meta[s * 8 + 4] != 1 {
                    throw "Fit schema changed";
                }
                x :: Float64 := ss_material_get(snapshot.material, id, 0);
                low :: Float64 := vmath.max(d.parameters.ranges[s * 2], x - epsilon);
                high :: Float64 := vmath.min(d.parameters.ranges[s * 2 + 1], x + epsilon);
                if high <= low {
                    throw "Fit parameter has no continuous interval";
                }
                ss_material_set(candidate, id, 0, high);
                plus :: Float64 := objective(candidate);
                ss_evaluation_copy(e, snapshot);
                ss_material_set(candidate, id, 0, low);
                minus :: Float64 := objective(candidate);
                ss_evaluation_copy(e, snapshot);
                ss_material_set(candidate, id, 0, x);
                if !(vmath.abs(plus) <= 1000000.0 && vmath.abs(minus) <= 1000000.0) {
                    throw "Nonfinite fit objective";
                }
                gradients[i] = (plus - minus) / (high - low);
                if !(vmath.abs(gradients[i]) <= 2147483648.0) {
                    throw "Nonfinite/unbounded fit gradient";
                }
                optimizer.params[i] = x;
                i = i + 1;
            }
            voptim.sgd_step(optimizer, voptim.Series(gradients, 16), rate);
            i = 0;
            while i < snapshot.state[4] {
                s :: Int64 := ss_parameter_slot(d.parameters, snapshot.ids[i]);
                v :: Float64 := vmath.max(
                    d.parameters.ranges[s * 2],
                    vmath.min(d.parameters.ranges[s * 2 + 1], optimizer.params[i])
                );
                ss_component_validate(d.parameters.meta[s * 8 + 1], v);
                optimizer.params[i] = v;
                ss_material_set(candidate, snapshot.ids[i], 0, v);
                i = i + 1;
            }
            result = float64(objective(candidate));
            if !(vmath.abs(result) <= 1000000.0) {
                throw "Nonfinite final candidate objective";
            }
            i = 0;
            while i < 4 {
                if !(vmath.abs(e.metrics[i]) <= 2147483648.0) {
                    throw "Nonfinite final candidate metrics";
                }
                final_metrics[i] = e.metrics[i];
                i = i + 1;
            }
            i = 0;
            while i < e.difference.size() {
                if !(vmath.abs(e.difference[i]) <= 2147483648.0) {
                    throw "Nonfinite final candidate difference";
                }
                final_difference[i] = e.difference[i];
                i = i + 1;
            }
            ss_evaluation_copy(e, snapshot);
            ss_material_copy(e.material, candidate);
            e.material.state[3] = snapshot.material.state[3] + 1;
            i = 0;
            while i < 16 {
                e.gradients[i] = gradients[i];
                e.optimizer.params[i] = optimizer.params[i];
                i = i + 1;
            }
            i = 0;
            while i < e.difference.size() {
                e.difference[i] = final_difference[i];
                i = i + 1;
            }
            i = 0;
            while i < 4 {
                e.metrics[i] = final_metrics[i];
                i = i + 1;
            }
            e.metrics[0] = result;
            e.metrics[2] = epsilon;
            e.metrics[3] = rate;
            e.state[3] = snapshot.state[3] + 1;
        } catch (error) {
            ss_evaluation_copy(e, snapshot);
            throw error;
        }
    };
    return e.metrics[0];
}

# Serialize frozen source/schema, camera/time, material, pixels, metrics, selected fit IDs, gradients,
# and SGD state without native tokens.
#
# Parameters:
#     e (SSEvaluation): Frozen camera/time/material evaluation, linear pixel buffers, metrics, and
#         fitting/checkpoint state.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn ss_evaluation_text(e :: SSEvaluation) -> String {
    b :: Int64 := vcore.sb_create();
    vcore.sb_append(b, "VMGE_EVALUATION|1\nS");
    i :: Int64 := 0;
    while i < 10 {
        # Compiled revision is session-local and restored as zero.
        v :: Int64 := e.state[i];
        if i == 7 {
            v = 0;
        }
        vcore.sb_append(b, "|" + string(v));
        i = i + 1;
    }
    vcore.sb_append(b, "\nC");
    i = 0;
    while i < 12 {
        vcore.sb_append(b, "|" + ss_encode(e.context[i]));
        i = i + 1;
    }
    vcore.sb_append(b, "\nL");
    i = 0;
    while i < 4 {
        vcore.sb_append(b, "|" + ss_encode(e.metrics[i]));
        i = i + 1;
    }
    vcore.sb_append(b, "\nD|" + se_text_hex(e.document) + "\n");
    material :: Array<Int64> := ss_text(262144);
    se_text_write(material, ss_material_text(e.material));
    vcore.sb_append(b, "M|" + se_text_hex(material) + "\n");
    i = 0;
    while i < e.state[4] {
        vcore.sb_append(
            b,
            "F|"
                +string(e.ids[i])
                +"|"
                +ss_encode(e.optimizer.params[i])
                +"|"
                +ss_encode(e.gradients[i])
                +"\n"
        );
        i = i + 1;
    }
    i = 0;
    while i < e.reference.size() {
        vcore.sb_append(b, "R|" + ss_encode(e.reference[i]) + "|" + ss_encode(e.difference[i]) + "\n");
        i = i + 1;
    }
    return vcore.sb_build(b);
}

# Atomically save the full exact evaluation checkpoint in a bounded serialization region.
#
# Parameters:
#     e (SSEvaluation): Frozen camera/time/material evaluation, linear pixel buffers, metrics, and
#         fitting/checkpoint state.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Cannot save evaluation checkpoint.
fn ss_evaluation_save(e :: SSEvaluation, path :: String) -> Bool {
    @bounded<134217728> region evaluation_checkpoint {
        if !vfs.write_atomic(path, ss_evaluation_text(e)) {
            throw "Cannot save evaluation checkpoint";
        }
    };
    return true;
}

# Validate a complete matching-resolution checkpoint in staging before copying it into the evaluation
# owner.
#
# Parameters:
#     e (SSEvaluation): Frozen camera/time/material evaluation, linear pixel buffers, metrics, and
#         fitting/checkpoint state.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Cannot read evaluation checkpoint/budget.
#     Unsupported evaluation version.
#     Checkpoint resolution mismatch.
#     Invalid checkpoint state.
#     Invalid checkpoint records.
#     Checkpoint shader hash mismatch.
#     Invalid checkpoint fit record.
#     Invalid checkpoint pixel.
#     Incomplete checkpoint.
fn ss_evaluation_load(e :: SSEvaluation, path :: String) -> Bool {
    @bounded<134217728> region evaluation_restore {
        raw := vfs.read(path);
        if raw == null || raw.size() > 20000000 {
            throw "Cannot read evaluation checkpoint/budget";
        }
        lines :: Array := ss_split(raw, "\n");
        if lines.size() < 6 || lines[0] != "VMGE_EVALUATION|1" {
            throw "Unsupported evaluation version";
        }
        s :: Array := ss_split(lines[1], "|");
        if s.size() != 11 || s[0] != "S" || ss_integer(s[1]) != e.state[0] || ss_integer(s[2]) != e.state[1] {
            throw "Checkpoint resolution mismatch";
        }
        candidate :: SSEvaluation := ss_evaluation(e.state[0], e.state[1]);
        i :: Int64 := 0;
        while i < 10 {
            candidate.state[i] = ss_integer(s[i + 1]);
            i = i + 1;
        }
        if candidate.state[4] < 0
            || candidate.state[4] > 16
            || candidate.state[2] < 0
            || candidate.state[2] > 1
            || candidate.state[5] != 1
            || candidate.state[3] < 0 {
            throw "Invalid checkpoint state";
        }
        c :: Array := ss_split(lines[2], "|");
        l :: Array := ss_split(lines[3], "|");
        doc :: Array := ss_split(lines[4], "|");
        mat :: Array := ss_split(lines[5], "|");
        if c.size() != 13
            || c[0] != "C"
            || l.size() != 5
            || l[0] != "L"
            || doc.size() != 2
            || doc[0] != "D"
            || mat.size() != 2
            || mat[0] != "M" {
            throw "Invalid checkpoint records";
        }
        i = 0;
        while i < 12 {
            candidate.context[i] = ss_number(c[i + 1]);
            i = i + 1;
        }
        i = 0;
        while i < 4 {
            candidate.metrics[i] = ss_number(l[i + 1]);
            i = i + 1;
        }
        ss_unhex(candidate.document, doc[1]);
        material :: Array<Int64> := ss_text(262144);
        ss_unhex(material, mat[1]);
        ss_material_parse(candidate.material, se_text_read(material));
        restored_doc :: SSDocument := ss_document();
        ss_document_parse(restored_doc, se_text_read(candidate.document));
        if ss_hash(restored_doc.source) != candidate.state[6]
            || ss_hash(restored_doc.vertex) != candidate.state[8] {
            throw "Checkpoint shader hash mismatch";
        }
        row :: Int64 := 6;
        fit :: Int64 := 0;
        pixel :: Int64 := 0;
        while row < lines.size() {
            if lines[row] != "" {
                f :: Array := ss_split(lines[row], "|");
                if f[0] == "F" {
                    if f.size() != 4 || fit >= candidate.state[4] {
                        throw "Invalid checkpoint fit record";
                    }
                    candidate.ids[fit] = ss_integer(f[1]);
                    candidate.optimizer.params[fit] = ss_number(f[2]);
                    candidate.gradients[fit] = ss_number(f[3]);
                    fit = fit + 1;
                } else {
                    if f[0] != "R" || f.size() != 3 || pixel >= candidate.reference.size() {
                        throw "Invalid checkpoint pixel";
                    }
                    candidate.reference[pixel] = ss_number(f[1]);
                    candidate.difference[pixel] = ss_number(f[2]);
                    pixel = pixel + 1;
                }
            }
            row = row + 1;
        }
        if fit != candidate.state[4] || pixel != candidate.reference.size() {
            throw "Incomplete checkpoint";
        }
        ss_material_copy(e.material, candidate.material);
        se_text_copy(e.document, candidate.document);
        i = 0;
        while i < 10 {
            e.state[i] = candidate.state[i];
            i = i + 1;
        }
        i = 0;
        while i < 12 {
            e.context[i] = candidate.context[i];
            i = i + 1;
        }
        i = 0;
        while i < 4 {
            e.metrics[i] = candidate.metrics[i];
            i = i + 1;
        }
        i = 0;
        while i < 16 {
            e.ids[i] = candidate.ids[i];
            e.gradients[i] = candidate.gradients[i];
            e.optimizer.params[i] = candidate.optimizer.params[i];
            i = i + 1;
        }
        i = 0;
        while i < e.reference.size() {
            e.reference[i] = candidate.reference[i];
            e.difference[i] = candidate.difference[i];
            i = i + 1;
        }
    };
    return true;
}
