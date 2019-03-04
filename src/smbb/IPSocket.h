
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

#ifndef SMBB_IPSOCKET_H
#define SMBB_IPSOCKET_H

#include <cstring>

#include "utilities/StaticCast.h"

#include "IPAddress.h"

#define IP_SOCKET_CONCATENATE(A, B) A ## B

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#ifndef IP_SOCKET_NO_QWAVE
#include <qos2.h>
#endif

#ifndef WSAEAGAIN
#define WSAEAGAIN WSAEWOULDBLOCK
#endif

#define IP_SOCKET_ERROR(X) IP_SOCKET_CONCATENATE(WSAE, X)
#define GET_OPTION_TYPE(NORMAL_TYPE, WINDOWS_TYPE) NORMAL_TYPE, WINDOWS_TYPE
#else
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#define IP_SOCKET_ERROR(X) IP_SOCKET_CONCATENATE(E, X)
#define GET_OPTION_TYPE(NORMAL_TYPE, WINDOWS_TYPE) NORMAL_TYPE, NORMAL_TYPE
#endif

namespace smbb {

class IPSocket {
#ifdef _WIN32
	typedef SOCKET Handle;

	static const Handle INVALID_HANDLE = INVALID_SOCKET;
#else
	typedef int Handle;

	static const Handle INVALID_HANDLE = Handle(-1);
#endif

	typedef void (*DefaultFunction)();

	// Loads the specified function by name
	static DefaultFunction FindFunction(const char *name) {
#if defined(_WIN32) || defined(IP_SOCKET_NO_LOADED_FUNCTIONS)
		(void)name;
		return (DefaultFunction)NULL;
#else
		static void *program = dlopen(NULL, RTLD_LAZY);
		union SymbolU { void *pointer; DefaultFunction function; } symbol = { dlsym(program, name) };
		return symbol.function;
#endif
	}

	template <typename F> struct LoadedFunction {
		// The function type
		typedef F Type;

		// Loads the specified function
		static Type Load(const char *name) { return Type(FindFunction(name)); }
	};

public:
#ifdef _WIN32
	typedef char *OptionPointer;
	typedef const char *ConstOptionPointer;

	typedef int OptionLength;
	typedef int DataLength;
	typedef int ResultLength;

	static const int SEND_FLAGS = 0;
	static const int RECV_MULTIPLE_FLAGS = 0;
	static const ResultLength INVALID_RESULT = ResultLength(-1);
	static const int FUNCTION_DOESNT_EXIST = ERROR_INVALID_FUNCTION;

	// Initializes the socket implementation
	static bool Initialize() {
		WSADATA data;

		if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
			return false;

		return true;
	}

	// Cleans up the socket implementation
	static void Finish() { WSACleanup(); }

	// Returns the last error for a given operation on this thread (note that this must be called immediately following a socket call)
	static int LastError() { return WSAGetLastError(); }
#ifndef IP_SOCKET_NO_QWAVE
	// Gets the QoS handle
	static HANDLE GetQoSHandle() {
		static HANDLE qosHandle = INVALID_HANDLE_VALUE;

		if (qosHandle == INVALID_HANDLE_VALUE) {
			QOS_VERSION version = { 1 };
			(void)QOSCreateHandle(&version, &qosHandle);
		}

		return qosHandle;
	}
#endif
#else
	typedef void *OptionPointer;
	typedef const void *ConstOptionPointer;

	typedef socklen_t OptionLength;
	typedef size_t DataLength;
	typedef ssize_t ResultLength;

#if defined(MSG_NOSIGNAL)
	static const int SEND_FLAGS = MSG_NOSIGNAL;
#else
	static const int SEND_FLAGS = 0;
#endif
#if defined(MSG_WAITFORONE)
	static const int RECV_MULTIPLE_FLAGS = MSG_WAITFORONE;
#else
	static const int RECV_MULTIPLE_FLAGS = 0;
#endif
	static const ResultLength INVALID_RESULT = ResultLength(-1);
	static const int FUNCTION_DOESNT_EXIST = ENOSYS;

	// Initializes the socket implementation
	static bool Initialize() {
#if !defined(MSG_NOSIGNAL) && !defined(SO_SIGNOPIPE) && !defined(IP_SOCKET_NO_SIGPIPE_IGNORE)
#warn All pipe signals will be ignored
		signal(SIGPIPE, SIG_IGN);
#endif
		return true;
	}

	// Cleans up the socket implementation
	static void Finish() { }

	// Returns the last error for a given operation on this thread (note that this must be called immediately following a socket call)
	static int LastError() { return errno; }
#endif

	// Stores the results of a single message operation
	class MessageResult {
		ResultLength _result;
		int _error;

		MessageResult(ResultLength result, int error) : _result(result), _error(error) { }

	public:
		static MessageResult UnimplementedFunction() { return MessageResult(INVALID_RESULT, FUNCTION_DOESNT_EXIST); }

		MessageResult(ResultLength result) : _result(result), _error(result < 0 ? LastError() : 0) { }

		// Gets the error from the send
		int GetError() const { return _error; }

		// Gets the number of received bytes
		ResultLength GetBytes() const { return _result; }

		// Checks for errors
		bool HasError() const { return _result < 0; }
		bool HasTemporaryReceiveError() const { return _error == IP_SOCKET_ERROR(INTR) || _error == IP_SOCKET_ERROR(WOULDBLOCK) || _error == IP_SOCKET_ERROR(AGAIN) || _error == IP_SOCKET_ERROR(NOTCONN); }
		bool HasTemporarySendError() const { return _error == IP_SOCKET_ERROR(INTR) || _error == IP_SOCKET_ERROR(WOULDBLOCK) || _error == IP_SOCKET_ERROR(AGAIN) || _error == IP_SOCKET_ERROR(NOTCONN) || _error == IP_SOCKET_ERROR(NOBUFS); }

		bool operator==(const MessageResult &other) { return _result == other._result && _error == other._error; }
		bool operator!=(const MessageResult &other) { return !(*this == other); }
	};

	// A POD class representing a buffer
	class Buffer {
#ifdef _WIN32
		static const size_t MAX_SIZE = size_t(ULONG(-1));

		WSABUF _value;
#else
		static const size_t MAX_SIZE = size_t(-1);

		struct iovec _value;
#endif
	public:
		// Constructs a buffer from the specified data
		static Buffer Make(const void *data = NULL, size_t length = 0) {
			Buffer buffer = { };
#ifdef _WIN32
			buffer._value.buf = reinterpret_cast<CHAR *>(const_cast<void *>(data));
			buffer._value.len = static_cast<ULONG>(length);
#else
			buffer._value.iov_base = const_cast<void *>(data);
			buffer._value.iov_len = length;
#endif
			return buffer;
		}

#ifdef _WIN32
		void *GetData() const { return reinterpret_cast<void *>(_value.buf); }
		size_t GetLength() const { return _value.len; }
#else
		void *GetData() const { return _value.iov_base; }
		size_t GetLength() const { return _value.iov_len; }
#endif
	};

