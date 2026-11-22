use "workspace_layout.vy";

# Shared source is [byte length, UTF-8 bytes]. All retained storage is preframe.
# state: caret, anchor, top line, horizontal cells, focus, revision, indexed revision,
# history cursor/count, overflow, evictions, preferred column, drag, find focus,
# token revision/top/count, line count, token overflow, cached visible count,
# circular history origin (20).
interface SSCodeEditor {
    source :: Array<Int64>, state :: Array<Int64>, edit_buffer :: Array<Int64>,
    history :: Array<Int64>, lines :: Array<Int64>, blocks :: Array<Int64>,
    spans :: Array<Int64>, query :: Array<Int64>, history_carets :: Array<Int64>,
    history_last :: Array<Int64>, history_lengths :: Array<Int64>,
}
fn ss_code_editor(source :: Array<Int64>) -> SSCodeEditor {
    state :: Array<Int64> := []; state.reserve(24); state.resize(24,0); state[5] = 1; state[6] = -1; state[11] = -1; state[14] = -1;
    edit_buffer :: Array<Int64> := []; edit_buffer.reserve(source.size()); edit_buffer.resize(source.size(),0);
    # Payload only: 16 power-of-two-sized banks plus a separate seventeenth.
    # Lengths/carets are metadata, avoiding a 65537 stride doubling the whole bank.
    payload :: Int64 := source.size()-1;
    history :: Array<Int64> := []; history.reserve(payload*16); history.resize(payload*16,0);
    history_last :: Array<Int64> := []; history_last.reserve(payload); history_last.resize(payload,0);
    history_lengths :: Array<Int64> := []; history_lengths.reserve(17); history_lengths.resize(17,0);
    lines :: Array<Int64> := []; lines.reserve(source.size()); lines.resize(source.size(),0);
    blocks :: Array<Int64> := []; blocks.reserve(source.size()); blocks.resize(source.size(),0);
    spans :: Array<Int64> := []; spans.reserve(16384); spans.resize(16384,0);
    query :: Array<Int64> := []; query.reserve(256); query.resize(256,0);
    history_carets :: Array<Int64> := []; history_carets.reserve(34); history_carets.resize(34,0);
    editor :: SSCodeEditor := SSCodeEditor(source,state,edit_buffer,history,lines,blocks,spans,query,history_carets,history_last,history_lengths);
    ss_code_snapshot(editor,0); ss_code_index(editor); return editor;
}
fn ss_code_snapshot(editor :: SSCodeEditor,slot :: Int64) -> Bool {
    physical :: Int64 := (editor.state[20]+slot)%17; base :: Int64 := physical*(editor.source.size()-1); i :: Int64 := 0;
    while i < editor.source[0] { if physical == 16 { editor.history_last[i] = editor.source[i+1]; } else { editor.history[base+i] = editor.source[i+1]; } i = i+1; }
    editor.history_lengths[physical] = editor.source[0]; return ss_code_snapshot_carets(editor,slot);
}
fn ss_code_snapshot_carets(editor :: SSCodeEditor,slot :: Int64) -> Bool {
    physical :: Int64 := (editor.state[20]+slot)%17;
    editor.history_carets[physical*2] = editor.state[0]; editor.history_carets[physical*2+1] = editor.state[1]; return true;
}
fn ss_code_reset_history(editor :: SSCodeEditor) -> Bool {
    editor.state[7] = 0; editor.state[8] = 0; editor.state[20] = 0; editor.state[0] = 0; editor.state[1] = 0;
    editor.state[2] = 0; editor.state[3] = 0; editor.state[5] = editor.state[5]+1;
    ss_code_snapshot(editor,0); return ss_code_index(editor);
}
fn ss_code_record(editor :: SSCodeEditor) -> Bool {
    if editor.state[7] == 16 {
        # Rotate logical slots; no multi-megabyte bank copy on each eviction.
        editor.state[20] = (editor.state[20]+1)%17;
        editor.state[7] = 15; editor.state[10] = editor.state[10]+1;
    }
    editor.state[7] = editor.state[7]+1; editor.state[8] = editor.state[7];
    ss_code_snapshot(editor,editor.state[7]); editor.state[5] = editor.state[5]+1;
    editor.state[9] = 0; editor.state[11] = -1; return true;
}
fn ss_code_undo(editor :: SSCodeEditor,redo :: Bool) -> Bool {
    slot :: Int64 := editor.state[7]-1; if redo { slot = editor.state[7]+1; }
    if slot < 0 || slot > editor.state[8] { return false; }
    physical :: Int64 := (editor.state[20]+slot)%17; base :: Int64 := physical*(editor.source.size()-1); i :: Int64 := 0;
    editor.source[0] = editor.history_lengths[physical];
    while i < editor.source[0] { if physical == 16 { editor.source[i+1] = editor.history_last[i]; } else { editor.source[i+1] = editor.history[base+i]; } i = i+1; }
    editor.state[7] = slot; editor.state[0] = int64(vmath.min(float64(editor.history_carets[physical*2]),float64(editor.source[0])));
    # Clamp to a UTF-8 boundary after restoring a differently shaped document.
    while editor.state[0] > 0 && editor.state[0] < editor.source[0] && editor.source[editor.state[0]+1] >= 128 && editor.source[editor.state[0]+1] < 192 { editor.state[0] = editor.state[0]-1; }
    editor.state[1] = int64(vmath.min(float64(editor.history_carets[physical*2+1]),float64(editor.source[0]))); editor.state[5] = editor.state[5]+1; return ss_code_index(editor);
}
fn ss_code_boundary(source :: Array<Int64>,position :: Int64) -> Bool {
    if position < 0 || position > source[0] { return false; }
    if position == source[0] { return true; } return source[position+1] < 128 || source[position+1] >= 192;
}
fn ss_code_valid(bytes :: Array) -> Bool {
    if bytes[0] < 0 || bytes[0] >= bytes.size() { return false; } i :: Int64 := 1;
    while i <= bytes[0] {
        ch :: Int64 := bytes[i]; if ch == 0 || ch == 13 || ch < 0 || ch > 244 { return false; }
        width :: Int64 := 1; if ch >= 128 { if ch < 194 { return false; } width = 2; if ch >= 224 { width = 3; } if ch >= 240 { width = 4; } }
        if i+width-1 > bytes[0] { return false; }
        j :: Int64 := 1; while j < width { if bytes[i+j] < 128 || bytes[i+j] > 191 { return false; } j = j+1; }
        if width >= 3 { if (ch == 224 && bytes[i+1] < 160) || (ch == 237 && bytes[i+1] >= 160) || (ch == 240 && bytes[i+1] < 144) || (ch == 244 && bytes[i+1] >= 144) { return false; } }
        i = i+width;
    } return true;
}
fn ss_code_replace(editor :: SSCodeEditor,left :: Int64,right :: Int64,bytes :: Array) -> Bool {
    if left < 0 || right < left || right > editor.source[0] { return false; }
    if !ss_code_boundary(editor.source,left) || !ss_code_boundary(editor.source,right) || !ss_code_valid(bytes) { editor.state[9] = 2; return false; }
    length :: Int64 := editor.source[0]-right+left+bytes[0];
    if length >= editor.source.size() { editor.state[9] = 1; return false; }
    i :: Int64 := 1;
    if left == right && bytes[0] == 0 { return false; }
    ss_code_snapshot_carets(editor,editor.state[7]);
    # Copy tail direction handles both growth and shrink without allocating.
    delta :: Int64 := bytes[0]-(right-left);
    if delta > 0 { i = editor.source[0]; while i > right { editor.source[i+delta] = editor.source[i]; i = i-1; } }
    else { i = right+1; while i <= editor.source[0] { editor.source[i+delta] = editor.source[i]; i = i+1; } }
    i = 1; while i <= bytes[0] { editor.source[left+i] = bytes[i]; i = i+1; }
    editor.source[0] = length; editor.state[0] = left+bytes[0]; editor.state[1] = editor.state[0];
    ss_code_record(editor); return ss_code_index(editor);
}
fn ss_code_insert(editor :: SSCodeEditor,bytes :: Array) -> Bool {
    return ss_code_replace(editor,int64(vmath.min(float64(editor.state[0]),float64(editor.state[1]))),int64(vmath.max(float64(editor.state[0]),float64(editor.state[1]))),bytes);
}
fn ss_code_index(editor :: SSCodeEditor) -> Bool {
    if editor.state[6] == editor.state[5] { return true; }
    count :: Int64 := 1; editor.lines[0] = 0; editor.blocks[0] = 0;
    block :: Int64 := 0; comment :: Bool := false; i :: Int64 := 0;
    while i < editor.source[0] {
        ch :: Int64 := editor.source[i+1]; next :: Int64 := 0; if i+1 < editor.source[0] { next = editor.source[i+2]; }
        if ch == 10 { editor.lines[count] = i+1; editor.blocks[count] = block; count = count+1; comment = false; }
        if !comment {
            if block == 0 && ch == 47 && next == 47 { comment = true; }
            if block == 0 && ch == 47 && next == 42 { block = 1; i = i+1; }
            else { if block == 1 && ch == 42 && next == 47 { block = 0; i = i+1; } }
        }
        i = i+1;
    }
    editor.state[17] = count; editor.state[6] = editor.state[5]; return true;
}
fn ss_code_line(editor :: SSCodeEditor,caret :: Int64) -> Int64 {
    ss_code_index(editor); low :: Int64 := 0; high :: Int64 := editor.state[17]-1;
    while low < high { middle :: Int64 := (low+high+1)/2; if editor.lines[middle] <= caret { low = middle; } else { high = middle-1; } } return low;
}
fn ss_code_end(editor :: SSCodeEditor,line :: Int64) -> Int64 {
    if line+1 >= editor.state[17] { return editor.source[0]; } return editor.lines[line+1]-1;
}
fn ss_code_column(editor :: SSCodeEditor,start :: Int64,caret :: Int64) -> Int64 {
    column :: Int64 := 0; i :: Int64 := start;
    while i < caret { if editor.source[i+1] == 9 { column = column+4-column%4; } else { column = column+1; } i = se_edit_next(editor.source,i); } return column;
}
fn ss_code_at(editor :: SSCodeEditor,line :: Int64,column :: Int64) -> Int64 {
    selected_line :: Int64 := int64(vmath.clamp(float64(line),0.0,float64(editor.state[17]-1)));
    i :: Int64 := editor.lines[selected_line]; cells :: Int64 := 0; end :: Int64 := ss_code_end(editor,selected_line);
    while i < end && cells < column { if editor.source[i+1] == 9 { cells = cells+4-cells%4; } else { cells = cells+1; } i = se_edit_next(editor.source,i); } return i;
}
fn ss_code_jump(editor :: SSCodeEditor,line :: Int64,column :: Int64) -> Bool {
    editor.state[0] = ss_code_at(editor,line-1,column-1); editor.state[1] = editor.state[0]; editor.state[2] = int64(vmath.max(0.0,float64(line-4))); editor.state[4] = 1; return true;
}
fn ss_code_indent(editor :: SSCodeEditor,unindent :: Bool) -> Bool {
    left :: Int64 := int64(vmath.min(float64(editor.state[0]),float64(editor.state[1])));
    right :: Int64 := int64(vmath.max(float64(editor.state[0]),float64(editor.state[1])));
    first :: Int64 := ss_code_line(editor,left); last :: Int64 := ss_code_line(editor,right);
    if right > left && right == editor.lines[last] { last = last-1; }
    left = editor.lines[first]; right = ss_code_end(editor,last); i :: Int64 := left; n :: Int64 := 0;
    while i <= right {
        if i == left || (i > left && editor.source[i] == 10) {
            if unindent { removed :: Int64 := 0; if i < right && editor.source[i+1] == 9 { i = i+1; } else { while i < right && editor.source[i+1] == 32 && removed < 4 { i = i+1; removed = removed+1; } } }
            else { j :: Int64 := 0; while j < 4 { if n+1 >= editor.edit_buffer.size() { editor.state[9] = 1; return false; } n = n+1; editor.edit_buffer[n] = 32; j = j+1; } }
        }
        if i < right { if n+1 >= editor.edit_buffer.size() { editor.state[9] = 1; return false; } n = n+1; editor.edit_buffer[n] = editor.source[i+1]; } i = i+1;
    }
    editor.edit_buffer[0] = n; if !ss_code_replace(editor,left,right,editor.edit_buffer) { return false; }
    editor.state[1] = left; ss_code_snapshot_carets(editor,editor.state[7]); return true;
}
fn ss_code_string_valid(text :: String) -> Bool {
    i :: Int64 := 0; length :: Int64 := text.size();
    while i < length {
        ch :: Int64 := text.byte_at(i); if ch == 0 || ch < 0 || ch > 244 { return false; }
        width :: Int64 := 1; if ch >= 128 { if ch < 194 { return false; } width = 2; if ch >= 224 { width = 3; } if ch >= 240 { width = 4; } }
        if i+width > length { return false; }
        j :: Int64 := 1; while j < width { byte :: Int64 := text.byte_at(i+j); if byte < 128 || byte > 191 { return false; } j = j+1; }
        if width >= 3 { second :: Int64 := text.byte_at(i+1); if (ch == 224 && second < 160) || (ch == 237 && second >= 160) || (ch == 240 && second < 144) || (ch == 244 && second >= 144) { return false; } }
        i = i+width;
    } return true;
}
# Validate original bytes before any Unicode decoder can replace malformed input.
# The native clipboard is a C string: bytes after an embedded NUL are unavailable.
fn ss_code_paste(editor :: SSCodeEditor,text :: String) -> Bool {
    if text.size() == 0 { return false; }
    if !ss_code_string_valid(text) { editor.state[9] = 2; return false; }
    left :: Int64 := int64(vmath.min(float64(editor.state[0]),float64(editor.state[1]))); right :: Int64 := int64(vmath.max(float64(editor.state[0]),float64(editor.state[1])));
    required :: Int64 := text.size(); i :: Int64 := 0;
    while i+1 < text.size() { if text.byte_at(i) == 13 && text.byte_at(i+1) == 10 { required = required-1; i = i+1; } i = i+1; }
    if required >= editor.edit_buffer.size() || editor.source[0]-right+left+required >= editor.source.size() { editor.state[9] = 1; return false; }
    i = 0; n :: Int64 := 0;
    while i < text.size() { byte :: Int64 := text.byte_at(i); if byte == 13 { byte = 10; if i+1 < text.size() && text.byte_at(i+1) == 10 { i = i+1; } } n = n+1; editor.edit_buffer[n] = byte; i = i+1; }
    editor.edit_buffer[0] = n; return ss_code_insert(editor,editor.edit_buffer);
}
fn ss_code_enter(editor :: SSCodeEditor) -> Bool {
    ss_code_index(editor); line :: Int64 := ss_code_line(editor,editor.state[0]); start :: Int64 := editor.lines[line]; end :: Int64 := start;
    while end < editor.state[0] && (editor.source[end+1] == 32 || editor.source[end+1] == 9) { end = end+1; }
    required :: Int64 := 1+end-start;
    left :: Int64 := int64(vmath.min(float64(editor.state[0]),float64(editor.state[1]))); right :: Int64 := int64(vmath.max(float64(editor.state[0]),float64(editor.state[1])));
    if required >= editor.edit_buffer.size() || editor.source[0]-right+left+required >= editor.source.size() { editor.state[9] = 1; return false; }
    editor.edit_buffer[1] = 10; i :: Int64 := start; n :: Int64 := 1;
    while i < end { n = n+1; editor.edit_buffer[n] = editor.source[i+1]; i = i+1; }
    editor.edit_buffer[0] = n; return ss_code_insert(editor,editor.edit_buffer);
}
fn ss_code_find(editor :: SSCodeEditor) -> Bool {
    if editor.query[0] == 0 { return false; }
    start :: Int64 := editor.state[0]; pass :: Int64 := 0;
    while pass < 2 {
        i :: Int64 := start; limit :: Int64 := editor.source[0]-editor.query[0]; if pass == 1 { limit = int64(vmath.min(float64(limit),float64(editor.state[0]-1))); }
        while i <= limit {
            j :: Int64 := 1; while j <= editor.query[0] && editor.source[i+j] == editor.query[j] { j = j+1; }
            if j > editor.query[0] { editor.state[1] = i; editor.state[0] = i+editor.query[0]; editor.state[2] = int64(vmath.max(0.0,float64(ss_code_line(editor,i)-2))); return true; } i = se_edit_next(editor.source,i);
        }
        start = 0; pass = pass+1;
    } return false;
}
fn ss_code_input(editor :: SSCodeEditor,box :: vraylib.Rectangle) -> Bool {
    ss_code_index(editor); revision :: Int64 := editor.state[5]; mouse :: vraylib.Vector2 := vraylib.get_mouse_position();
    if vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) { editor.state[4] = 0; if se_hover(box) { editor.state[4] = 1; editor.state[12] = 1; editor.state[13] = 0; } }
    if !vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_LEFT) { editor.state[12] = 0; }
    shift :: Bool := vraylib.is_key_down(vraylib.KEY_LEFT_SHIFT) || vraylib.is_key_down(vraylib.KEY_RIGHT_SHIFT);
    control :: Bool := vraylib.is_key_down(vraylib.KEY_LEFT_CONTROL) || vraylib.is_key_down(vraylib.KEY_RIGHT_CONTROL);
    if editor.state[12] != 0 {
        line :: Int64 := editor.state[2]+int64(vmath.max(0.0,(mouse.y-box.y-ss_scale(6.0))/ss_line_height()));
        column :: Int64 := editor.state[3]+int64(vmath.max(0.0,(mouse.x-box.x-ss_scale(56.0))/ss_cell_width()+0.5));
        editor.state[0] = ss_code_at(editor,line,column);
        if vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) && !shift { editor.state[1] = editor.state[0]; }
        if mouse.y > box.y+box.height { editor.state[2] = int64(vmath.min(float64(editor.state[17]-1),float64(editor.state[2]+1))); }
        if mouse.y < box.y { editor.state[2] = int64(vmath.max(0.0,float64(editor.state[2]-1))); }
    }
    if se_hover(box) { wheel :: Float64 := vraylib.get_mouse_wheel_move(); if shift { editor.state[3] = int64(vmath.max(0.0,float64(editor.state[3])-wheel*4.0)); } else { editor.state[2] = int64(vmath.clamp(float64(editor.state[2])-wheel*3.0,0.0,float64(editor.state[17]-1))); } }
    if editor.state[4] == 0 { return false; }
    if control && vraylib.is_key_pressed(vraylib.KEY_F) { editor.state[13] = 1; }
    if vraylib.is_key_pressed(vraylib.KEY_ESCAPE) { editor.state[13] = 0; }
    if editor.state[13] != 0 {
        code :: Int64 := vraylib.get_char_pressed(); while code > 0 { if code >= 32 && !control && editor.query[0]+se_utf8_length(code) < editor.query.size() { editor.query[0] = editor.query[0]+se_utf8_put(editor.query,editor.query[0]+1,code); } code = vraylib.get_char_pressed(); }
        if se_key(vraylib.KEY_BACKSPACE) { se_text_backspace(editor.query); }
        if vraylib.is_key_pressed(vraylib.KEY_ENTER) || vraylib.is_key_pressed(vraylib.KEY_F3) { ss_code_find(editor); } return false;
    }
    if control && vraylib.is_key_pressed(vraylib.KEY_A) { editor.state[1] = 0; editor.state[0] = editor.source[0]; }
    if control && vraylib.is_key_pressed(vraylib.KEY_Z) { ss_code_undo(editor,shift); }
    if control && vraylib.is_key_pressed(vraylib.KEY_Y) { ss_code_undo(editor,true); }
    if vraylib.is_key_pressed(vraylib.KEY_F3) { ss_code_find(editor); }
    left :: Int64 := int64(vmath.min(float64(editor.state[0]),float64(editor.state[1]))); right :: Int64 := int64(vmath.max(float64(editor.state[0]),float64(editor.state[1])));
    if control && (vraylib.is_key_pressed(vraylib.KEY_C) || vraylib.is_key_pressed(vraylib.KEY_X)) && right > left { vraylib.set_clipboard_text(se_edit_slice(editor.source,left,right)); if vraylib.is_key_pressed(vraylib.KEY_X) { editor.edit_buffer[0] = 0; ss_code_replace(editor,left,right,editor.edit_buffer); } }
    if control && vraylib.is_key_pressed(vraylib.KEY_V) { ss_code_paste(editor,vraylib.get_clipboard_text()); }
    line :: Int64 := ss_code_line(editor,editor.state[0]); caret :: Int64 := editor.state[0]; moved :: Bool := false;
    if se_key(vraylib.KEY_LEFT) { caret = se_edit_previous(editor.source,caret); if !shift && left != right { caret = left; } moved = true; }
    if se_key(vraylib.KEY_RIGHT) { caret = se_edit_next(editor.source,caret); if !shift && left != right { caret = right; } moved = true; }
    if se_key(vraylib.KEY_HOME) { caret = editor.lines[line]; if control { caret = 0; } moved = true; }
    if se_key(vraylib.KEY_END) { caret = ss_code_end(editor,line); if control { caret = editor.source[0]; } moved = true; }
    if se_key(vraylib.KEY_UP) || se_key(vraylib.KEY_DOWN) || se_key(vraylib.KEY_PAGE_UP) || se_key(vraylib.KEY_PAGE_DOWN) {
        if editor.state[11] < 0 { editor.state[11] = ss_code_column(editor,editor.lines[line],caret); }
        step :: Int64 := 1; if se_key(vraylib.KEY_PAGE_UP) || se_key(vraylib.KEY_PAGE_DOWN) { step = int64(vmath.max(1.0,box.height/ss_line_height()-1.0)); }
        if se_key(vraylib.KEY_UP) || se_key(vraylib.KEY_PAGE_UP) { step = -step; } caret = ss_code_at(editor,line+step,editor.state[11]); moved = true;
    } else { if moved { editor.state[11] = -1; } }
    if moved { editor.state[0] = caret; if !shift { editor.state[1] = caret; } }
    if se_key(vraylib.KEY_BACKSPACE) || se_key(vraylib.KEY_DELETE) {
        left = int64(vmath.min(float64(editor.state[0]),float64(editor.state[1]))); right = int64(vmath.max(float64(editor.state[0]),float64(editor.state[1])));
        if left == right { if se_key(vraylib.KEY_BACKSPACE) { left = se_edit_previous(editor.source,left); } else { right = se_edit_next(editor.source,right); } }
        editor.edit_buffer[0] = 0; ss_code_replace(editor,left,right,editor.edit_buffer);
    }
    if vraylib.is_key_pressed(vraylib.KEY_TAB) { if editor.state[0] != editor.state[1] || shift { ss_code_indent(editor,shift); } else { editor.edit_buffer[0] = 4; i :: Int64 := 1; while i <= 4 { editor.edit_buffer[i] = 32; i = i+1; } ss_code_insert(editor,editor.edit_buffer); } }
    if vraylib.is_key_pressed(vraylib.KEY_ENTER) && !control { ss_code_enter(editor); }
    code :: Int64 := vraylib.get_char_pressed(); while code > 0 { if code >= 32 && code != 127 && !control { editor.edit_buffer[0] = se_utf8_put(editor.edit_buffer,1,code); ss_code_insert(editor,editor.edit_buffer); } code = vraylib.get_char_pressed(); }
    if moved || revision != editor.state[5] {
        line = ss_code_line(editor,editor.state[0]); visible :: Int64 := int64(vmath.max(1.0,box.height/ss_line_height()-1.0));
        if line < editor.state[2] { editor.state[2] = line; } if line >= editor.state[2]+visible { editor.state[2] = line-visible+1; }
        column :: Int64 := ss_code_column(editor,editor.lines[line],editor.state[0]); cells :: Int64 := int64(vmath.max(1.0,(box.width-ss_scale(64.0))/ss_cell_width()));
        if column < editor.state[3] { editor.state[3] = column; } if column >= editor.state[3]+cells { editor.state[3] = column-cells+1; }
    }
    return revision != editor.state[5];
}
fn ss_code_word(ch :: Int64) -> Bool { return (ch >= 65 && ch <= 90) || (ch >= 97 && ch <= 122) || ch == 95 || (ch >= 48 && ch <= 57); }
fn ss_code_keyword(word :: String) -> Bool {
    return word == "uniform" || word == "in" || word == "out" || word == "void" || word == "float" || word == "int" || word == "bool" || word == "vec2" || word == "vec3" || word == "vec4" || word == "mat3" || word == "mat4" || word == "sampler2D" || word == "return" || word == "if" || word == "else" || word == "for" || word == "while" || word == "const" || word == "layout" || word == "discard";
}
fn ss_code_tokenize(editor :: SSCodeEditor,visible :: Int64) -> Bool {
    if editor.state[14] == editor.state[5] && editor.state[15] == editor.state[2] && editor.state[19] == visible { return true; }
    editor.state[16] = 0; editor.state[18] = 0; line :: Int64 := editor.state[2]; last :: Int64 := int64(vmath.min(float64(editor.state[17]),float64(line+visible)));
    while line < last {
        i :: Int64 := editor.lines[line]; end :: Int64 := ss_code_end(editor,line); block :: Int64 := editor.blocks[line];
        while i < end {
            start :: Int64 := i; kind :: Int64 := 0; ch :: Int64 := editor.source[i+1]; next :: Int64 := 0; if i+1 < end { next = editor.source[i+2]; }
            if block != 0 || (ch == 47 && next == 42) {
                kind = 1; if block == 0 { i = i+2; } block = 1;
                while i < end { if i+1 < end && editor.source[i+1] == 42 && editor.source[i+2] == 47 { i = i+2; block = 0; break; } i = i+1; }
            } else {
                if ch == 47 && next == 47 { kind = 1; i = end; }
                else { if ch == 35 { kind = 4; i = end; }
                else { if ch == 34 { kind = 3; i = i+1; while i < end { if editor.source[i+1] == 92 { i = int64(vmath.min(float64(end),float64(i+2))); } else { if editor.source[i+1] == 34 { i = i+1; break; } i = i+1; } } }
                else { if ch >= 48 && ch <= 57 { kind = 3; i = i+1; while i < end && (ss_code_word(editor.source[i+1]) || editor.source[i+1] == 46) { i = i+1; } }
                else { if ss_code_word(ch) { i = i+1; while i < end && ss_code_word(editor.source[i+1]) { i = i+1; } if ss_code_keyword(se_edit_slice(editor.source,start,i)) { kind = 2; } }
                else { i = se_edit_next(editor.source,i); } } } } }
            }
            n :: Int64 := editor.state[16];
            if n > 0 && editor.spans[(n-1)*3+1] == start && editor.spans[(n-1)*3+2] == kind { editor.spans[(n-1)*3+1] = i; }
            else { if n*3+3 >= editor.spans.size() { editor.state[18] = 1; break; }
                editor.spans[n*3] = start; editor.spans[n*3+1] = i; editor.spans[n*3+2] = kind; editor.state[16] = n+1;
            }
        }
        if editor.state[18] != 0 { break; } line = line+1;
    }
    editor.state[14] = editor.state[5]; editor.state[15] = editor.state[2]; editor.state[19] = visible; return true;
}
fn ss_code_tint(kind :: Int64) -> vraylib.Color {
    if kind == 1 { return vraylib.rgba(106,143,117,255); } if kind == 2 { return vraylib.rgba(160,181,245,255); }
    if kind == 3 { return vraylib.rgba(220,183,132,255); } if kind == 4 { return vraylib.rgba(183,145,202,255); } return ss_color(4);
}
fn ss_code_draw(app :: vraylib.App,editor :: SSCodeEditor,box :: vraylib.Rectangle,diagnostics :: Array<Int64>,diagnostic_count :: Int64) -> Bool {
    if box.width <= 0.0 || box.height <= 0.0 { return false; } ss_code_index(editor);
    visible :: Int64 := int64(box.height/ss_line_height())+1; ss_code_tokenize(editor,visible);
    clip :: vraylib.Clip := app.clip(box); se_clip_start(clip,box);
    try {
        vraylib.draw_rectangle_rec(box,ss_color(0)); vraylib.draw_rectangle_rec(se_rect(box.x,box.y,ss_scale(48.0),box.height),ss_color(1));
        line :: Int64 := editor.state[2]; last :: Int64 := int64(vmath.min(float64(editor.state[17]),float64(line+visible))); token :: Int64 := 0;
        left :: Int64 := int64(vmath.min(float64(editor.state[0]),float64(editor.state[1]))); right :: Int64 := int64(vmath.max(float64(editor.state[0]),float64(editor.state[1])));
        while line < last {
            y :: Float64 := box.y+ss_scale(6.0)+float64(line-editor.state[2])*ss_line_height(); start :: Int64 := editor.lines[line]; end :: Int64 := ss_code_end(editor,line);
            if ss_code_line(editor,editor.state[0]) == line { vraylib.draw_rectangle_rec(se_rect(box.x+ss_scale(48.0),y-ss_scale(3.0),box.width-ss_scale(48.0),ss_line_height()),ss_color(1)); }
            ss_mono(string(line+1),box.x+ss_scale(6.0),y,ss_color(5)); d :: Int64 := 0;
            while d < diagnostic_count { if diagnostics[d*4] == line+1 { vraylib.draw_rectangle_rec(se_rect(box.x+ss_scale(43.0),y,ss_scale(3.0),ss_scale(14.0)),ss_color(7)); if ss_theme_input[0] != 0 && se_hover(se_rect(box.x,y,ss_scale(48.0),ss_line_height())) && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) { ss_code_jump(editor,line+1,diagnostics[d*4+1]); } } d = d+1; }
            i :: Int64 := start; column :: Int64 := 0;
            while i < end {
                next :: Int64 := se_edit_next(editor.source,i); width :: Int64 := 1; if editor.source[i+1] == 9 { width = 4-column%4; }
                x :: Float64 := box.x+ss_scale(56.0)+float64(column-editor.state[3])*ss_cell_width();
                if x+float64(width)*ss_cell_width() >= box.x+ss_scale(48.0) && x < box.x+box.width {
                    if i >= left && i < right { vraylib.draw_rectangle_rec(se_rect(vmath.max(x,box.x+ss_scale(48.0)),y-ss_scale(3.0),float64(width)*ss_cell_width(),ss_line_height()),vraylib.rgba(47,71,99,255)); }
                    while token < editor.state[16] && editor.spans[token*3+1] <= i { token = token+1; }
                    kind :: Int64 := 0; if token < editor.state[16] && editor.spans[token*3] <= i { kind = editor.spans[token*3+2]; }
                    if editor.source[i+1] != 9 && x >= box.x+ss_scale(48.0) { ss_mono(se_edit_slice(editor.source,i,next),x,y,ss_code_tint(kind)); }
                }
                i = next; column = column+width;
            }
            if right > end && left <= end { x :: Float64 := box.x+ss_scale(56.0)+float64(column-editor.state[3])*ss_cell_width(); if x >= box.x+ss_scale(48.0) { vraylib.draw_rectangle_rec(se_rect(x,y-ss_scale(3.0),ss_cell_width(),ss_line_height()),vraylib.rgba(47,71,99,255)); } }
            if editor.state[4] != 0 && ss_code_line(editor,editor.state[0]) == line && int64(vraylib.get_time()*2.0)%2 == 0 {
                x :: Float64 := box.x+ss_scale(56.0)+float64(ss_code_column(editor,start,editor.state[0])-editor.state[3])*ss_cell_width();
                if x >= box.x+ss_scale(48.0) { vraylib.draw_rectangle_rec(se_rect(x,y-ss_scale(2.0),ss_scale(1.0),ss_scale(18.0)),ss_color(4)); }
            }
            line = line+1;
        }
        if editor.state[13] != 0 { find_box :: vraylib.Rectangle := se_rect(box.x+ss_scale(52.0),box.y+box.height-ss_scale(32.0),vmath.max(0.0,box.width-ss_scale(60.0)),ss_scale(28.0)); vraylib.draw_rectangle_rec(find_box,ss_color(2)); ss_mono("Find: "+se_text_read(editor.query),find_box.x+ss_scale(6.0),find_box.y+ss_scale(5.0),ss_color(4)); }
    } finally { se_clip_finish(clip); } return true;
}
