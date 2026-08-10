#pragma once

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET SocketHandle;
    #define IS_INVALID_SOCKET(s) ((s) == INVALID_SOCKET)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    typedef int SocketHandle;
    #define IS_INVALID_SOCKET(s) ((s) < 0)
    #define closesocket close
#endif

#include <vector>
#include <string>
#include <iostream>

#include "../../../compiler/ast/ast.h"
#include "../../../compiler/ast/value.h"

void setupVNet(SymbolContainer& env, StringPool& pool);