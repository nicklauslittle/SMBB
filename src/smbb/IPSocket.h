
/**
Copyright (c) 2019-2020 Nick Little

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

#define IP_SOCKET_CONCATENATE(A, B) A ## B

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#if !defined(SMBB_NO_QWAVE)
#include <qos2.h>
#endif

#ifndef WSAEAGAIN
#define WSAEAGAIN WSAEWOULDBLOCK
#endif

#define IP_SOCKET_ERROR(X) IP_SOCKET_CONCATENATE(WSAE, X)
#define GET_OPTION_TYPE(NORMAL_TYPE, WINDOWS_TYPE) NORMAL_TYPE, WINDOWS_TYPE
#else
#include <unistd.h>
#include <errno.h>
#if !defined(SMBB_NO_FCNTL)
#include <fcntl.h>
#else
#include <sys/ioctl.h>
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#if !defined(SMBB_NO_POLL)
#include <poll.h>
#endif

#define IP_SOCKET_ERROR(X) IP_SOCKET_CONCATENATE(E, X)
#define GET_OPTION_TYPE(NORMAL_TYPE, WINDOWS_TYPE) NORMAL_TYPE, NORMAL_TYPE
#endif

#include "utilities/Inline.h"
#include "utilities/StaticCast.h"

#include "IPAddress.h"

struct timespec;

namespace smbb {

class IPSocket {
	typedef void (*DefaultFunction)();

	// Loads the specified function by name
	static SMBB_INLINE DefaultFunction FindFunction(const char *name);

	template <typename F> struct LoadedFunction {
		// The function type
		typedef F Type;

		// Loads the specified function
		static Type Load(const char *name) { return Type(FindFunction(name)); }
	};

public:
#if defined(_WIN32)
	typedef SOCKET Handle;
	typedef char *OptionPointer;
	typedef const char *ConstOptionPointer;

	typedef int OptionLength;
	typedef int DataLength;
	typedef int ResultLength;

	static const Handle INVALID_HANDLE = INVALID_SOCKET;

	// Gets the QoS handle
	static HANDLE &GetQoSHandle() {
		static HANDLE qosHandle = INVALID_HANDLE_VALUE;
		return qosHandle;
	}

	// Returns the last error for a given operation on this thread (note that this must be called immediately following a socket call)
	static int LastError() { return WSAGetLastError(); }
#else
	typedef int Handle;
	typedef void *OptionPointer;
	typedef const void *ConstOptionPointer;

	typedef socklen_t OptionLength;
	typedef size_t DataLength;
	typedef ssize_t ResultLength;

	static const Handle INVALID_HANDLE = Handle(-1);

	// Returns the last error for a given operation on this thread (note that this must be called immediately following a socket call)
	static int LastError() { return errno; }
#endif

#if defined(MSG_NOSIGNAL)
	static const int SEND_FLAGS = MSG_NOSIGNAL;
#else
	static const int SEND_FLAGS = 0;
#endif

	// Initializes the socket implementation
	static SMBB_INLINE bool Initialize();

	// Cleans up the socket implementation
	static SMBB_INLINE void Finish();

	// Stores the results of a single message operation
	class MessageResult {
		ResultLength _result;
		int _error;

	public:
		MessageResult(ResultLength result, int error) : _result(result), _error(error) { }
		MessageResult(ResultLength result) : _result(result), _error(result < 0 ? LastError() : 0) { }

		// Gets the error from the send or receive (may be set even if the result is positive; will always be zero if there is no error)
		int GetError() const { return _error; }

		// Gets the number of sent or received bytes or messages
		ResultLength GetResult() const { return _result; }

		// Checks for errors
		bool Failed() const { return _result < 0; }
		bool HasSizeError() const { return _error == IP_SOCKET_ERROR(MSGSIZE); }
#if defined(_WIN32)
		bool HasTemporaryReceiveError() const { return _error == IP_SOCKET_ERROR(WOULDBLOCK); }
		bool HasTemporarySendError() const { return _error == IP_SOCKET_ERROR(WOULDBLOCK) || _error == IP_SOCKET_ERROR(NOBUFS); }
#else
		bool HasTemporaryReceiveError() const { return _error == IP_SOCKET_ERROR(INTR) || _error == IP_SOCKET_ERROR(WOULDBLOCK) || _error == IP_SOCKET_ERROR(AGAIN); }
		bool HasTemporarySendError() const { return _error == IP_SOCKET_ERROR(INTR) || _error == IP_SOCKET_ERROR(WOULDBLOCK) || _error == IP_SOCKET_ERROR(AGAIN) || _error == IP_SOCKET_ERROR(NOBUFS); }
#endif
		// Compare results
		bool operator==(const MessageResult &other) { return _result == other._result && _error == other._error; }
		bool operator!=(const MessageResult &other) { return !(*this == other); }
	};

#if !defined(SMBB_NO_SOCKET_MSG)
	// A standard layout class representing a buffer
	class Buffer {
#if defined(_WIN32)
		static const size_t MAX_SIZE = size_t(ULONG(-1));

		WSABUF _value;
#else
		static const size_t MAX_SIZE = size_t(-1);

		struct iovec _value;
#endif
	public:
		// Constructs a buffer from the specified data
		Buffer(const void *data = NULL, size_t length = 0) : _value() {
#if defined(_WIN32)
			_value.buf = reinterpret_cast<CHAR *>(const_cast<void *>(data));
			_value.len = static_cast<ULONG>(length);
#else
			_value.iov_base = StaticCast(_value.iov_base, const_cast<void *>(data));
			_value.iov_len = length;
#endif
		}

#if defined(_WIN32)
		void *GetData() const { return reinterpret_cast<void *>(_value.buf); }
		size_t GetLength() const { return _value.len; }
#else
		void *GetData() const { return _value.iov_base; }
		size_t GetLength() const { return _value.iov_len; }
#endif
	};

	// A standard layout class containing a set of buffers that can be sent or received as a single message
	class Message {
#if defined(_WIN32)
		struct msghdr {
			LPSOCKADDR msg_name;
			INT        msg_namelen;
			LPWSABUF   lpBuffers;
			DWORD      dwBufferCount;
		};
#endif
		mutable msghdr _value;

	public:
		// Creates a message structure around an array of buffers (Note: no error checking is done here, it only provides a cross-platform way to access the data)
		Message(const Buffer buffers[], size_t bufferCount, const IPAddress *address = NULL) : _value() {
			if (address)
				_value.msg_name = address->GetPointer();
#if defined(_WIN32)
			_value.lpBuffers = reinterpret_cast<WSABUF *>(const_cast<Buffer *>(buffers));
			_value.dwBufferCount = static_cast<ULONG>(bufferCount);
#else
			_value.msg_iov = reinterpret_cast<struct iovec *>(const_cast<Buffer *>(buffers));
			_value.msg_iovlen = bufferCount;
#endif
		}

		// Gets the array of buffers and length from the metadata (Note: no error checking is done here, it only provides a cross-platform way to access the data)
#if defined(_WIN32)
		Buffer *GetBuffers() const { return reinterpret_cast<Buffer *>(_value.lpBuffers); }
		size_t GetLength() const { return _value.dwBufferCount; }
#else
		Buffer *GetBuffers() const { return reinterpret_cast<Buffer *>(_value.msg_iov); }
		size_t GetLength() const { return _value.msg_iovlen; }
#endif
		friend class IPSocket;
	};

	// A standard layout class containing a set of buffers that can be sent or received as a single message as part of a multi-message send or receive
	class MultiMessagePart {
		Message _message;
		ResultLength _result;

	public:
		// Creates a message structure for multiple messages around an array of buffers (Note: no error checking is done here, it only provides a cross-platform way to access the data)
		MultiMessagePart(const Buffer buffers[], size_t bufferCount, const IPAddress *address = NULL) : _message(buffers, bufferCount, address), _result() { }

		// Gets the array of buffers and length from the metadata (Note: no error checking is done here, it only provides a cross-platform way to access the data)
		Buffer *GetBuffers() const { return _message.GetBuffers(); }
		size_t GetLength() const { return _message.GetLength(); }

		friend class IPSocket;
	};
#endif

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
#if defined(_WIN32)
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
#if defined(_WIN32)
		fd_set _exceptSet;
#else
		int _max;
		SelectValue _checks;
#endif
	public:
		SelectSets()
#if !defined(_WIN32)
			: _max(), _checks()
#endif
		{
			FD_ZERO(&_readSet);
			FD_ZERO(&_writeSet);
#if defined(_WIN32)
			FD_ZERO(&_exceptSet);
#endif
		}

		// Adds the socket to the select set for monitoring, returning true if successful
		// (If this ever returns false use polling instead, as the contents of the select sets cannot be updated to contain the socket.)
		bool AddSocket(const IPSocket &socket, SelectValue toMonitor) {
#if defined(_WIN32)
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
#if defined(_WIN32)
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
#if defined(_WIN32)
			if (_readSet.fd_count == 0 && _writeSet.fd_count == 0 && _exceptSet.fd_count == 0) {
				Sleep(timeoutUs / 1000);
				return 0;
			}
#endif
			// Populate the timeout
			timeval timeout;

			timeout.tv_sec = timeoutUs / 1000000;
			timeout.tv_usec = timeoutUs % 1000000;
#if defined(_WIN32)
			return select(0, _readSet.fd_count != 0 ? &_readSet : NULL, _writeSet.fd_count != 0 ? &_writeSet : NULL, _exceptSet.fd_count != 0 ? &_exceptSet : NULL, &timeout);
#else
			return select(_max + 1, (_checks & SELECT_CAN_READ) != 0 ? &_readSet : NULL, (_checks & (SELECT_CAN_WRITE | SELECT_CONNECT_FAILED)) != 0 ? &_writeSet : NULL, NULL, &timeout);
#endif
		}

		// Tests the socket for the specified values after a call to Wait(), returning the set values
		SelectValue TestSocket(const IPSocket &socket, SelectValue checkResult) {
#if defined(_WIN32)
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

#if !defined(SMBB_NO_POLL)
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
			PollItem item = PollItem();

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
#if defined(_WIN32)
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

		// Gets the socket associated with a poll item
		IPSocket GetSocket() const { return IPSocket(GetEnabledHandle()); }
	};

	// Waits for one of the sockets in the specified set to receive or become connected / available to send or for a timeout
	// (Use a sane timeout value, as some OSes do not return early when an outgoing connection attempt fails.)
	static int Poll(PollItem set[], unsigned int numItems, int timeoutMs = 0) {
#if defined(_WIN32)
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
#endif // SMBB_NO_POLL

private:
#if !defined(SMBB_NO_SOCKET_MSG)
	// Helpers to get the recvmmsg and sendmmsg functions
	typedef LoadedFunction<int(*)(Handle, MultiMessagePart[], size_t, int, timespec *)> RecvMMsgFunction;
	typedef LoadedFunction<int(*)(Handle, MultiMessagePart[], size_t, int)> SendMMsgFunction;

	// Gets the recvmmsg function
	static RecvMMsgFunction::Type &GetRecvMMsg() {
		static RecvMMsgFunction::Type recvMMsg = RecvMMsgFunction::Type();
		return recvMMsg;
	}

	// Gets the recvmmsg function
	static SendMMsgFunction::Type &GetSendMMsg() {
		static SendMMsgFunction::Type sendMMsg = SendMMsgFunction::Type();
		return sendMMsg;
	}
#endif

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
#if !defined(SMBB_NO_IPV6)
		else if (multicastAddress.GetFamily() == IPV6) {
			ipv6_mreq requestV6;

			requestV6.ipv6mr_multiaddr = multicastAddress._ipv6.sin6_addr;
			requestV6.ipv6mr_interface = static_cast<unsigned int>(localAddress.GetInterfaceIndex());

			return setsockopt(_handle, IPPROTO_IPV6, subscribe ? IPV6_ADD_MEMBERSHIP : IPV6_DROP_MEMBERSHIP, reinterpret_cast<ConstOptionPointer>(&requestV6), static_cast<OptionLength>(sizeof(requestV6))) == 0;
		}
#endif
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

	// Returns true if the socket is valid
	bool IsValid() const { return _handle != INVALID_HANDLE; }

	// Gets the native handle of the underlying implementation
	Handle GetNativeHandle() const { return _handle; }

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
#if defined(_WIN32)
		(void)closesocket(_handle);
#else
		(void)close(_handle);
#endif
		_handle = INVALID_HANDLE;
		return Chainable<bool>(this, true);
	}

	// Closes only the sending side of a TCP socket (useful if data still needs to be received but none needs to be sent)
	Chainable<bool> CloseTCPSend() {
#if defined(_WIN32)
		(void)shutdown(_handle, SD_SEND);
#else
		(void)shutdown(_handle, SHUT_WR);
#endif
		return Chainable<bool>(this, true);
	}

	// Disables checksum calculations where applicable (useful if the data has built-in validation that will be performed at the receiving end)
	Chainable<bool> SetDisableChecksum(bool disable = true) {
#if defined(UDP_NOCHECKSUM)
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
#if defined(IP_MTU)
		if (GetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IP, IP_MTU, mtu))
			return mtu;
#endif
#if defined(IPV6_MTU)
		(void)GetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IPV6, IPV6_MTU, mtu);
#endif
		return mtu;
	}

	enum MTUDiscover {
#if defined(IP_MTU_DISCOVER)
#if defined(_WIN32)
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
#if defined(IP_MTU_DISCOVER)
		if (GetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IP, IP_MTU_DISCOVER, mtuDiscover))
			return static_cast<MTUDiscover>(mtuDiscover);
#endif
#if defined(IPV6_MTU_DISCOVER)
		(void)GetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IPV6, IPV6_MTU_DISCOVER, mtuDiscover);
#endif
		return static_cast<MTUDiscover>(mtuDiscover);
	}

	// Sets the MTU discover option (TCP only)
	Chainable<bool> SetMTUDiscover(MTUDiscover value) {
		(void)value;
#if defined(IP_MTU_DISCOVER)
		if (SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IP, IP_MTU_DISCOVER, value))
			return Chainable<bool>(this, true);
#endif
#if defined(IPV6_MTU_DISCOVER)
		if (SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IPV6, IPV6_MTU_DISCOVER, value))
			return Chainable<bool>(this, true);
#endif
		return Chainable<bool>(this, false);
	}

	// Puts the socket in non-blocking mode (or blocking mode if nonblocking is false)
	Chainable<bool> SetNonblocking(bool nonblocking = true) {
#if defined(_WIN32)
		u_long mode = nonblocking ? 1 : 0;
		return Chainable<bool>(this, ioctlsocket(_handle, FIONBIO, &mode) == 0);
#elif !defined(SMBB_NO_FCNTL)
		int flags = fcntl(_handle, F_GETFL);
		return Chainable<bool>(this, flags != -1 && fcntl(_handle, F_SETFL, nonblocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) != -1);
#else
		int mode = nonblocking ? 1 : 0;
		return Chainable<bool>(this, ioctl(_handle, FIONBIO, &mode) == 0);
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
#if defined(SO_REUSEPORT)
		return GetOptionInternalDefault<GET_OPTION_TYPE(int, DWORD)>(SOL_SOCKET, SO_REUSEPORT, 0) != 0;
#else
		return false;
#endif
	}

	// Sets the socket option to reuse the port on a socket (does not work on all OSes)
	Chainable<bool> SetReusePort(bool enable = true) {
#if defined(SO_REUSEPORT)
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
#if defined(IP_TOS)
		if (GetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IP, IP_TOS, tos))
			return static_cast<TypeOfService>(tos);
#endif
#if defined(IPV6_TCLASS)
		(void)GetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IPV6, IPV6_TCLASS, tos);
#endif
		return static_cast<TypeOfService>(tos);
	}

	// Sets the type of service option
	Chainable<bool> SetTOS(TypeOfService value) {
#if defined(_WIN32)
		DSCPData data;
		return SetDSCP(static_cast<DSCP>(value >> 2), data);
#else
#if defined(IP_TOS)
		if (SetOptionInternal<GET_OPTION_TYPE(int, DWORD)>(IPPROTO_IP, IP_TOS, value & TOS_MASK))
			return Chainable<bool>(this, true);
#endif
#if defined(IPV6_TCLASS)
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
#if defined(_WIN32) && !defined(SMBB_NO_QWAVE)
		const IPAddress _address;
		QOS_FLOWID _flow; // Internal use only

		DSCPData(const IPAddress &connectAddress = IPAddress()) : _address(connectAddress), _flow() { }
#else
		DSCPData(const IPAddress &connectAddress = IPAddress()) { (void)connectAddress; }
#endif
	};

	// Gets the DSCP value on the socket (the data argument must be the one used to set the DSCP value on some OSes)
	DSCP GetDSCP(const DSCPData &data = DSCPData()) const {
#if defined(_WIN32)
#if !defined(SMBB_NO_QWAVE)
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
#if defined(_WIN32)
#if !defined(SMBB_NO_QWAVE)
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
#if defined(_WIN32)
		else if (error == IP_SOCKET_ERROR(ALREADY) || error == IP_SOCKET_ERROR(WOULDBLOCK) || error == IP_SOCKET_ERROR(INPROGRESS))
			return Chainable<ConnectResult>(this, CONNECT_PENDING);
#else
		else if (error == IP_SOCKET_ERROR(ALREADY) || error == IP_SOCKET_ERROR(INPROGRESS))
			return Chainable<ConnectResult>(this, CONNECT_PENDING);
#endif
		return Chainable<ConnectResult>(this, CONNECT_FAILED);
	}

	enum ReceiveFlags {
		RECEIVE_NORMAL = 0,
		RECEIVE_PEEK = MSG_PEEK, // Peek at the data, but don't consume it (if data is available, the size returned will be greater than 0, but may be less than the total number of bytes available)
#if defined(MSG_DONTWAIT)
		RECEIVE_REQUEST_NONBLOCKING = MSG_DONTWAIT, // Request that the receive is non-blocking (this only works on some OSes; use if setting non-blocking mode on the socket fails)
#else
		RECEIVE_REQUEST_NONBLOCKING = 0, // (Not available) Request that the receive is non-blocking (this only works on some OSes; use if setting non-blocking mode on the socket fails)
#endif
#if defined(MSG_WAITALL)
		RECEIVE_REQUEST_WAIT_FOR_FULL_DATA = MSG_WAITALL, // Request waiting until the full amount of requested data has been consumed (this is not a guarantee, and should not be relied upon)
#else
		RECEIVE_REQUEST_WAIT_FOR_FULL_DATA = 0, // (Not available) Request waiting until the full amount of requested data has been consumed (this is not a guarantee, and should not be relied upon)
#endif
#if defined(MSG_WAITFORONE)
		RECEIVE_REQUEST_ONLY_WAIT_FOR_ONE = MSG_WAITFORONE // Request non-blocking operation after one data packet is consumed when receiving multiple packets (this is not a guarantee, use non-blocking mode if immediate return is required)
#else
		RECEIVE_REQUEST_ONLY_WAIT_FOR_ONE = 0 // (Not available) Request non-blocking operation after one data packet is consumed when receiving multiple packets (this is not a guarantee, use non-blocking mode if immediate return is required)
#endif
	};

	// Receive data from the socket
	MessageResult Receive(void *data, DataLength length, ReceiveFlags flags = RECEIVE_NORMAL) {
#if defined(_WIN32)
		DWORD bytesReceived;
		DWORD recvflags = flags;
		WSABUF buffer = { static_cast<unsigned long>(length), reinterpret_cast<char *>(data) };
		int result = WSARecv(_handle, &buffer, 1, &bytesReceived, &recvflags, NULL, NULL);

		if (result == 0)
			return MessageResult(static_cast<ResultLength>(bytesReceived), 0);

		int error = LastError();
		return MessageResult(error == IP_SOCKET_ERROR(MSGSIZE) ? static_cast<ResultLength>(bytesReceived) : -1, error);
#else
		return MessageResult(recv(_handle, reinterpret_cast<char *>(data), length, flags));
#endif
	}

	// Receive data from the socket
	MessageResult Receive(void *data, DataLength length, IPAddress &from, ReceiveFlags flags = RECEIVE_NORMAL) {
		IPAddressLength addressLength = sizeof(IPAddress);
#if defined(_WIN32)
		DWORD bytesReceived;
		DWORD recvflags = flags;
		WSABUF buffer = { static_cast<unsigned long>(length), reinterpret_cast<char *>(data) };
		int result = WSARecvFrom(_handle, &buffer, 1, &bytesReceived, &recvflags, from.GetPointer(), &addressLength, NULL, NULL);

		if (result == 0)
			return MessageResult(static_cast<ResultLength>(bytesReceived), 0);

		int error = LastError();
		return MessageResult(error == IP_SOCKET_ERROR(MSGSIZE) ? static_cast<ResultLength>(bytesReceived) : -1, error);
#else
		return MessageResult(recvfrom(_handle, reinterpret_cast<char *>(data), length, flags, from.GetPointer(), &addressLength));
#endif
	}

#if !defined(SMBB_NO_SOCKET_MSG)
	// Receive data from the socket
	MessageResult Receive(const Message &message, ReceiveFlags flags = RECEIVE_NORMAL) {
#if defined(_WIN32)
		DWORD bytesReceived;
		DWORD recvflags = flags;
		int result;

		if (message._value.msg_name) {
			message._value.msg_namelen = sizeof(IPAddress);
			result = WSARecvFrom(_handle, message._value.lpBuffers, message._value.dwBufferCount, &bytesReceived, &recvflags, message._value.msg_name, &message._value.msg_namelen, NULL, NULL);
		}
		else {
			result = WSARecv(_handle, message._value.lpBuffers, message._value.dwBufferCount, &bytesReceived, &recvflags, NULL, NULL);
		}

		if (result == 0)
			return MessageResult(static_cast<ResultLength>(bytesReceived), 0);

		int error = LastError();
		return MessageResult(error == IP_SOCKET_ERROR(MSGSIZE) ? static_cast<ResultLength>(bytesReceived) : -1, error);
#else
		message._value.msg_namelen = static_cast<int>(message._value.msg_name ? sizeof(IPAddress) : 0);
		ResultLength result = recvmsg(_handle, &message._value, flags);
		return MessageResult(result, (result < 0 ? LastError() : ((message._value.msg_flags & MSG_TRUNC) != 0 ? IP_SOCKET_ERROR(MSGSIZE) : 0)));
#endif
	}

	// Checks if a native receive multiple function is available (otherwise, this is emulated with multiple receive calls)
	static bool HasNativeReceiveMultiple() { return GetRecvMMsg() != RecvMMsgFunction::Type(); }

	// Receive multiple data packets from the socket (result is number of messages received)
	MessageResult ReceiveMultiple(MultiMessagePart parts[], ResultLength length, ReceiveFlags flags = RECEIVE_NORMAL) {
		if (GetRecvMMsg())
			return MessageResult(GetRecvMMsg()(_handle, parts, length, flags, NULL));

		for (ResultLength i = 0; i < length; i++) {
			MessageResult result = Receive(parts[i]._message, flags);

			if (result.Failed())
				return MessageResult(i == 0 ? -1 : i, result.GetError());

			parts[i]._result = result.GetResult();
		}

		return MessageResult(length, 0);
	}
#endif // SMBB_NO_SOCKET_MSG

	// Send data on the socket
	MessageResult Send(const void *data, DataLength length) {
		return MessageResult(send(_handle, reinterpret_cast<char *>(const_cast<void *>(data)), length, SEND_FLAGS));
	}

	// Send data to the specific address on the socket
	MessageResult Send(const void *data, DataLength length, const IPAddress &address) {
		return MessageResult(sendto(_handle, reinterpret_cast<char *>(const_cast<void *>(data)), length, SEND_FLAGS, address.GetPointer(), address.GetLength()));
	}

#if !defined(SMBB_NO_SOCKET_MSG)
	// Send data to the specific address on the socket
	MessageResult Send(const Message &message) {
		message._value.msg_namelen = (message._value.msg_name ? reinterpret_cast<const IPAddress *>(message._value.msg_name)->GetLength() : 0);
#if defined(_WIN32)
		DWORD bytesSent;
		int result = WSASendTo(_handle, message._value.lpBuffers, message._value.dwBufferCount, &bytesSent, SEND_FLAGS, message._value.msg_name, message._value.msg_namelen, NULL, NULL);

		return MessageResult(result == 0 ? static_cast<int>(bytesSent) : -1);
#else
		return MessageResult(sendmsg(_handle, &message._value, SEND_FLAGS));
#endif
	}

	// Checks if a native send multiple function is available (otherwise, this is emulated with multiple send calls)
	static bool HasNativeSendMultiple() { return GetSendMMsg() != SendMMsgFunction::Type(); }

	// Send multiple data packets on the socket (result is number of messages sent)
	MessageResult SendMultiple(MultiMessagePart parts[], ResultLength length) {
		if (GetSendMMsg())
			return MessageResult(GetSendMMsg()(_handle, parts, length, SEND_FLAGS));

		for (ResultLength i = 0; i < length; i++) {
			MessageResult result = Send(parts[i]._message);

			if (result.Failed())
				return MessageResult(i == 0 ? -1 : i, result.GetError());

			parts[i]._result = result.GetResult();
		}

		return MessageResult(length, 0);
	}
#endif // SMBB_NO_SOCKET_MSG

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

// Wraps an IP socket in a container that is automatically closed when it falls out of scope.
//  Note: The underlying IP socket can still be changed, so care must be taken to close any intermediate connections
class AutoCloseIPSocket : public IPSocket {
	// Disable public copying / assignment
	AutoCloseIPSocket(const AutoCloseIPSocket &);
	AutoCloseIPSocket &operator=(const IPSocket &other) { IPSocket::operator=(other); return *this; }
	AutoCloseIPSocket &operator=(const AutoCloseIPSocket &other) { IPSocket::operator=(other); return *this; }

public:
	AutoCloseIPSocket(const IPSocket &socket = IPSocket()) : IPSocket(socket) { }
	AutoCloseIPSocket(IPAddressFamily family, IPProtocol protocol) : IPSocket(family, protocol) { }
	AutoCloseIPSocket(const IPAddress &address, IPProtocol protocol, OpenAttempt openAttempt = OPEN_ONLY) : IPSocket(address, protocol, openAttempt) { }

	~AutoCloseIPSocket() {
		if (IsValid())
			(void)Close();
	}

	// Swaps the contents of this with another, and returns the guard
	AutoCloseIPSocket &Swap(AutoCloseIPSocket &other) {
		IPSocket otherSocket = other;
		other = *this;
		return operator=(otherSocket);
	}
};

inline IPSocket::SelectValue operator|(IPSocket::SelectValue x, IPSocket::SelectValue y) { return static_cast<IPSocket::SelectValue>(static_cast<int>(x) | y); }
inline IPSocket::SelectValue operator&(IPSocket::SelectValue x, IPSocket::SelectValue y) { return static_cast<IPSocket::SelectValue>(static_cast<int>(x) & y); }

#if !defined(SMBB_NO_POLL)
inline IPSocket::PollValue operator|(IPSocket::PollValue x, IPSocket::PollValue y) { return static_cast<IPSocket::PollValue>(static_cast<int>(x) | y); }
inline IPSocket::PollValue operator&(IPSocket::PollValue x, IPSocket::PollValue y) { return static_cast<IPSocket::PollValue>(static_cast<int>(x) & y); }
#endif

inline IPSocket::TypeOfService operator|(IPSocket::TypeOfService x, IPSocket::TypeOfService y) { return static_cast<IPSocket::TypeOfService>(static_cast<int>(x) | y); }
inline IPSocket::TypeOfService operator&(IPSocket::TypeOfService x, IPSocket::TypeOfService y) { return static_cast<IPSocket::TypeOfService>(static_cast<int>(x) & y); }

inline IPSocket::DSCP operator|(IPSocket::DSCP x, IPSocket::DSCP y) { return static_cast<IPSocket::DSCP>(static_cast<int>(x) | y); }
inline IPSocket::DSCP operator&(IPSocket::DSCP x, IPSocket::DSCP y) { return static_cast<IPSocket::DSCP>(static_cast<int>(x) & y); }

inline IPSocket::ReceiveFlags operator|(IPSocket::ReceiveFlags x, IPSocket::ReceiveFlags y) { return static_cast<IPSocket::ReceiveFlags>(static_cast<int>(x) | y); }
inline IPSocket::ReceiveFlags operator&(IPSocket::ReceiveFlags x, IPSocket::ReceiveFlags y) { return static_cast<IPSocket::ReceiveFlags>(static_cast<int>(x) & y); }

}

#undef GET_OPTION_TYPE

#endif