	// A POD class containing a set of buffers that can be sent or received as a single message
	class Message {
#ifdef _WIN32
		struct msghdr {
			LPSOCKADDR msg_name;
			INT        msg_namelen;
			LPWSABUF   lpBuffers;
			DWORD      dwBufferCount;
		} _value;
#else
		struct msghdr _value;
#endif
	public:
		// Creates a message structure around an array of buffers (Note: no error checking is done here, it only provides a cross-platform way to access the data)
		static Message Make(const Buffer buffers[], size_t bufferCount, const IPAddress *address = NULL) {
			Message message = { };

			if (address) {
				message._value.msg_name = address->GetPointer();
				message._value.msg_namelen = address->GetLength();
			}
#ifdef _WIN32
			message._value.lpBuffers = reinterpret_cast<WSABUF *>(const_cast<Buffer *>(buffers));
			message._value.dwBufferCount = static_cast<ULONG>(bufferCount);
#else
			message._value.msg_iov = reinterpret_cast<struct iovec *>(const_cast<Buffer *>(buffers));
			message._value.msg_iovlen = bufferCount;
#endif
			return message;
		}

		// Gets the array of buffers and length from the metadata (Note: no error checking is done here, it only provides a cross-platform way to access the data)
#ifdef _WIN32
		Buffer *GetBuffers() const { return reinterpret_cast<Buffer *>(_value.lpBuffers); }
		size_t GetLength() const { return _value.dwBufferCount; }
#else
		Buffer *GetBuffers() const { return reinterpret_cast<Buffer *>(_value.msg_iov); }
		size_t GetLength() const { return _value.msg_iovlen; }
#endif
		friend class IPSocket;
	};

	// A POD class containing a set of buffers that can be sent or received as a single message as part of a multi-message send or receive
	class MultiMessagePart {
		Message _message;
		ResultLength _result;

	public:
		// Creates a message structure for multiple messages around an array of buffers (Note: no error checking is done here, it only provides a cross-platform way to access the data)
		static MultiMessagePart Make(const Buffer buffers[], size_t bufferCount, const IPAddress *address = NULL) {
			MultiMessagePart part = { };

			part._message = Message::Make(buffers, bufferCount, address);
			return part;
		}

		// Gets the array of buffers and length from the metadata (Note: no error checking is done here, it only provides a cross-platform way to access the data)
		Buffer *GetBuffers() const { return _message.GetBuffers(); }
		size_t GetLength() const { return _message.GetLength(); }
	};

	// Recommended MTU values to minimize fragmentation
	static const size_t IPV4_HEADER_SIZE = 20;
	static const size_t IPV6_HEADER_SIZE = 40;
	static const size_t TCP_HEADER_SIZE = 20;
	static const size_t UDP_HEADER_SIZE = 8;

	static const int ETHERNET_MTU = 1500;
	static const int BALANCED_MTU = 1450;
	static const int IPV6_MINIMUM_MTU = 1280;
	static const int IPV4_MINIMUM_MTU = 576;

	// Support for select()
	enum SelectValue {
		SELECT_NO_CHECK = 0,
		SELECT_CAN_READ = 0x1,
		SELECT_CAN_ACCEPT = SELECT_CAN_READ,
		SELECT_CAN_WRITE = 0x2,
		SELECT_IS_CONNECTED = SELECT_CAN_WRITE,
#ifdef _WIN32
		SELECT_CONNECT_FAILED = 0x4,
#else
		SELECT_CONNECT_FAILED = 0x8,
#endif
		SELECT_CHECK_ALL = SELECT_CAN_READ | SELECT_CAN_WRITE | SELECT_CONNECT_FAILED
	};

	// Select sets can be filled to monitor specific states for sockets.
	// (To reuse a select set, make a copy before calling Wait(), as it modifies the underlying sets.
	//  Some OSes support modifying the FD_SETSIZE during compilation to increase the set size.)
	class SelectSets {
		fd_set _readSet;
		fd_set _writeSet;
#ifdef _WIN32
		fd_set _exceptSet;
#else
		int _max;
		SelectValue _checks;
#endif
	public:
		static bool ShouldPreferSelectOverPoll(size_t maxSockets) {
#ifdef _WIN32
			return maxSockets <= FD_SETSIZE;
#else
			return false;
#endif
		}

		SelectSets()
#ifndef _WIN32
			: _max(), _checks()
#endif
		{
			FD_ZERO(&_readSet);
			FD_ZERO(&_writeSet);
#ifdef _WIN32
			FD_ZERO(&_exceptSet);
#endif
		}

		// Adds the socket to the select set for monitoring, returning true if successful
		// (If this ever returns false use polling instead, as the contents of the select sets cannot be updated to contain the socket.)
		bool AddSocket(const IPSocket &socket, SelectValue toMonitor) {
#ifdef _WIN32
			if ((toMonitor & SELECT_CHECK_ALL) == 0)
				return false;

			if ((toMonitor & SELECT_CAN_READ) != 0) {
				if (_readSet.fd_count >= FD_SETSIZE)
					return false;

				FD_SET(socket._handle, &_readSet);
			}

			if ((toMonitor & SELECT_CAN_WRITE) != 0) {
				if (_writeSet.fd_count >= FD_SETSIZE)
					return false;

				FD_SET(socket._handle, &_writeSet);
			}

			if ((toMonitor & SELECT_CONNECT_FAILED) != 0) {
				if (_exceptSet.fd_count >= FD_SETSIZE)
					return false;

				FD_SET(socket._handle, &_exceptSet);
			}
#else
			if ((toMonitor & SELECT_CHECK_ALL) == 0 || socket._handle >= FD_SETSIZE)
				return false;

			if (socket._handle > _max)
				_max = socket._handle;

			if ((toMonitor & SELECT_CAN_READ) != 0)
				FD_SET(socket._handle, &_readSet);

			if ((toMonitor & (SELECT_CAN_WRITE | SELECT_CONNECT_FAILED)) != 0)
				FD_SET(socket._handle, &_writeSet);

			_checks = static_cast<SelectValue>(_checks | (toMonitor & SELECT_CHECK_ALL));
#endif
			return true;
		}

		// Removes the socket from the select set for monitoring
		void RemoveSocket(const IPSocket &socket, SelectValue toMonitor) {
#ifdef _WIN32
			if ((toMonitor & SELECT_CAN_READ) != 0)
				FD_CLR(socket._handle, &_readSet);

			if ((toMonitor & SELECT_CAN_WRITE) != 0)
				FD_CLR(socket._handle, &_writeSet);

			if ((toMonitor & SELECT_CONNECT_FAILED) != 0)
				FD_CLR(socket._handle, &_exceptSet);
#else
			if (socket._handle >= FD_SETSIZE)
				return;

			if ((toMonitor & SELECT_CAN_READ) != 0)
				FD_CLR(socket._handle, &_readSet);

			if ((toMonitor & (SELECT_CAN_WRITE | SELECT_CONNECT_FAILED)) != 0)
				FD_CLR(socket._handle, &_writeSet);
#endif
		}

