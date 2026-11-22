# One catalog drives validation, labels, search and creation. Literal strings
# need no commit arena; callers may reconstruct temporary search strings.

# Report whether a node kind is one of the eight supported catalog types.
#
# Parameters:
#     kind (Int64): Catalog/tool/graph operation kind index used by the selected subsystem.
#
# Returns:
#     The Boolean result of kind >= 0 && kind < 8.
fn se_node_kind_valid(kind :: Int64) -> Bool {
    return kind >= 0 && kind < 8;
}

# Return the catalog display name for a scene-node kind.
#
# Parameters:
#     kind (Int64): Catalog/tool/graph operation kind index used by the selected subsystem.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
#
# Errors:
#     Unknown node type.
fn se_type_name(kind :: Int64) -> String {
    if kind == 0 {
        return "Node3D";
    }
    if kind == 1 {
        return "MeshInstance3D";
    }
    if kind == 2 {
        return "Camera3D";
    }
    if kind == 3 {
        return "CollisionShape3D";
    }
    if kind == 4 {
        return "CharacterBody3D";
    }
    if kind == 5 {
        return "Sky";
    }
    if kind == 6 {
        return "Directional Light";
    }
    if kind == 7 {
        return "Point Light";
    }
    throw "Unknown node type";
}

# Return the picker category for a catalog node kind.
#
# Parameters:
#     kind (Int64): Catalog/tool/graph operation kind index used by the selected subsystem.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn se_node_category(kind :: Int64) -> String {
    if kind >= 5 {
        return "Environment";
    }
    if kind == 3 || kind == 4 {
        return "Physics";
    }
    return "Scene";
}

# Return the picker description explaining a catalog node kind's purpose.
#
# Parameters:
#     kind (Int64): Catalog/tool/graph operation kind index used by the selected subsystem.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn se_node_description(kind :: Int64) -> String {
    if kind == 0 {
        return "Organize a parent/child hierarchy";
    }
    if kind == 1 {
        return "Render geometry with a material";
    }
    if kind == 2 {
        return "Player view and custom camera effects";
    }
    if kind == 3 {
        return "Collision volume";
    }
    if kind == 4 {
        return "Controllable player body";
    }
    if kind == 5 {
        return "Procedural horizon, zenith and sun";
    }
    if kind == 6 {
        return "Sunlight from a rotated local +Z axis";
    }
    return "Local lamp with bounded range";
}

# Match a search query against a node kind's name, category, description, and search aliases.
#
# Parameters:
#     kind (Int64): Catalog/tool/graph operation kind index used by the selected subsystem.
#     query (String): Case-insensitive substring search text.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn se_node_matches(kind :: Int64, query :: String) -> Bool {
    text :: String := se_type_name(kind) + " " + se_node_category(kind) + " " + se_node_description(kind);
    if kind == 6 {
        text = text + " sun moon";
    }
    if kind == 7 {
        text = text + " lamp";
    }
    text = vraylib.text_to_lower(text);
    needle :: String := vraylib.text_to_lower(query);
    i :: Int64 := 0;
    while i + needle.size() <= text.size() {
        j :: Int64 := 0;
        match :: Bool := true;
        while j < needle.size() {
            if text[i + j] != needle[j] {
                match = false;
            }
            j = j + 1;
        }
        if match {
            return true;
        }
        i = i + 1;
    }
    return false;
}
