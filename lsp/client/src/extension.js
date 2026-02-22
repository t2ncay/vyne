const path = require('path');
const { workspace, ExtensionContext, window } = require('vscode');
const {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
  TransportKind
} = require('vscode-languageclient/node');

let client;

function activate(context) {
  // Show activation message
  console.log('Vyne extension is now active!');
  
  // Path to the server module
  const serverModule = context.asAbsolutePath(path.join('lsp', 'server', 'src', 'server.js'));
  
  // Server options
  const serverOptions = {
    run: { 
      module: serverModule, 
      transport: TransportKind.ipc 
    },
    debug: { 
      module: serverModule, 
      transport: TransportKind.ipc,
      options: { execArgv: ['--nolazy', '--inspect=6009'] }
    }
  };
  
  // Client options
  const clientOptions = {
    documentSelector: [
      { scheme: 'file', language: 'vyne' }
    ],
    synchronize: {
      fileEvents: workspace.createFileSystemWatcher('**/*.{vy,vyne,v}')
    }
  };
  
  // Create the client
  client = new LanguageClient(
    'vyne',
    'Vyne Language Server',
    serverOptions,
    clientOptions
  );
  
  // Start the client
  client.start();
  
  console.log('Vyne client started');
}

function deactivate() {
  if (!client) {
    return undefined;
  }
  return client.stop();
}

module.exports = { activate, deactivate };