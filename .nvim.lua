-- .nvim.lua
vim.keymap.set("n", "<leader>F5", function()
	vim.cmd("split | terminal sh ./run.sh")
end, {
	desc = "Run local run.sh",
	silent = true,
})
