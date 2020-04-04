
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

#ifndef SMBB_IPADDRESS_H
#define SMBB_IPADDRESS_H

#include <cstring>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "utilities/Inline.h"
#include "utilities/StaticCast.h"

namespace smbb {

enum IPAddressFamily {
	IPV4 = AF_INET,
#if !defined(SMBB_NO_IPV6)
	IPV6 = AF_INET6,
#endif
	FAMILY_UNSPECIFIED = AF_UNSPEC
};

enum IPProtocol {
	PROTOCOL_UNSPECIFIED = 0,
	TCP = SOCK_STREAM,
	UDP = SOCK_DGRAM
};

#if defined(_WIN32)
typedef int IPAddressLength;
#else
typedef socklen_t IPAddressLength;
#endif

union IPAddress {
private:
#if !defined(SMBB_NO_IPV6)
	sockaddr_in6 _ipv6;
#endif
	sockaddr_in _ipv4;

public:
	typedef char String[64];

	// Parses a set of addresses from an address (NULL => loopback, "" => all non-loopback addresses) and service (NULL => any service/port)
	static SMBB_INLINE int Parse(IPAddress results[], int resultsSize, const char *address, const char *service = NULL, bool bindable = false, IPAddressFamily family = FAMILY_UNSPECIFIED);

