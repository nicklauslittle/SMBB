
/**
Copyright (c) 2019 Nick Little

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "IPSocket.h"

#if !defined(_WIN32) && !defined(SMBB_NO_DYNAMIC_LOADING)
#include <dlfcn.h>
#endif

// Loads the specified function by name
smbb::IPSocket::DefaultFunction smbb::IPSocket::FindFunction(const char *name) {
#if !defined(_WIN32) && !defined(SMBB_NO_DYNAMIC_LOADING)
	static void *program = dlopen(NULL, RTLD_LAZY);
	union SymbolU { void *pointer; DefaultFunction function; } symbol = { dlsym(program, name) };
	return symbol.function;
#else
	(void)name;
	return DefaultFunction();
#endif
}

// Initializes the socket implementation
bool smbb::IPSocket::Initialize() {
#ifdef _WIN32
	WSADATA data;

	if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
		return false;

#ifndef SMBB_NO_QWAVE
	if (GetQoSHandle() == INVALID_HANDLE_VALUE) {
		QOS_VERSION version = { 1 };
		(void)QOSCreateHandle(&version, &GetQoSHandle());
	}
#endif
#else
#ifndef SMBB_NO_SOCKET_MSG
	if (!GetRecvMMsg())
		GetRecvMMsg() = RecvMMsgFunction::Load("recvmmsg");

	if (!GetSendMMsg())
		GetSendMMsg() = SendMMsgFunction::Load("sendmmsg");
#endif
#endif
	return true;
}

// Cleans up the socket implementation
void smbb::IPSocket::Finish() {
#ifdef _WIN32
	WSACleanup();
#endif
}