		// Waits for either a timeout or one of the sockets specified to be able to receive data / accept a connection or become connected / available to send
		// (This function normalizes the behaviors between Windows / POSIX to be more like the POSIX standards)
		int Wait(unsigned long timeoutUs = static_cast<unsigned long>(-1)) {
#ifdef _WIN32
			if (_readSet.fd_count == 0 && _writeSet.fd_count == 0 && _exceptSet.fd_count == 0) {
				Sleep(timeoutUs / 1000);
				return 0;
			}
#endif
			// Populate the timeout
			timeval timeout;

			timeout.tv_sec = timeoutUs / 1000000;
			timeout.tv_usec = timeoutUs % 1000000;
#ifdef _WIN32
			return select(0, _readSet.fd_count != 0 ? &_readSet : NULL, _writeSet.fd_count != 0 ? &_writeSet : NULL, _exceptSet.fd_count != 0 ? &_exceptSet : NULL, &timeout);
#else
			return select(_max + 1, (_checks & SELECT_CAN_READ) != 0 ? &_readSet : NULL, (_checks & (SELECT_CAN_WRITE | SELECT_CONNECT_FAILED)) != 0 ? &_writeSet : NULL, NULL, &timeout);
#endif
		}

		// Tests the socket for the specified values after a call to Wait(), returning the set values
		SelectValue TestSocket(const IPSocket &socket, SelectValue checkResult) {
#ifdef _WIN32
			if ((checkResult & SELECT_CONNECT_FAILED) != 0 && FD_ISSET(socket._handle, &_exceptSet) == 0)
				checkResult = static_cast<SelectValue>(checkResult & ~SELECT_CONNECT_FAILED);
#else
			if ((checkResult & SELECT_CONNECT_FAILED) != 0) {
				if (checkResult & (FD_ISSET(socket._handle, &_writeSet) == 0))
					checkResult = static_cast<SelectValue>(checkResult & ~(SELECT_CONNECT_FAILED | SELECT_CAN_WRITE));
				else
					checkResult = static_cast<SelectValue>(socket.GetError() == 0 ? ~SELECT_CONNECT_FAILED : ~SELECT_IS_CONNECTED);
			}
			else
#endif
				if ((checkResult & SELECT_CAN_WRITE) != 0 && FD_ISSET(socket._handle, &_writeSet) == 0)
					checkResult = static_cast<SelectValue>(checkResult & ~SELECT_CAN_WRITE);

			if ((checkResult & SELECT_CAN_READ) != 0 && FD_ISSET(socket._handle, &_readSet) == 0)
				checkResult = static_cast<SelectValue>(checkResult & ~SELECT_CAN_READ);

			return static_cast<SelectValue>(checkResult & SELECT_CHECK_ALL);
		}
	};

	// Support for poll()
	enum PollValue {
		POLL_NO_CHECK = 0,
		POLL_CAN_READ = POLLIN,
		POLL_CAN_ACCEPT = POLLIN,
		POLL_CAN_WRITE = POLLOUT,
		POLL_IS_CONNECTED = POLLOUT,
		POLL_DISCONNECTING = POLLHUP, // The socket is disconnecting, continue reading until a successful read returns 0 for connected sockets (no guarantee this will be returned for a socket)
		POLL_ERROR = POLLERR, // Use this for detecting hard disconnects or other error values not including a failed connection
		POLL_CHECK_ALL = POLL_CAN_READ | POLL_CAN_WRITE | POLL_DISCONNECTING
	};

	class PollItem {
		struct pollfd _item;

		// Gets the disabled version of the handle
		Handle GetDisabledHandle() const { return _item.fd | (Handle(1) << (sizeof(Handle) * 8 - 1)); }

		// Gets the enabled version of the handle
		Handle GetEnabledHandle() const { return _item.fd & ~(Handle(1) << (sizeof(Handle) * 8 - 1)); }

	public:
		// Gets a poll item for the specified socket
		// (The value to monitor can be empty, in which case status will be returned, but no events will be monitored.)
		static PollItem Make(const IPSocket &socket, PollValue monitor = POLL_NO_CHECK) {
			PollItem item = { };

			item._item.fd = socket._handle;
			item._item.events = static_cast<short>(monitor & POLL_CHECK_ALL);
			return item;
		}

		// Checks if the poll item is enabled
		bool IsEnabled() const { return (_item.fd & (Handle(1) << (sizeof(Handle) * 8 - 1))) == 0; }

		// Disables the poll item
		void Disable() { _item.fd = GetDisabledHandle(); }

		// Enables the poll item
		void Enable() { _item.fd = GetEnabledHandle(); }

		// Gets the value to monitor
		PollValue GetMonitor() const { return static_cast<PollValue>(_item.events); }

		// Sets the value to monitor
		void SetMonitor(PollValue value) { _item.events = static_cast<short>(value & POLL_CHECK_ALL); }

		// Checks if the specified item indicates a failed connection attempt after a poll
		bool HasFailedConnectionResult() const {
#ifdef _WIN32
			if (!IsEnabled() || _item.revents != 0)
				return false;

			timeval timeout = {};
			fd_set exceptSet;

			FD_ZERO(&exceptSet);
			FD_SET(_item.fd, &exceptSet);

			return select(0, NULL, NULL, &exceptSet, &timeout) == 1;
#else
			return (_item.revents & POLL_ERROR) != 0;
#endif
		}

		// Checks if the specified result is set after a poll
		bool HasResult(PollValue value) const { return (_item.revents & value) != 0; }

		// Gets the results after a poll
		PollValue GetResult() const { return static_cast<PollValue>(_item.revents); }

		// Gets the socket associated a poll item
		IPSocket GetSocket() const { return IPSocket(GetEnabledHandle()); }
	};

	// Waits for one of the sockets in the specified set to receive or become connected / available to send or for a timeout
	// (Use a sane timeout value, as some OSes do not return early when an outgoing connection attempt fails.)
	static int Poll(PollItem set[], unsigned int numItems, int timeoutMs = 0) {
#ifdef _WIN32
		unsigned int i;

		// Find at least one enabled item
		for (i = 0; i < numItems && !set[i].IsEnabled(); i++);

		if (i >= numItems) {
			Sleep(timeoutMs);
			return 0;
		}

		return WSAPoll(reinterpret_cast<struct pollfd *>(set), numItems, timeoutMs);
#else
		return poll(reinterpret_cast<struct pollfd *>(set), numItems, timeoutMs);
#endif
	}

private:
	Handle _handle;

	IPSocket(Handle handle) : _handle(handle) { }

	// Gets the specified option on the socket, returning true if successful
	template <typename T, typename OptionT> bool GetOptionInternal(int level, int name, T &value) const {
		OptionT valueOfOption;
		OptionLength valueLen = static_cast<OptionLength>(sizeof(OptionT));

		if (getsockopt(_handle, level, name, reinterpret_cast<OptionPointer>(&valueOfOption), &valueLen) != 0)
			return false;

		value = static_cast<T>(valueOfOption);
		return true;
	}