	// Gets the loopback address for the specified family
	static IPAddress Loopback(IPAddressFamily family) {
		const char LOOPBACK_IP[] = { 127, 0, 0, 1 };
#if !defined(SMBB_NO_IPV6)
		const char LOOPBACK_IPV6[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
#endif
		IPAddress address(family);

		if (family == IPV4)
			(void)memcpy(&address._ipv4.sin_addr, LOOPBACK_IP, sizeof(LOOPBACK_IP));
#if !defined(SMBB_NO_IPV6)
		else if (family == IPV6)
			(void)memcpy(&address._ipv6.sin6_addr, LOOPBACK_IPV6, sizeof(LOOPBACK_IPV6));
#endif
		return address;
	}

	// Constructs an empty address (equivalent to "any" address)
	IPAddress(IPAddressFamily family = FAMILY_UNSPECIFIED) {
		memset(this, 0, sizeof(*this));
		_ipv4.sin_family = StaticCast(_ipv4.sin_family, family);
	}

	// Constructs a new address from a native address
	IPAddress(const sockaddr_in &address) : _ipv4(address) { _ipv4.sin_family = StaticCast(_ipv4.sin_family, IPV4); }
#if !defined(SMBB_NO_IPV6)
	IPAddress(const sockaddr_in6 &address) : _ipv6(address) { _ipv6.sin6_family = StaticCast(_ipv6.sin6_family, IPV6); }
#endif

	// Constructs a new address with a different port
	IPAddress(const IPAddress &address, unsigned short port) {
		*this = address;

		if (GetFamily() == IPV4)
			_ipv4.sin_port = htons(port);
#if !defined(SMBB_NO_IPV6)
		else if (GetFamily() == IPV6)
			_ipv6.sin6_port = htons(port);
#endif
	}

	// Constructs a new address using the parse function
	IPAddress(const char *address, const char *service = NULL, bool bindable = false, IPAddressFamily family = FAMILY_UNSPECIFIED) {
		memset(this, 0, sizeof(*this));
		Parse(this, 1, address, service, bindable, family);
	}

	// Compare to another address
	bool operator==(const IPAddress &other) const {
		if (GetFamily() == other.GetFamily()) {
			if (GetFamily() == IPV4)
				return memcmp(&_ipv4.sin_addr, &other._ipv4.sin_addr, sizeof(_ipv4.sin_addr)) == 0 && _ipv4.sin_port == other._ipv4.sin_port;
#if !defined(SMBB_NO_IPV6)
			else if (GetFamily() == IPV6)
				return memcmp(&_ipv6.sin6_addr, &other._ipv6.sin6_addr, sizeof(_ipv6.sin6_addr)) == 0 && _ipv6.sin6_port == other._ipv6.sin6_port;
#endif
		}

		return false;
	}

	bool operator!=(const IPAddress &other) const { return !(*this == other); }

	// Get the family of the address
	IPAddressFamily GetFamily() const { return static_cast<IPAddressFamily>(_ipv4.sin_family); }

	// Gets the interface index from the address
	SMBB_INLINE int GetInterfaceIndex() const;

	// Gets the length of the address
	IPAddressLength GetLength() const {
		if (GetFamily() == IPV4)
			return sizeof(sockaddr_in);
#if !defined(SMBB_NO_IPV6)
		else if (GetFamily() == IPV6)
			return sizeof(sockaddr_in6);
#endif
		return 0;
	}

	// Gets the address pointer
	sockaddr *GetPointer() const { return const_cast<sockaddr *>(reinterpret_cast<const sockaddr *>(this)); }

	// Gets the port associated with the address
	short GetPort() const {
		if (GetFamily() == IPV4)
			return ntohs(_ipv4.sin_port);
#if !defined(SMBB_NO_IPV6)
		else if (GetFamily() == IPV6)
			return ntohs(_ipv6.sin6_port);
#endif
		return 0;
	}

	// Gets a hash of the address (without the port)
	size_t Hash(bool includePort = true) const {
		const int multiplier = 16777619;
		size_t hash = 0x811c9dc5;

		if (GetFamily() == IPV4) {
			for (size_t i = 0; i < 4; i++)
				hash = (hash ^ reinterpret_cast<const unsigned char *>(&_ipv4.sin_addr)[i]) * multiplier;

			if (includePort)
				hash = (((hash ^ static_cast<unsigned char>(_ipv4.sin_port)) * multiplier) ^ static_cast<unsigned char>(_ipv4.sin_port >> 8)) * multiplier;
		}
#if !defined(SMBB_NO_IPV6)
		else if (GetFamily() == IPV6) {
			for (size_t i = 0; i < 16; i++)
				hash = (hash ^ reinterpret_cast<const char *>(&_ipv6.sin6_addr)[i]) * multiplier;

			if (includePort)
				hash = (((hash ^ static_cast<unsigned char>(_ipv6.sin6_port)) * multiplier) ^ static_cast<unsigned char>(_ipv6.sin6_port >> 8)) * multiplier;
		}
#endif
		return hash;
	}

	// Checks if the address is the any address
	bool IsAny() const {
		const char ANY_IP[sizeof(_ipv4.sin_addr)] = { };
#if !defined(SMBB_NO_IPV6)
		const char ANY_IPV6[sizeof(_ipv6.sin6_addr)] = { };
#endif

		if (GetFamily() == IPV4)
			return memcmp(&_ipv4.sin_addr, ANY_IP, sizeof(_ipv4.sin_addr)) == 0;
#if !defined(SMBB_NO_IPV6)
		else if (GetFamily() == IPV6)
			return memcmp(&_ipv6.sin6_addr, ANY_IPV6, sizeof(_ipv6.sin6_addr)) == 0;
#endif
		return false;
	}

	// Checks if the address is the loopback address
	bool IsLoopback() const {
		const char LOOPBACK_IP[] = { 127, 0, 0, 1 };
#if !defined(SMBB_NO_IPV6)
		const char LOOPBACK_IPV6[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
#endif
		if (GetFamily() == IPV4)
			return memcmp(&_ipv4.sin_addr, LOOPBACK_IP, sizeof(LOOPBACK_IP)) == 0;
#if !defined(SMBB_NO_IPV6)
		else if (GetFamily() == IPV6)
			return memcmp(&_ipv6.sin6_addr, LOOPBACK_IPV6, sizeof(LOOPBACK_IPV6)) == 0;
#endif
		return false;
	}

	// Checks if the address is a multicast address
	bool IsMulticast() const {
		if (GetFamily() == IPV4)
			return (*reinterpret_cast<const unsigned char *>(&_ipv4.sin_addr) & 0xF0) == 0xE0;
#if !defined(SMBB_NO_IPV6)
		else if (GetFamily() == IPV6)
			return (*reinterpret_cast<const unsigned char *>(&_ipv6.sin6_addr) & 0xFF) == 0xFF;
#endif
		return false;
	}

	// Gets whether or not the address is valid
	bool IsValid() const { return GetFamily() != FAMILY_UNSPECIFIED; }

	// Gets the string URI authority (host/port) representation of the address
	SMBB_INLINE char *ToURI(String buffer, bool includePort = true) const;

	friend class IPSocket;
};

}

#endif
