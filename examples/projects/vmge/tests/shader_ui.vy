use "../src/ui/code_editor.vy";
use native vmem;
fn shader_ui_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
app :: vraylib.App := vraylib.app_options(640,360,"Shader UI checks",0,vraylib.FLAG_WINDOW_HIDDEN);
try {
    se_theme_load(app,vfs.join(vraylib.get_working_directory(),"examples/projects/vmge"));
    font :: vraylib.Font := vraylib.Font(ss_theme_resources[0],app.token);
    shader_ui_check(vmath.abs(font.measure("WWW",14.0).x-font.measure("iii",14.0).x) < 0.01,"source font is truly monospaced");
    bytes :: Array<Int64> := []; bytes.reserve(1024); bytes.resize(1024,0); se_text_write(bytes,"Aé😀Z\n    vec4 color;\n/* comment\ncontinued */\n");
    editor :: SSCodeEditor := ss_code_editor(bytes); editor.state[0] = 7; editor.state[1] = 3;
    se_text_write(editor.edit_buffer,"ç"); ss_code_insert(editor,editor.edit_buffer);
    shader_ui_check(se_edit_slice(bytes,0,6) == "AéçZ" && editor.state[0] == 5,"multibyte selected replacement");
    ss_code_undo(editor,false); shader_ui_check(se_edit_slice(bytes,0,8) == "Aé😀Z","undo restores Unicode");
    ss_code_undo(editor,true); shader_ui_check(se_edit_slice(bytes,0,6) == "AéçZ","redo restores Unicode");
    editor.edit_buffer[0] = 1; editor.edit_buffer[1] = 128;
    shader_ui_check(!ss_code_replace(editor,0,0,editor.edit_buffer) && editor.state[9] == 2,"invalid UTF-8 edit rejected before mutation");
    se_text_write(editor.edit_buffer,"x"); shader_ui_check(!ss_code_replace(editor,2,3,editor.edit_buffer),"partial Unicode selection bounds rejected");
    editor.state[0] = editor.lines[1]; editor.state[1] = editor.lines[2]; ss_code_indent(editor,false);
    shader_ui_check(se_edit_slice(bytes,editor.lines[1],editor.lines[1]+8) == "        ","multiline indent skips trailing unselected line");
    indent_caret :: Int64 := editor.state[0]; indent_anchor :: Int64 := editor.state[1];
    ss_code_undo(editor,false); ss_code_undo(editor,true);
    shader_ui_check(editor.state[0] == indent_caret && editor.state[1] == indent_anchor && indent_caret != indent_anchor,"indent redo restores final selection metadata");
    ss_code_indent(editor,true); shader_ui_check(se_edit_slice(bytes,editor.lines[1],editor.lines[1]+4) == "    ","unindent restores leading whitespace");
    se_text_write(editor.query,"color"); editor.state[0] = bytes[0]; shader_ui_check(ss_code_find(editor),"find wraps");
    shader_ui_check(se_edit_slice(bytes,editor.state[1],editor.state[0]) == "color","find selects exact match");
    ss_code_jump(editor,4,3); shader_ui_check(ss_code_line(editor,editor.state[0]) == 3,"diagnostic jump chooses requested line");
    editor.state[2] = 3; ss_code_tokenize(editor,1); shader_ui_check(editor.spans[2] == 1 && editor.state[16] > 0,"visible lexer resumes block comment from indexed line state");
    editor.state[0] = bytes[0]; editor.state[1] = bytes[0]; i :: Int64 := 0;
    while i < 20 { se_text_write(editor.edit_buffer,"x"); ss_code_insert(editor,editor.edit_buffer); i = i+1; }
    shader_ui_check(editor.state[10] > 0 && editor.state[7] == 16,"bounded history reports oldest-entry eviction");
    editor.edit_buffer[0] = 1023; i = 1; while i <= 1023 { editor.edit_buffer[i] = 120; i = i+1; }
    length :: Int64 := bytes[0]; shader_ui_check(!ss_code_insert(editor,editor.edit_buffer) && bytes[0] == length && editor.state[9] == 1,"capacity overflow is atomic and explicit");
    builder :: Int64 := vcore.sb_create(); vcore.sb_append_char(builder,128); malformed :: String := vcore.sb_build(builder);
    editor.state[0] = 3; editor.state[1] = 0; revision :: Int64 := editor.state[5]; history_count :: Int64 := editor.state[8];
    shader_ui_check(malformed.byte_at(0) == 128 && !ss_code_paste(editor,malformed) && editor.state[9] == 2,"clipboard validates original invalid bytes without Unicode replacement");
    shader_ui_check(bytes[0] == length && editor.state[0] == 3 && editor.state[1] == 0 && editor.state[5] == revision && editor.state[8] == history_count,"rejected clipboard preserves text selection revision and history");
    shader_ui_check(!ss_code_paste(editor,"") && editor.state[0] == 3 && editor.state[1] == 0 && bytes[0] == length,"empty native clipboard is a no-op rather than deleting selection");
    paste_bytes :: Array<Int64> := []; paste_bytes.reserve(32); paste_bytes.resize(32,0); paste_editor :: SSCodeEditor := ss_code_editor(paste_bytes);
    shader_ui_check(ss_code_paste(paste_editor,"é\r\n😀\rZ") && se_text_read(paste_bytes) == "é\n😀\nZ","raw-byte CRLF and lone CR normalize explicitly to LF");
    tiny :: Array<Int64> := []; tiny.reserve(16); tiny.resize(16,0); se_text_write(tiny,"               "); tiny_editor :: SSCodeEditor := ss_code_editor(tiny); tiny_editor.state[0] = 15; tiny_editor.state[1] = 15;
    tiny_editor.edit_buffer[0] = 7; tiny_editor.edit_buffer[1] = 77; tiny_editor.edit_buffer[15] = 88;
    shader_ui_check(!ss_code_enter(tiny_editor) && tiny_editor.state[9] == 1 && tiny[0] == 15 && tiny_editor.state[5] == 1 && tiny_editor.state[0] == 15 && tiny_editor.state[8] == 0,"near-capacity Enter rejects atomically before staging");
    shader_ui_check(tiny_editor.edit_buffer[0] == 7 && tiny_editor.edit_buffer[1] == 77 && tiny_editor.edit_buffer[15] == 88,"overflow Enter leaves staging bytes untouched");
    se_text_write(tiny,"    x"); ss_code_reset_history(tiny_editor); tiny_editor.state[0] = 5; tiny_editor.state[1] = 5;
    shader_ui_check(ss_code_enter(tiny_editor) && se_text_read(tiny) == "    x\n    ","Enter copies indentation after successful preflight");
    large :: Array<Int64> := []; large.reserve(65537); large.resize(65537,0); large[0] = 65536; i = 1; while i <= 65536 { large[i] = 120; i = i+1; }
    allocation_before :: Int64 := vmem.total_allocated(); large_editor :: SSCodeEditor := ss_code_editor(large); allocation_bytes :: Int64 := vmem.total_allocated()-allocation_before;
    shader_ui_check(large_editor.history.size() == 1048576 && large_editor.history_last.size() == 65536 && large_editor.history_lengths.size() == 17,"17 snapshots use 16 exact payload banks plus separate final bank");
    shader_ui_check(allocation_bytes < 16777216,"whole 64 KiB editor allocates less than the old history payload alone");
    out("VMGE_SHADER_UI_HISTORY allocation_bytes=",allocation_bytes," bank_elements=",large_editor.history.size()," final_elements=",large_editor.history_last.size());
    # Exercise circular physical slot 16 and rollover, full payloads, and redo branching.
    large_editor.state[0] = 1; large_editor.state[1] = 0; i = 0;
    while i < 40 { large_editor.edit_buffer[0] = 1; large_editor.edit_buffer[1] = 65+(i%26); shader_ui_check(ss_code_insert(large_editor,large_editor.edit_buffer),"large payload replacement succeeds"); large_editor.state[0] = 1; large_editor.state[1] = 0; i = i+1; }
    shader_ui_check(large_editor.state[10] == 24 && large[0] == 65536,"full payload circular history wraps without growing: evicted="+string(large_editor.state[10])+" length="+string(large[0])+" cursor="+string(large_editor.state[7]));
    i = 0; while i < 16 { shader_ui_check(ss_code_undo(large_editor,false),"all sixteen undo steps retained"); i = i+1; }
    shader_ui_check(large[1] == 65+(23%26) && !ss_code_undo(large_editor,false),"oldest retained payload is exact after rollover");
    i = 0; while i < 16 { shader_ui_check(ss_code_undo(large_editor,true),"all sixteen redo steps retained"); i = i+1; }
    shader_ui_check(large[1] == 65+(39%26),"final spill-bank snapshot restores exact payload");
    ss_code_undo(large_editor,false); large_editor.state[0] = 1; large_editor.state[1] = 0; large_editor.edit_buffer[0] = 1; large_editor.edit_buffer[1] = 90; ss_code_insert(large_editor,large_editor.edit_buffer);
    shader_ui_check(!ss_code_undo(large_editor,true) && large[1] == 90,"branching truncates redo after physical-slot rollover");
    diagnostic :: Array<Int64> := [2,5,0,1]; target :: vraylib.RenderTarget := app.target(640,360); editor.state[2] = 0;
    warm :: Int64 := vmem.commit_total_allocated(); app.limit(4);
    while app.frame() {
        target.begin(); app.clear(vraylib.BLACK); ss_code_draw(app,editor,se_rect(0.0,0.0,640.0,360.0),diagnostic,1); target.end();
        app.begin(); target.present(640.0,360.0); app.end(); shader_ui_check(app.passes() == 0,"balanced code editor clip and target passes");
    }
    shader_ui_check(vmem.commit_total_allocated() == warm,"editor retains no frame strings or headers");
} finally { app.close(); }
shader_ui_check(vraylib.live_handles() == 0,"shared app owns all font and target resources");
out("VMGE_SHADER_UI_OK");
