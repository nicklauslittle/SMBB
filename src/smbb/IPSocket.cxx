
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

#ifdef _WIN32
HANDLE smbb::IPSocket::_qosHandle = INVALID_HANDLE_VALUE;
#else
#ifndef SMBB_NO_DYNAMIC_LOADING
#include <dlfcn.h>
#endif
#endif

#ifndef SMBB_NO_SOCKET_MSG
smbb::IPSocket::RecvMMsgFunction::Type smbb::IPSocket::_recvMMsg = smbb::IPSocket::RecvMMsgFunction::Type();
smbb::IPSocket::SendMMsgFunction::Type smbb::IPSocket::_sendMMsg = smbb::IPSocket::SendMMsgFunction::Type();
#endif

// Loads the specified function by name
smbb::IPSocket::DefaultFunction smbb::IPSocket::FindFunction(const char *name) {
#if defined(_WIN32) || defined(SMBB_NO_DYNAMIC_LOADING)
	(void)name;
	return DefaultFunction();
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

#ifndef SMBB_NO_QWAVE
	if (_qosHandle == INVALID_HANDLE_VALUE) {
		QOS_VERSION version = { 1 };
		(void)QOSCreateHandle(&version, &_qosHandle);
	}
#endif
#else
#ifndef SMBB_NO_SOCKET_MSG
	if (!_recvMMsg)
		_recvMMsg = RecvMMsgFunction::Load("recvmmsg");

	if (!_sendMMsg)
		_sendMMsg = SendMMsgFunction::Load("sendmmsg");
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
