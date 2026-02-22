const path = require("path");
const fs = require("fs");
const { workspace, window } = require("vscode");
const { LanguageClient, TransportKind } = require("vscode-languageclient/node");

let client;

function activate(context) {
  console.log("Vyne extension is now active!");

  /**
   * PATH LOGIC:
   * Your file is at: lsp/client/src/extension.js
   * Target is at:    lsp/backend/build/vyne_bin.exe
   * * Inside the packaged VSIX, 'context.asAbsolutePath' starts from the root.
   * So we need to point to: 'backend', 'build', 'vyne_bin.exe'
   */

  // Check if we are on Windows for the .exe suffix
  const isWindows = process.platform === "win32";
  const binaryName = isWindows ? "vyne_bin.exe" : "vyne_bin";

  const serverExecutable = context.asAbsolutePath(
    path.join("backend", "build", binaryName),
  );

  // Safety check: Show an error in the IDE if the binary wasn't packaged correctly
  if (!fs.existsSync(serverExecutable)) {
    window.showErrorMessage(
      `Vyne LSP Error: Binary not found at ${serverExecutable}. Check your build folder!`,
    );
    return;
  }

  const serverOptions = {
    run: {
      command: serverExecutable,
      transport: TransportKind.stdio,
    },
    debug: {
      command: serverExecutable,
      transport: TransportKind.stdio,
    },
  };

  const clientOptions = {
    documentSelector: [{ scheme: "file", language: "vyne" }],
    synchronize: {
      fileEvents: workspace.createFileSystemWatcher("**/*.{vy,vyne,v}"),
    },
  };

  // Create and start the client
  client = new LanguageClient(
    "vyne",
    "Vyne Language Server",
    serverOptions,
    clientOptions,
  );

  client
    .start()
    .then(() => console.log("Vyne client started successfully"))
    .catch((err) => console.error("Vyne client failed to start:", err));

  console.log(
    "Vyne client attempting to start using binary at:",
    serverExecutable,
  );
}

function deactivate() {
  if (!client) {
    return undefined;
  }
  return client.stop();
}

module.exports = {
  activate,
  deactivate,
};
