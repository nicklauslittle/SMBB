
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

#ifndef SMBB_IPADDRESS_H
#define SMBB_IPADDRESS_H

#include <cstring>

#include "utilities/StaticCast.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace smbb {

enum IPAddressFamily {
	FAMILY_UNSPECIFIED = AF_UNSPEC,
	IPV4 = AF_INET,
	IPV6 = AF_INET6
};

enum IPProtocol {
	PROTOCOL_UNSPECIFIED = 0,
	TCP = SOCK_STREAM,
	UDP = SOCK_DGRAM
};

#ifdef _WIN32
typedef int IPAddressLength;
#else
typedef socklen_t IPAddressLength;
#endif

union IPAddress {
private:
	sockaddr_in6 _ipv6;
	sockaddr_in _ipv4;

public:
	typedef char String[64];

	// Parses a set of addresses from an address (NULL => loopback, "" => all non-loopback addresses) and service (NULL => any service/port)
	static int Parse(IPAddress results[], int resultsSize, const char *address, const char *service = NULL, bool bindable = false, IPAddressFamily family = FAMILY_UNSPECIFIED);

	// Gets the loopback address for the specified family
	static IPAddress Loopback(IPAddressFamily family) {
		const char LOOPBACK_IPV6[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
		const char LOOPBACK_IP[] = { 127, 0, 0, 1 };

		IPAddress address(family);

		if (family == IPV4)
			(void)memcpy(&address._ipv4.sin_addr, LOOPBACK_IP, sizeof(LOOPBACK_IP));
		else if (family == IPV6)
			(void)memcpy(&address._ipv6.sin6_addr, LOOPBACK_IPV6, sizeof(LOOPBACK_IPV6));

		return address;
	}

	// Constructs an empty address (equivalent to "any" address)
	IPAddress(IPAddressFamily family = FAMILY_UNSPECIFIED) : _ipv6() { _ipv6.sin6_family = StaticCast(_ipv6.sin6_family, family); }

	// Constructs a new address from a native address
	IPAddress(const sockaddr_in6 &address) : _ipv6(address) { }
	IPAddress(const sockaddr_in &address) : _ipv4(address) { }

	// Constructs a new address with a different port
	IPAddress(const IPAddress &address, unsigned short port) {
		*this = address;

		if (GetFamily() == IPV4)
			_ipv4.sin_port = htons(port);
		else if (GetFamily() == IPV6)
			_ipv6.sin6_port = htons(port);
	}

	// Constructs a new address using the parse function
	IPAddress(const char *address, const char *service = NULL, bool bindable = false, IPAddressFamily family = FAMILY_UNSPECIFIED) : _ipv6() {
		Parse(this, 1, address, service, bindable, family);
	}

	// Get the family of the address
	IPAddressFamily GetFamily() const { return static_cast<IPAddressFamily>(_ipv4.sin_family); }

	// Gets the interface index from the address
	int GetInterfaceIndex() const;

	// Gets the length of the address
	IPAddressLength GetLength() const {
		if (GetFamily() == IPV4)
			return sizeof(sockaddr_in);
		else if (GetFamily() == IPV6)
			return sizeof(sockaddr_in6);

		return 0;
	}

	// Gets the address pointer
	sockaddr *GetPointer() const { return const_cast<sockaddr *>(reinterpret_cast<const sockaddr *>(&_ipv4)); }

	// Gets the port associated with the address
	short GetPort() const {
		if (GetFamily() == IPV6)
			return ntohs(_ipv6.sin6_port);
		else if (GetFamily() == IPV4)
			return ntohs(_ipv4.sin_port);

		return 0;
	}

	// Gets a hash of the address (without the port)
	size_t Hash() const {
		const int multiplier = 16777619;
		size_t hash = 0x811c9dc5;

		if (GetFamily() == IPV4) {
			for (size_t i = 0; i < 4; i++)
				hash = (hash ^ reinterpret_cast<const char *>(&_ipv4.sin_addr)[i]) * multiplier;
		}
		else if (GetFamily() == IPV6) {
			for (size_t i = 0; i < 16; i++)
				hash = (hash ^ reinterpret_cast<const char *>(&_ipv6.sin6_addr)[i]) * multiplier;
		}

		return hash;
	}

	// Checks if the address is the any address
	bool IsAny() const {
		const char ANY_IPV6[sizeof(_ipv6.sin6_addr)] = { };
		const char ANY_IP[sizeof(_ipv4.sin_addr)] = { };

		if (GetFamily() == IPV4)
			return memcmp(&_ipv4.sin_addr, ANY_IP, sizeof(_ipv4.sin_addr)) == 0;
		else if (GetFamily() == IPV6)
			return memcmp(&_ipv6.sin6_addr, ANY_IPV6, sizeof(_ipv6.sin6_addr)) == 0;

		return false;
	}

	// Checks if the address is the loopback address
	bool IsLoopback() const {
		const char LOOPBACK_IPV6[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
		const char LOOPBACK_IP[] = { 127, 0, 0, 1 };

		if (GetFamily() == IPV4)
			return memcmp(&_ipv4.sin_addr, LOOPBACK_IP, sizeof(LOOPBACK_IP)) == 0;
		else if (GetFamily() == IPV6)
			return memcmp(&_ipv6.sin6_addr, LOOPBACK_IPV6, sizeof(LOOPBACK_IPV6)) == 0;

		return false;
	}

	// Checks if the address is a multicast address
	bool IsMulticast() const {
		if (GetFamily() == IPV4)
			return (*reinterpret_cast<const unsigned char *>(&_ipv4.sin_addr) & 0xF0) == 0xE0;
		else if (GetFamily() == IPV6)
			return (*reinterpret_cast<const unsigned char *>(&_ipv6.sin6_addr) & 0xFF) == 0xFF;

		return false;
	}

	// Gets whether or not the address is valid
	bool IsValid() const { return GetFamily() != FAMILY_UNSPECIFIED; }

	// Gets the string URI authority (host/port) representation of the address
	char *ToURI(String buffer, bool includePort = false) const;

	friend class IPSocket;
};

}

#endif