	// Gets the specified option on the socket, returning true if successful
	template <typename T, typename OptionT> T GetOptionInternalDefault(int level, int name, T defaultValue) const {
		(void)GetOptionInternal<T, OptionT>(level, name, defaultValue);
		return defaultValue;
	}

	// Sets the specified option on the socket and returns true if successful, otherwise false
	template <typename T, typename OptionT> bool SetOptionInternal(int level, int name, const T &value) {
		OptionT realValue = static_cast<OptionT>(value);
		return setsockopt(_handle, level, name, reinterpret_cast<ConstOptionPointer>(&realValue), static_cast<OptionLength>(sizeof(OptionT))) == 0;
	}

	// Performs a multicast address operation (subscribe, unsubscribe) on a socket
	bool ManageMulticastAddress(bool subscribe, const IPAddress &multicastAddress, const IPAddress &localAddress) {
		if (multicastAddress.GetFamily() == IPV4) {
			ip_mreq request;

			request.imr_multiaddr = multicastAddress._ipv4.sin_addr;
			request.imr_interface = localAddress._ipv4.sin_addr;

			return setsockopt(_handle, IPPROTO_IP, subscribe ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, reinterpret_cast<ConstOptionPointer>(&request), static_cast<OptionLength>(sizeof(request))) == 0;
		}
		else if (multicastAddress.GetFamily() == IPV6) {
			ipv6_mreq requestV6;

			requestV6.ipv6mr_multiaddr = multicastAddress._ipv6.sin6_addr;
			requestV6.ipv6mr_interface = static_cast<unsigned int>(localAddress.GetInterfaceIndex());

			return setsockopt(_handle, IPPROTO_IPV6, subscribe ? IPV6_ADD_MEMBERSHIP : IPV6_DROP_MEMBERSHIP, reinterpret_cast<ConstOptionPointer>(&requestV6), static_cast<OptionLength>(sizeof(requestV6))) == 0;
		}

		return false;
	}

	// Performs a multicast source address operation (subscribe, unsubscribe) on a socket
	bool ManageMulticastSourceAddress(bool subscribe, const IPAddress &multicastAddress, const IPAddress &sourceAddress, const IPAddress &localAddress) {
		if (multicastAddress.GetFamily() == IPV4) {
			ip_mreq_source request;

			request.imr_multiaddr = multicastAddress._ipv4.sin_addr;
			request.imr_interface = localAddress._ipv4.sin_addr;
			request.imr_sourceaddr = sourceAddress._ipv4.sin_addr;

			return setsockopt(_handle, IPPROTO_IP, subscribe ? IP_ADD_SOURCE_MEMBERSHIP : IP_DROP_SOURCE_MEMBERSHIP, reinterpret_cast<ConstOptionPointer>(&request), static_cast<OptionLength>(sizeof(request))) == 0;
		}

		return false;
	}

public:
	enum OpenAttempt {
		OPEN_ONLY,
		OPEN_AND_BIND,
		OPEN_BIND_AND_LISTEN,
		OPEN_AND_CONNECT
	};

	// Allow chaining certain function calls together
	template <typename T> class Chainable {
		IPSocket *_socket;
		T _result;

	public:
		Chainable(IPSocket *socket, T result) : _socket(socket), _result(result) { }

		// Implicit conversion to the actual result
		operator T() const { return _result; }

		// Allow chaining using ->
		IPSocket *operator->() const { return _socket; }
	};

	IPSocket() : _handle(INVALID_HANDLE) { }
	IPSocket(IPAddressFamily family, IPProtocol protocol) : _handle(INVALID_HANDLE) { (void)Open(family, protocol); }
	IPSocket(const IPAddress &address, IPProtocol protocol, OpenAttempt openAttempt = OPEN_ONLY) : _handle(INVALID_HANDLE) { (void)Open(address, protocol, openAttempt); }

	virtual ~IPSocket() { }

	bool operator==(const IPSocket &other) const { return _handle == other._handle; }
	bool operator!=(const IPSocket &other) const { return _handle != other._handle; }
	bool operator>=(const IPSocket &other) const { return _handle >= other._handle; }
	bool operator>(const IPSocket &other) const { return _handle > other._handle; }
	bool operator<=(const IPSocket &other) const { return _handle <= other._handle; }
	bool operator<(const IPSocket &other) const { return _handle < other._handle; }

	// Opens the socket (if it is not already open) for the given address and protocol
	Chainable<bool> Open(IPAddressFamily family, IPProtocol protocol) {
		if (IsValid())
			return Chainable<bool>(this, false);

		_handle = socket(static_cast<int>(family), static_cast<int>(protocol), 0);
		return Chainable<bool>(this, IsValid());
	}

	// Opens the socket for the given address and protocol
	Chainable<bool> Open(const IPAddress &address, IPProtocol protocol, OpenAttempt openAttempt = OPEN_ONLY) {
		if (Open(address.GetFamily(), protocol)) {
#if defined(SO_SIGNOPIPE)
			(void)SetOptionInternal<int>(SOL_SOCKET, SO_NOSIGPIPE, 1);
#endif
#if defined(SO_EXCLUSIVEADDRUSE)
			(void)SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(SOL_SOCKET, SO_EXCLUSIVEADDRUSE, 1);
#endif
			if (openAttempt == OPEN_AND_BIND)
				return Chainable<bool>(this, Bind(address));
			else if (openAttempt == OPEN_BIND_AND_LISTEN)
				return Chainable<bool>(this, Bind(address) && Listen());
			else if (openAttempt == OPEN_AND_CONNECT)
				return Chainable<bool>(this, Connect(address) != CONNECT_FAILED);

			return Chainable<bool>(this, true);
		}

		return Chainable<bool>(this, false);
	}

	// Returns true if the socket is valid
	bool IsValid() const { return _handle != INVALID_HANDLE; }

	// Gets the local address that is bound to the socket
	IPAddress GetAddress() const {
		IPAddress address;
		IPAddressLength addressLength = sizeof(address);

		(void)getsockname(_handle, address.GetPointer(), &addressLength);
		return address;
	}

	// Gets the address of the peer connected to the socket
	IPAddress GetPeerAddress() const {
		IPAddress address;
		IPAddressLength addressLength = sizeof(address);

		(void)getpeername(_handle, address.GetPointer(), &addressLength);
		return address;
	}

	// Closes a socket (if using a TCP socket, be sure to call CloseTCPSend() before this call
	Chainable<bool> Close() {
#ifdef _WIN32
		(void)closesocket(_handle);
#else
		(void)close(_handle);
#endif
		_handle = INVALID_HANDLE;
		return Chainable<bool>(this, true);
	}

	// Closes only the sending side of a TCP socket (useful if data still needs to be received but none needs to be sent)
	Chainable<bool> CloseTCPSend() {
#ifdef _WIN32
		(void)shutdown(_handle, SD_SEND);
#else
		(void)shutdown(_handle, SHUT_WR);
#endif
		return Chainable<bool>(this, true);
	}

