local M = {}

function M.setup(opts)
    opts = opts or {}
    
    local cmd = { "vyne_bin.exe", "--lsp" }
    
    if opts.binary_path then
        cmd = { opts.binary_path, "--lsp" }
    end

    local client_id = vim.lsp.start_client({
        name = "vyne-lsp",
        cmd = cmd,
        root_dir = vim.fs.dirname(vim.fs.find({'.git', 'package.json'}, { upward = true })[1]),
        settings = {},
        on_attach = function(client, bufnr)
            local bufopts = { noremap=true, silent=true, buffer=bufnr }
            vim.keymap.set('n', 'gd', vim.lsp.buf.definition, bufopts)
            vim.keymap.set('n', 'K', vim.lsp.buf.hover, bufopts)
            vim.keymap.set('n', '<leader>rn', vim.lsp.buf.rename, bufopts)
            
            print("Vyne LSP attached to buffer")
        end,
    })

    if not client_id then
        vim.notify("Error: Vyne LSP failed to start. Ensure vyne_bin.exe is in your PATH.", vim.log.levels.ERROR)
        return
    end

    vim.api.nvim_create_autocmd("FileType", {
        pattern = "vyne",
        callback = function()
            vim.lsp.buf_attach_client(0, client_id)
        end,
    })
end

return M