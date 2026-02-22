const {
  createConnection,
  TextDocuments,
  ProposedFeatures,
  DiagnosticSeverity
} = require('vscode-languageserver/node');
const { TextDocument } = require('vscode-languageserver-textdocument');
const { spawn } = require('child_process');
const path = require('path');

// Create LSP connection
const connection = createConnection(ProposedFeatures.all);
const documents = new TextDocuments(TextDocument);

// Path to your compiled C++ backend
// After building, your executable will be in ../bin/vyne
const backendPath = path.join(__dirname, '../../bin/vyne');
const backend = spawn(backendPath, ['--lsp']);

// Handle messages from backend
let requestId = 0;
const pendingRequests = new Map();

// Read responses from C++ backend
backend.stdout.on('data', (data) => {
  const lines = data.toString().split('\n').filter(l => l.trim());
  
  for (const line of lines) {
    try {
      const response = JSON.parse(line);
      
      // Check if it's a notification (like diagnostics)
      if (response.method) {
        if (response.method === 'textDocument/publishDiagnostics') {
          connection.sendDiagnostics(response.params);
        }
        continue;
      }
      
      // It's a response to a request
      const { id, result, error } = response;
      const pending = pendingRequests.get(id);
      
      if (pending) {
        if (error) {
          pending.reject(new Error(error.message));
        } else {
          pending.resolve(result);
        }
        pendingRequests.delete(id);
      }
    } catch (e) {
      console.error('Failed to parse backend response:', e);
    }
  }
});

// Log backend errors
backend.stderr.on('data', (data) => {
  console.error('Backend error:', data.toString());
});

// Send request to backend
function sendRequest(method, params) {
  return new Promise((resolve, reject) => {
    const id = ++requestId;
    pendingRequests.set(id, { resolve, reject });
    
    const request = {
      jsonrpc: '2.0',
      id,
      method,
      params
    };
    
    backend.stdin.write(JSON.stringify(request) + '\n');
  });
}

// Send notification to backend (no response expected)
function sendNotification(method, params) {
  const notification = {
    jsonrpc: '2.0',
    method,
    params
  };
  backend.stdin.write(JSON.stringify(notification) + '\n');
}

// Server capabilities
connection.onInitialize(async (params) => {
  const result = await sendRequest('initialize', params);
  return result;
});

connection.onInitialized(() => {
  console.log('Vyne LSP server initialized');
});

connection.onShutdown(async () => {
  await sendRequest('shutdown', {});
  backend.kill();
});

// Document opened
documents.onDidOpen(async (event) => {
  sendNotification('textDocument/didOpen', {
    textDocument: {
      uri: event.document.uri,
      languageId: event.document.languageId,
      version: event.document.version,
      text: event.document.getText()
    }
  });
});

// Document changed
documents.onDidChangeContent(async (event) => {
  sendNotification('textDocument/didChange', {
    textDocument: {
      uri: event.document.uri,
      version: event.document.version
    },
    contentChanges: [{
      text: event.document.getText()
    }]
  });
});

// Document closed
documents.onDidClose(async (event) => {
  sendNotification('textDocument/didClose', {
    textDocument: {
      uri: event.document.uri
    }
  });
});

// Completion provider
connection.onCompletion(async (params) => {
  return await sendRequest('textDocument/completion', params);
});

// Go to definition
connection.onDefinition(async (params) => {
  return await sendRequest('textDocument/definition', params);
});

// Hover information
connection.onHover(async (params) => {
  return await sendRequest('textDocument/hover', params);
});

// Find references
connection.onReferences(async (params) => {
  return await sendRequest('textDocument/references', params);
});

// Rename symbol
connection.onRenameRequest(async (params) => {
  return await sendRequest('textDocument/rename', params);
});

// Document symbols (outline)
connection.onDocumentSymbol(async (params) => {
  return await sendRequest('textDocument/documentSymbol', params);
});

// Start listening
documents.listen(connection);
connection.listen();