	// Disables checksum calculations where applicable (useful if the data has built-in validation that will be performed at the receiving end)
	Chainable<bool> SetDisableChecksum(bool disable = true) {
#ifdef UDP_NOCHECKSUM
		return Chainable<bool>(this, SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_UDP, UDP_NOCHECKSUM, disable ? 1 : 0));
#else
		return Chainable<bool>(this, false);
#endif
	}

	// Gets the error associated with the socket
	int GetError() const {
		return GetOptionInternalDefault<GET_OPTION_TYPE(int, DWORD)>(SOL_SOCKET, SO_ERROR, 0);
	}

	// Checks the socket option to allow immediate send on a socket
	bool GetImmediateSend() const {
		return GetOptionInternalDefault<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_TCP, TCP_NODELAY, 0) != 0;
	}

	// Sets the socket option to allow immediate send on a socket
	Chainable<bool> SetImmediateSend(bool enable = true) {
		return Chainable<bool>(this, SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_TCP, TCP_NODELAY, enable ? 1 : 0));
	}

	// Checks the socket option to send keep alive packets on a socket
	bool GetKeepAlive() const {
		return GetOptionInternalDefault<GET_OPTION_TYPE(int, DWORD)>(SOL_SOCKET, SO_KEEPALIVE, 0) != 0;
	}

	// Sets the socket option to to send keep alive packets on a socket
	Chainable<bool> SetKeepAlive(bool enable = true) {
		return Chainable<bool>(this, SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(SOL_SOCKET, SO_KEEPALIVE, enable ? 1 : 0));
	}

	// Gets the linger time socket option in milliseconds
	unsigned long GetLingerTime() const {
		linger value = { };

		(void)GetOptionInternal<GET_OPTION_TYPE(linger, linger)>(SOL_SOCKET, SO_LINGER, value);
		return value.l_onoff ? value.l_linger * 1000UL : 0UL;
	}

	// Sets the linger time socket option in milliseconds
	Chainable<bool> SetLingerTime(unsigned long milliseconds = 0) {
		linger value = { };

		value.l_onoff = 1;
		value.l_linger = StaticCast(value.l_linger, milliseconds / 1000UL + (milliseconds % 1000UL ? 1UL : 0UL));

		return Chainable<bool>(this, SetOptionInternal<GET_OPTION_TYPE(linger, linger)>(SOL_SOCKET, SO_LINGER, value));
	}

	// Gets an estimated path MTU for outgoing packets (TCP only)
	int GetMTU() const {
		int mtu = 0;
#ifdef IP_MTU
		if (GetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IP, IP_MTU, mtu))
			return mtu;
#endif
#ifdef IPV6_MTU
		(void)GetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IPV6, IPV6_MTU, mtu);
#endif
		return mtu;
	}

	enum MTUDiscover {
#ifdef IP_MTU_DISCOVER
#ifdef _WIN32
		MTU_DISCOVER_DEFAULT = IP_PMTUDISC_NOT_SET,
#else
		MTU_DISCOVER_DEFAULT = IP_PMTUDISC_DONT,
#endif
		MTU_DISCOVER_NONE = IP_PMTUDISC_DONT,
#if defined(_WIN32) || defined(IP_PMTUDISC_PROBE)
		MTU_DISCOVER_PROBE = IP_PMTUDISC_PROBE,
#endif
		MTU_DISCOVER_FULL = IP_PMTUDISC_DO
#else
		MTU_DISCOVER_DEFAULT,
		MTU_DISCOVER_NONE,
		MTU_DISCOVER_FULL
#endif
	};

	// Gets the MTU discover option (TCP only)
	MTUDiscover GetMTUDiscover() const {
		int mtuDiscover = MTU_DISCOVER_DEFAULT;
#ifdef IP_MTU_DISCOVER
		if (GetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IP, IP_MTU_DISCOVER, mtuDiscover))
			return static_cast<MTUDiscover>(mtuDiscover);
#endif
#ifdef IPV6_MTU_DISCOVER
		(void)GetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IPV6, IPV6_MTU_DISCOVER, mtuDiscover);
#endif
		return static_cast<MTUDiscover>(mtuDiscover);
	}

	// Sets the MTU discover option (TCP only)
	Chainable<bool> SetMTUDiscover(MTUDiscover value) {
		(void)value;
#ifdef IP_MTU_DISCOVER
		if (SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IP, IP_MTU_DISCOVER, value))
			return Chainable<bool>(this, true);
#endif
#ifdef IPV6_MTU_DISCOVER
		if (SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IPV6, IPV6_MTU_DISCOVER, value))
			return Chainable<bool>(this, true);
