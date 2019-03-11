
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

#ifndef _WIN32
#include <dlfcn.h>
#endif

#ifdef _WIN32
HANDLE smbb::IPSocket::_qosHandle = INVALID_HANDLE_VALUE;
#endif

smbb::IPSocket::RecvMMsgFunction::Type smbb::IPSocket::_recvMMsg = smbb::IPSocket::RecvMMsgFunction::Type();
smbb::IPSocket::SendMMsgFunction::Type smbb::IPSocket::_sendMMsg = smbb::IPSocket::SendMMsgFunction::Type();

// Loads the specified function by name
smbb::IPSocket::DefaultFunction smbb::IPSocket::FindFunction(const char *name) {
#if defined(_WIN32) || defined(IP_SOCKET_NO_LOADED_FUNCTIONS)
	(void)name;
	return (DefaultFunction)NULL;
#else
	static void *program = dlopen(NULL, RTLD_LAZY);
	union SymbolU { void *pointer; DefaultFunction function; } symbol = { dlsym(program, name) };
	return symbol.function;
#endif
}

// Initializes the socket implementation
bool smbb::IPSocket::Initialize() {
#ifdef _WIN32
	WSADATA data;

	if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
		return false;

#ifndef IP_SOCKET_NO_QWAVE
	if (_qosHandle == INVALID_HANDLE_VALUE) {
		QOS_VERSION version = { 1 };
		(void)QOSCreateHandle(&version, &_qosHandle);
	}
#endif
#else
	if (!_recvMMsg)
		_recvMMsg = RecvMMsgFunction::Load("recvmmsg");

	if (!_sendMMsg)
		_sendMMsg = SendMMsgFunction::Load("sendmmsg");
#endif
	return true;
}

// Cleans up the socket implementation
void smbb::IPSocket::Finish() {
#ifdef _WIN32
	WSACleanup();
#endif
}