#endif
		return Chainable<bool>(this, false);
	}

	// Puts the socket in non-blocking mode (or blocking mode if nonblocking is false)
	Chainable<bool> SetNonblocking(bool nonblocking = true) {
#ifdef _WIN32
		u_long mode = nonblocking ? 1 : 0;
		return Chainable<bool>(this, ioctlsocket(_handle, FIONBIO, &mode) == 0);
#else
		int flags = fcntl(_handle, F_GETFL);
		return Chainable<bool>(this, flags != -1 && fcntl(_handle, F_SETFL, nonblocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) != -1);
#endif
	}

	// Gets the receive buffer size for the socket
	int GetReceiveBufferSize() const {
		return GetOptionInternalDefault<GET_OPTION_TYPE(int, DWORD)>(SOL_SOCKET, SO_RCVBUF, 0);
	}

	// Sets the receive buffer size for the socket
	Chainable<bool> SetReceiveBufferSize(int size) {
		return Chainable<bool>(this, SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(SOL_SOCKET, SO_RCVBUF, size));
	}

	// Checks the socket option to reuse the address (address reuse means wildcard and specific binds are allowed on same port, port reuse is allowed on multicast subscriptions) and ignore the time wait state on a socket
	bool GetReuseAddress() const {
		return GetOptionInternalDefault<GET_OPTION_TYPE(int, DWORD)>(SOL_SOCKET, SO_REUSEADDR, 0) != 0;
	}

	// Sets the socket option to reuse the address (address reuse means wildcard and specific binds are allowed on same port, port reuse is allowed on multicast subscriptions) and ignore the time wait state on a socket
	Chainable<bool> SetReuseAddress(bool enable = true) {
#if defined(SO_EXCLUSIVEADDRUSE)
		(void)SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(SOL_SOCKET, SO_EXCLUSIVEADDRUSE, enable ? 0 : 1);
#endif
		return Chainable<bool>(this, SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(SOL_SOCKET, SO_REUSEADDR, enable ? 1 : 0));
	}

	// Checks the socket option to reuse the port on a socket (does not work on all OSes)
	bool GetReusePort() const {
#ifdef SO_REUSEPORT
		return GetOptionInternalDefault<GET_OPTION_TYPE(int, DWORD)>(SOL_SOCKET, SO_REUSEPORT, 0) != 0;
#else
		return false;
#endif
	}

	// Sets the socket option to reuse the port on a socket (does not work on all OSes)
	Chainable<bool> SetReusePort(bool enable = true) {
#ifdef SO_REUSEPORT
		return Chainable<bool>(this, SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(SOL_SOCKET, SO_REUSEPORT, enable ? 1 : 0));
#else
		(void)enable;
		return Chainable<bool>(this, false);
#endif
	}

	// Gets the send buffer size for the socket
	int GetSendBufferSize() const {
		return GetOptionInternalDefault<GET_OPTION_TYPE(int, DWORD)>(SOL_SOCKET, SO_SNDBUF, 0);
	}

	// Sets the send buffer size for the socket
	Chainable<bool> SetSendBufferSize(int size) {
		return Chainable<bool>(this, SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(SOL_SOCKET, SO_SNDBUF, size));
	}

	enum TypeOfService {
		TOS_NONE = 0,
		TOS_MIN_COST = 0x02,
		TOS_RELIABILITY = 0x04,
		TOS_THROUGHPUT = 0x08,
		TOS_LOW_DELAY = 0x10,

		// Precedence
		TOS_PRECEDENCE_NONE = 0x00,
		TOS_PRECEDENCE_VERY_LOW = 0x20,
		TOS_PRECEDENCE_LOW = 0x40,
		TOS_PRECEDENCE_MEDIUM = 0x60,
		TOS_PRECEDENCE_HIGH = 0x80,
		TOS_PRECEDENCE_VERY_HIGH = 0xA0,
		TOS_PRECEDENCE_ADMIN_HIGH = 0xC0,
		TOS_PRECEDENCE_ADMIN_MAX = 0xE0,

		TOS_PRECEDENCE_BEST_EFFORT = 0x00,
		TOS_PRECEDENCE_PRIORITY = 0x20,
		TOS_PRECEDENCE_IMMEDIATE = 0x40,
		TOS_PRECEDENCE_FLASH = 0x60,
		TOS_PRECEDENCE_FLASH_OVERRIDE = 0x80,
		TOS_PRECEDENCE_CRITICAL = 0xA0,
		TOS_PRECEDENCE_INTERNETWORK_CONTROL = 0xC0,
		TOS_PRECEDENCE_NETWORK_CONTROL = 0xE0,

		TOS_MASK = 0xFE
	};

	// Gets the type of service option (may not work on all OSes)
	TypeOfService GetTOS() const {
		int tos = 0;
#ifdef IP_TOS
		if (GetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IP, IP_TOS, tos))
			return static_cast<TypeOfService>(tos);
#endif
#ifdef IPV6_TCLASS
		(void)GetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IPV6, IPV6_TCLASS, tos);
#endif
		return static_cast<TypeOfService>(tos);
	}

	// Sets the type of service option
	Chainable<bool> SetTOS(TypeOfService value) {
#ifdef _WIN32
		DSCPData data;
		return SetDSCP(static_cast<DSCP>(value >> 2), data);
#else
#ifdef IP_TOS
		if (SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IP, IP_TOS, value & TOS_MASK))
			return Chainable<bool>(this, true);
#endif
#ifdef IPV6_TCLASS
		if (SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IPV6, IPV6_TCLASS, value & TOS_MASK))
			return Chainable<bool>(this, true);
#endif
		(void)value;
		return Chainable<bool>(this, false);
#endif
	}

	// Differentiated Services Code Point (DSCP)
	enum DSCP {
		DSCP_BEST_EFFORT = 0x00,
		DSCP_DEFAULT_FORWARDING = 0x00,
		DSCP_EXPEDITED_FORWARDING = 0x2E,

		// Assured Forwarding
		DSCP_AF_CLASS_1 = 0x08,
		DSCP_AF_CLASS_2 = 0x10,
		DSCP_AF_CLASS_3 = 0x18,
		DSCP_AF_CLASS_4 = 0x20,

		DSCP_AF_LOW_DROP = 0x02,
		DSCP_AF_MEDIUM_DROP = 0x04,
		DSCP_AF_HIGH_DROP = 0x06,

		// Class Selector
		DSCP_CS_0 = 0x00,
		DSCP_CS_1 = 0x08,
		DSCP_CS_2 = 0x10,
		DSCP_CS_3 = 0x18,
		DSCP_CS_4 = 0x20,
		DSCP_CS_5 = 0x28,
		DSCP_CS_6 = 0x30,
		DSCP_CS_7 = 0x38,

		// Service Classes
		DSCP_SERVICE_TELEPHONY = DSCP_EXPEDITED_FORWARDING,
		DSCP_SERVICE_SIGNALING = DSCP_CS_5,
		DSCP_SERVICE_VIDEO_CONFERENCING = DSCP_AF_CLASS_4, // Use with desired drop rate
		DSCP_SERVICE_INTERACTIVE = DSCP_CS_4,
		DSCP_SERVICE_STREAMING = DSCP_AF_CLASS_3, // Use with desired drop rate
		DSCP_SERVICE_BROADCAST = DSCP_CS_3,
		DSCP_SERVICE_TRANSACTION_DATA = DSCP_AF_CLASS_2, // Use with desired drop rate
		DSCP_SERVICE_OAM = DSCP_CS_2,
		DSCP_SERVICE_BIG_DATA = DSCP_AF_CLASS_1, // Use with desired drop rate
		DSCP_SERVICE_LOW_PRIORITY = DSCP_CS_1,

		DSCP_MASK = 0x3F
	};

	// DSCP Helper Data
	struct DSCPData {
#ifdef _WIN32
		IPAddress _address;
		QOS_FLOWID _flow;

		DSCPData(const IPAddress &connectAddress = IPAddress()) : _address(connectAddress), _flow() { }
#else
		DSCPData(const IPAddress &connectAddress = IPAddress()) { (void)connectAddress; }
#endif
	};

	// Gets the DSCP value on the socket (the data argument must be the one used to set the DSCP value on some OSes)
	DSCP GetDSCP(const DSCPData &data = DSCPData()) const {
#ifdef _WIN32
#ifndef IP_SOCKET_NO_QWAVE
		QOS_PACKET_PRIORITY priority;
		ULONG prioritySize = sizeof(priority);

		if (QOSQueryFlow(GetQoSHandle(), data._flow, QOSQueryPacketPriority, &prioritySize, &priority, 0, NULL)) {
			return static_cast<DSCP>(priority.ConformantDSCPValue);
		}
#endif
		return DSCP_DEFAULT_FORWARDING;
#else
		(void)data;
		return static_cast<DSCP>(GetTOS() >> 2);
#endif
	}

	// Sets the DSCP value on the socket (some OSes require the socket to be connected before setting the DSCP value)
	Chainable<bool> SetDSCP(DSCP value, DSCPData &data) {
#ifdef _WIN32
#ifndef IP_SOCKET_NO_QWAVE
		QOS_TRAFFIC_TYPE trafficType = value >= DSCP_CS_7 ? QOSTrafficTypeControl :
			value >= DSCP_SERVICE_TELEPHONY ? QOSTrafficTypeVoice :
			value >= DSCP_SERVICE_STREAMING ? QOSTrafficTypeAudioVideo :
			value > DSCP_SERVICE_LOW_PRIORITY ? QOSTrafficTypeExcellentEffort :
			value == DSCP_SERVICE_LOW_PRIORITY ? QOSTrafficTypeBackground :
			QOSTrafficTypeBestEffort;

		if (data._flow != 0 || QOSAddSocketToFlow(GetQoSHandle(), _handle, data._address.GetFamily() == FAMILY_UNSPECIFIED ? NULL : data._address.GetPointer(), trafficType, QOS_NON_ADAPTIVE_FLOW, &data._flow)) {
			DWORD dscp = value & DSCP_MASK;

			if (QOSSetFlow(GetQoSHandle(), data._flow, QOSSetOutgoingDSCPValue, sizeof(dscp), &dscp, 0, NULL))
				return Chainable<bool>(this, true);
		}
#endif
		return Chainable<bool>(this, false);
#else
		(void)data;
		return SetTOS(static_cast<TypeOfService>(value << 2));
#endif
	}

	// Sets the DSCP value on the socket (some OSes require the socket to be bound or connected before setting the DSCP value)
	Chainable<bool> SetDSCP(DSCP value) {
		DSCPData data;
		return SetDSCP(value, data);
	}

	// Gets the specified option on the socket, returning the size of the option returned if successful, otherwise 0
	template <typename T> OptionLength GetOption(int level, int name, T &value) const {
		OptionLength valueLen = static_cast<OptionLength>(sizeof(T));
		return getsockopt(_handle, level, name, reinterpret_cast<OptionPointer>(&value), &valueLen) == 0 ? valueLen : 0;
	}

	// Sets the specified option on the socket and returns true if successful, otherwise false
	template <typename T> bool SetOption(int level, int name, const T &value) {
		return setsockopt(_handle, level, name, reinterpret_cast<ConstOptionPointer>(&value), static_cast<OptionLength>(sizeof(T))) == 0;
	}

	// Bind the socket
	Chainable<bool> Bind(const IPAddress &address) {
		return Chainable<bool>(this, bind(_handle, address.GetPointer(), address.GetLength()) == 0);
	}

	// Listen on the socket after binding
	Chainable<bool> Listen(int maxPendingConnections = SOMAXCONN) {
		return Chainable<bool>(this, listen(_handle, maxPendingConnections) == 0);
	}

	// Accept new connections after listening on the socket
	IPSocket Accept(IPAddress *newAddress = NULL) {
		IPAddress address;
		IPAddressLength length = sizeof(address);
		IPSocket socket = IPSocket(accept(_handle, address.GetPointer(), &length));

		if (socket.IsValid() && newAddress)
			*newAddress = address;

		return socket;
	}

	enum ConnectResult {
		CONNECT_FAILED = 0,
		CONNECT_SUCCESS,
		CONNECT_PENDING
	};

	// Connect the socket
	Chainable<ConnectResult> Connect(const IPAddress &address) {
		if (connect(_handle, address.GetPointer(), address.GetLength()) == 0)
			return Chainable<ConnectResult>(this, CONNECT_SUCCESS);

		int error = LastError();

		if (error == IP_SOCKET_ERROR(ISCONN))
			return Chainable<ConnectResult>(this, CONNECT_SUCCESS);
#ifdef _WIN32
		else if (error == IP_SOCKET_ERROR(ALREADY) || error == IP_SOCKET_ERROR(WOULDBLOCK) || error == IP_SOCKET_ERROR(INPROGRESS))
			return Chainable<ConnectResult>(this, CONNECT_PENDING);
#else
		else if (error == IP_SOCKET_ERROR(ALREADY) || error == IP_SOCKET_ERROR(INPROGRESS))
			return Chainable<ConnectResult>(this, CONNECT_PENDING);
#endif
		return Chainable<ConnectResult>(this, CONNECT_FAILED);
	}

	// Receive data from the socket
	MessageResult Receive(void *data, DataLength length) {
		return MessageResult(recv(_handle, reinterpret_cast<char *>(data), length, 0));
	}

	// Receive data from the socket
	MessageResult Receive(void *data, DataLength length, IPAddress &from) {
		IPAddressLength addressLength = sizeof(from);
		return MessageResult(recvfrom(_handle, reinterpret_cast<char *>(data), length, 0, from.GetPointer(), &addressLength));
	}

	// Receive data from the socket
	MessageResult Receive(const Message &message) {
#ifdef _WIN32
		DWORD bytesReceived;
		DWORD flags = 0;
		int result = WSARecvFrom(_handle, message._value.lpBuffers, message._value.dwBufferCount, &bytesReceived, &flags, message._value.msg_name, const_cast<LPINT>(&message._value.msg_namelen), NULL, NULL);

		return MessageResult(result == 0 ? static_cast<int>(bytesReceived) : -1);
#else
		return MessageResult(recvmsg(_handle, const_cast<struct msghdr *>(&message._value), 0));
#endif
	}

private:
	// Helpers to get the recvmmsg function
	typedef LoadedFunction<int(*)(Handle, MultiMessagePart[], size_t, int, struct timespec *)> RecvMMsgFunction;

	static RecvMMsgFunction::Type GetRecvMMsg() {
#ifdef _WIN32
		return RecvMMsgFunction::Type();
#else
		static RecvMMsgFunction::Type recvMMsg = RecvMMsgFunction::Load("recvmmsg");
		return recvMMsg;
#endif
	}

public:
	// Checks if the receive multiple function is available
	static bool HasReceiveMultiple() { return GetRecvMMsg() != RecvMMsgFunction::Type(); }

	// Receive data from the socket
	MessageResult ReceiveMultiple(MultiMessagePart parts[], size_t length) {
		RecvMMsgFunction::Type recvMMsg = GetRecvMMsg();

		if (recvMMsg)
			return MessageResult(recvMMsg(_handle, parts, length, RECV_MULTIPLE_FLAGS, NULL));

		return MessageResult::UnimplementedFunction();
	}

	// Send data on the socket
	MessageResult Send(const void *data, DataLength length) {
		return MessageResult(send(_handle, reinterpret_cast<const char *>(data), length, SEND_FLAGS));
	}

	// Send data to the specific address on the socket
	MessageResult Send(const IPAddress &address, const void *data, DataLength length) {
		return MessageResult(sendto(_handle, reinterpret_cast<const char *>(data), length, SEND_FLAGS, address.GetPointer(), address.GetLength()));
	}

	// Send data to the specific address on the socket
	MessageResult Send(const Message &message) {
#ifdef _WIN32
		DWORD bytesSent;
		int result = WSASendTo(_handle, message._value.lpBuffers, message._value.dwBufferCount, &bytesSent, SEND_FLAGS, message._value.msg_name, message._value.msg_namelen, NULL, NULL);

		return MessageResult(result == 0 ? static_cast<int>(bytesSent) : -1);
#else
		return MessageResult(sendmsg(_handle, const_cast<struct msghdr *>(&message._value), SEND_FLAGS));
#endif
	}

private:
	// Helpers to get the sendmmsg function
	typedef LoadedFunction<int(*)(Handle, MultiMessagePart[], size_t, int)> SendMMsgFunction;

	static SendMMsgFunction::Type GetSendMMsg() {
#ifdef _WIN32
		return SendMMsgFunction::Type();
#else
		static SendMMsgFunction::Type sendMMsg = SendMMsgFunction::Load("sendmmsg");
		return sendMMsg;
#endif
	}

public:
	// Checks if the send multiple function is available
	static bool HasSendMultiple() { return GetSendMMsg() != SendMMsgFunction::Type(); }

	// Send data on the socket
	MessageResult SendMultiple(MultiMessagePart parts[], size_t length) {
		SendMMsgFunction::Type sendMMsg = GetSendMMsg();

		if (sendMMsg)
			return MessageResult(sendMMsg(_handle, parts, length, SEND_FLAGS));

		return MessageResult::UnimplementedFunction();
	}

	// Gets the number of hops value for outgoing multicast packets
	int GetMulticastHops() const {
		int hops = -1;

		if (GetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IP, IP_MULTICAST_TTL, hops))
			return hops;

		return GetOptionInternalDefault<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IPV6, IPV6_MULTICAST_HOPS, hops);
	}

	// Sets the TTL value of the socket for outgoing unicast packets
	Chainable<bool> SetMulticastHops(int value) {
		if (SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IP, IP_MULTICAST_TTL, value))
			return Chainable<bool>(this, true);

		return Chainable<bool>(this, SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IPV6, IPV6_MULTICAST_HOPS, value));
	}

	// Gets the number of hops value for outgoing multicast packets
	bool GetMulticastLoopback() const {
		int loopback = 0;

		if (GetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IP, IP_MULTICAST_LOOP, loopback))
			return loopback != 0;

		return GetOptionInternalDefault<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IPV6, IPV6_MULTICAST_LOOP, loopback) != 0;
	}

	// Sets the TTL value of the socket for outgoing unicast packets
	Chainable<bool> SetMulticastLoopback(bool value = true) {
		if (SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IP, IP_MULTICAST_LOOP, value ? 1 : 0))
			return Chainable<bool>(this, true);

		return Chainable<bool>(this, SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IPV6, IPV6_MULTICAST_LOOP, value ? 1 : 0));
	}

	// Sets the interface used to send multicast packets
	Chainable<bool> SetMulticastSendInterface(const IPAddress &localAddress = IPAddress()) {
		if (SetOptionInternal<GET_OPTION_TYPE(in_addr, in_addr)>(IPPROTO_IP, IP_MULTICAST_IF, localAddress._ipv4.sin_addr))
			return Chainable<bool>(this, true);

		return Chainable<bool>(this, SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IPV6, IPV6_MULTICAST_IF, localAddress.GetInterfaceIndex()));
	}

	// Subscribes to the specified multicast address using the local address
	Chainable<bool> SubscribeToMulticastAddress(const IPAddress &multicastAddress, const IPAddress &localAddress = IPAddress()) {
		return Chainable<bool>(this, ManageMulticastAddress(true, multicastAddress, localAddress));
	}

	// Unsubscribes from the specified multicast address using the local address
	Chainable<bool> UnsubscribeFromMulticastAddress(const IPAddress &multicastAddress, const IPAddress &localAddress = IPAddress()) {
		return Chainable<bool>(this, ManageMulticastAddress(false, multicastAddress, localAddress));
	}

	// Subscribes to the specified multicast address using the local address
	Chainable<bool> SubscribeToMulticastSourceAddress(const IPAddress &multicastAddress, const IPAddress &sourceAddress, const IPAddress &localAddress = IPAddress()) {
		return Chainable<bool>(this, ManageMulticastSourceAddress(true, multicastAddress, sourceAddress, localAddress));
	}

	// Unsubscribes from the specified multicast address using the local address
	Chainable<bool> UnsubscribeFromMulticastSourceAddress(const IPAddress &multicastAddress, const IPAddress &sourceAddress, const IPAddress &localAddress = IPAddress()) {
		return Chainable<bool>(this, ManageMulticastSourceAddress(false, multicastAddress, sourceAddress, localAddress));
	}
};

// Wraps an IP socket in a safe container that cannot be copied and is automatically closed when it falls out of scope
class IPSocketGuard : public IPSocket {
	// Disable public copying / assignment
	IPSocketGuard(const IPSocketGuard &) { }
	IPSocketGuard &operator=(const IPSocketGuard &other) { IPSocket::operator=(other); return *this; }

public:
	IPSocketGuard(const IPSocket &socket = IPSocket()) : IPSocket(socket) { }
	IPSocketGuard(IPAddressFamily family, IPProtocol protocol) : IPSocket(family, protocol) { }
	IPSocketGuard(const IPAddress &address, IPProtocol protocol, OpenAttempt openAttempt = OPEN_ONLY) : IPSocket(address, protocol, openAttempt) { }

	virtual ~IPSocketGuard() {
		if (IsValid())
			(void)Close();
	}

	// Swaps the contents of this guard with another, and returns the guard
	IPSocketGuard &Swap(IPSocketGuard &other) {
		IPSocket copy = other;
		other = *this;
		return *this = copy;
	}
};

inline IPSocket::SelectValue operator|(IPSocket::SelectValue x, IPSocket::SelectValue y) { return static_cast<IPSocket::SelectValue>(static_cast<int>(x) | y); }
inline IPSocket::SelectValue operator&(IPSocket::SelectValue x, IPSocket::SelectValue y) { return static_cast<IPSocket::SelectValue>(static_cast<int>(x) & y); }

inline IPSocket::PollValue operator|(IPSocket::PollValue x, IPSocket::PollValue y) { return static_cast<IPSocket::PollValue>(static_cast<int>(x) | y); }
inline IPSocket::PollValue operator&(IPSocket::PollValue x, IPSocket::PollValue y) { return static_cast<IPSocket::PollValue>(static_cast<int>(x) & y); }

inline IPSocket::TypeOfService operator|(IPSocket::TypeOfService x, IPSocket::TypeOfService y) { return static_cast<IPSocket::TypeOfService>(static_cast<int>(x) | y); }
inline IPSocket::TypeOfService operator&(IPSocket::TypeOfService x, IPSocket::TypeOfService y) { return static_cast<IPSocket::TypeOfService>(static_cast<int>(x) & y); }

inline IPSocket::DSCP operator|(IPSocket::DSCP x, IPSocket::DSCP y) { return static_cast<IPSocket::DSCP>(static_cast<int>(x) | y); }
inline IPSocket::DSCP operator&(IPSocket::DSCP x, IPSocket::DSCP y) { return static_cast<IPSocket::DSCP>(static_cast<int>(x) & y); }

}

#undef GET_OPTION_TYPE

#endif
