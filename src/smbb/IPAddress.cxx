
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

#include "IPAddress.h"

#include <cstring>

#ifdef _WIN32
#include <WinSock2.h>
#include <IPHlpApi.h>
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#include <net/if.h> // Due to a BSD bug, this must appear before ifaddrs.h
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

// Parses a set of addresses from an address (NULL => loopback, "" => all non-loopback addresses) and service (NULL => any service/port)
int smbb::IPAddress::Parse(IPAddress results[], int resultsSize, const char *address, const char *service, bool bindable, IPAddressFamily family) {
	addrinfo info = { 0 };
	addrinfo *result = NULL;
	int i = 0;

	if (!address && !service)
		service = "";

	info.ai_flags = (bindable ? AI_PASSIVE : 0) | AI_V4MAPPED;
	info.ai_family = family;
	info.ai_socktype = (!service || !service[0] || (service[0] >= '0' && service[0] <= '9') ? UDP : PROTOCOL_UNSPECIFIED);

#ifndef _WIN32
	if (address && !address[0]) {
		unsigned short port = 0;
		ifaddrs *addresses = NULL;

		// Find the port
		if (getaddrinfo(NULL, service, &info, &result) == 0) {
			for (addrinfo *it = result; !port && it; it = it->ai_next) {
				if (it->ai_family == IPV4 && it->ai_addr)
					port = ntohs(reinterpret_cast<sockaddr_in *>(it->ai_addr)->sin_port);
#ifndef SMBB_NO_IPV6
				else if (it->ai_family == IPV6 && it->ai_addr)
					port = ntohs(reinterpret_cast<sockaddr_in6 *>(it->ai_addr)->sin6_port);
#endif
			}

			freeaddrinfo(result);
		}

		// Iterate all addresses
		const char LOOPBACK_IP[] = { 127, 0, 0, 1 };
#ifndef SMBB_NO_IPV6
		const char LOOPBACK_IPV6[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
#endif
		if (getifaddrs(&addresses) == 0) {
			for (ifaddrs *it = addresses; i < resultsSize && it; it = it->ifa_next) {
				if ((family == FAMILY_UNSPECIFIED || family == IPV4) && it->ifa_addr && it->ifa_addr->sa_family == IPV4 && memcmp(&reinterpret_cast<sockaddr_in *>(it->ifa_addr)->sin_addr, LOOPBACK_IP, sizeof(LOOPBACK_IP)) != 0)
					results[i++] = IPAddress(*reinterpret_cast<sockaddr_in *>(it->ifa_addr), port);
#ifndef SMBB_NO_IPV6
				else if ((family == FAMILY_UNSPECIFIED || family == IPV6) && it->ifa_addr && it->ifa_addr->sa_family == IPV6 && memcmp(&reinterpret_cast<sockaddr_in6 *>(it->ifa_addr)->sin6_addr, LOOPBACK_IPV6, sizeof(LOOPBACK_IPV6)) != 0)
					results[i++] = IPAddress(*reinterpret_cast<sockaddr_in6 *>(it->ifa_addr), port);
#endif
			}

			freeifaddrs(addresses);
			return i;
		}
	}
	else
#endif
	if (getaddrinfo(address, service, &info, &result) == 0) {
		for (addrinfo *it = result; i < resultsSize && it; it = it->ai_next) {
			if (it->ai_family == IPV4 && it->ai_addr)
				results[i++] = IPAddress(*reinterpret_cast<sockaddr_in *>(it->ai_addr));
#ifndef SMBB_NO_IPV6
			else if (it->ai_family == IPV6 && it->ai_addr)
				results[i++] = IPAddress(*reinterpret_cast<sockaddr_in6 *>(it->ai_addr));
#endif
		}

		freeaddrinfo(result);
		return i;
	}

	return -1;
}

// Gets the interface index from the address
int smbb::IPAddress::GetInterfaceIndex() const {
	// Check for empty; if empty return 0
	if (GetFamily() == FAMILY_UNSPECIFIED || IsAny())
		return 0;

	// Go through all available address
	int result = -1;
#ifdef _WIN32
	IP_ADAPTER_ADDRESSES stackBuffer[32]; // Use .PhysicalAddress to get MAC Address
	ULONG bufferSize = static_cast<ULONG>(sizeof(stackBuffer));

	// Loop through all adapters, allocating a larger chunk of memory if needed (this is one exception to the no dynamic memory rule, and it is only for the Win32 API, so we should be okay)
	for (IP_ADAPTER_ADDRESSES *buffer = stackBuffer; buffer; buffer = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(malloc(bufferSize))) {
		ULONG getAdaptersResult = GetAdaptersAddresses(GetFamily(), GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_FRIENDLY_NAME, NULL, buffer, &bufferSize);

		if (getAdaptersResult == NO_ERROR) {
			for (IP_ADAPTER_ADDRESSES *it = buffer; result < 0 && it; it = it->Next) {
				for (IP_ADAPTER_UNICAST_ADDRESS *addr = it->FirstUnicastAddress; result < 0 && addr; addr = addr->Next) {
					if (GetFamily() == IPV4 && addr->Address.lpSockaddr && reinterpret_cast<sockaddr_in *>(addr->Address.lpSockaddr)->sin_addr.s_addr == _ipv4.sin_addr.s_addr)
						result = static_cast<int>(it->IfIndex);
#ifndef SMBB_NO_IPV6
					else if (GetFamily() == IPV6 && addr->Address.lpSockaddr && memcmp(&reinterpret_cast<sockaddr_in6 *>(addr->Address.lpSockaddr)->sin6_addr, &_ipv6.sin6_addr, sizeof(_ipv6.sin6_addr)) == 0)
						result = static_cast<int>(it->Ipv6IfIndex);
#endif
				}
			}
		}

		if (buffer != stackBuffer)
			free(buffer);

		if (getAdaptersResult != ERROR_BUFFER_OVERFLOW)
			break;
	}
#else
	ifaddrs *addresses = NULL;

	if (getifaddrs(&addresses) == 0) {
		for (ifaddrs *it = addresses; result < 0 && it; it = it->ifa_next) {
			if (it->ifa_addr && GetFamily() == it->ifa_addr->sa_family && ( // MAC Address when sa_family == AF_LINK (struct sockaddr_dl *, sdl_data + sdl_nlen, sdl_alen) or AF_PACKET (struct sockaddr_ll*, sll_addr)
#ifndef SMBB_NO_IPV6
				(GetFamily() == IPV6 && memcmp(&reinterpret_cast<sockaddr_in6 *>(it->ifa_addr)->sin6_addr, &_ipv6.sin6_addr, sizeof(_ipv6.sin6_addr)) == 0) ||
#endif
				(GetFamily() == IPV4 && reinterpret_cast<sockaddr_in *>(it->ifa_addr)->sin_addr.s_addr == _ipv4.sin_addr.s_addr)))
				result = if_nametoindex(it->ifa_name);
		}

		freeifaddrs(addresses);
	}
#endif
	return result;
}

#define IP_ADDRESS_GET_DIGIT(VALUE, BUFFER, DIV, SHOW) do { \
	char digit = static_cast<char>(VALUE / DIV); \
	VALUE = VALUE % DIV; \
	SHOW |= digit; \
\
	if (SHOW) \
		*BUFFER++ = '0' + digit; \
} while (0)

// Gets the string URI authority (host/port) representation of the address, returning the buffer if successful, or NULL if the address is not a valid addresss
char *smbb::IPAddress::ToURI(String buffer, bool includePort) const {
	char *start = buffer;
	unsigned short port = 0;

	if (GetFamily() == IPV4) {
		port = ntohs(_ipv4.sin_port);
		unsigned long value = ntohl(_ipv4.sin_addr.s_addr);

		for (size_t i = 0; i < 4; i++) {
			if (i)
				*buffer++ = '.';

			unsigned char show = 0;
			unsigned char num = static_cast<unsigned char>(value >> (8 * (3 - i)));
			IP_ADDRESS_GET_DIGIT(num, buffer, 100, show);
			IP_ADDRESS_GET_DIGIT(num, buffer, 10, show);
			*buffer++ = static_cast<char>('0' + num);
		}
	}
#ifndef SMBB_NO_IPV6
	else if (GetFamily() == IPV6) {
		const char hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

		port = ntohs(_ipv6.sin6_port);
		bool foundZeros = false;
		bool inZeros = false;

		*buffer++ = '[';

		for (size_t i = 0; i < 8; i++) {
			unsigned char a = *(reinterpret_cast<const unsigned char *>(&_ipv6.sin6_addr) + 2 * i);
			unsigned char b = *(reinterpret_cast<const unsigned char *>(&_ipv6.sin6_addr) + 2 * i + 1);

			if (a == 0 && b == 0 && !foundZeros && (inZeros || (i < 7 &&
					*(reinterpret_cast<const unsigned char *>(&_ipv6.sin6_addr) + 2 * i + 2) == 0 &&
					*(reinterpret_cast<const unsigned char *>(&_ipv6.sin6_addr) + 2 * i + 3) == 0)))
				inZeros = true;
			else {
				if (inZeros) *buffer++ = ':';
				if (i)       *buffer++ = ':';

				if (a >> 4)        *buffer++ = hex[a >> 4];
				if (a)             *buffer++ = hex[a & 0xF];
				if (a || (b >> 4)) *buffer++ = hex[b >> 4];
						            *buffer++ = hex[b & 0xF];

				if (inZeros) {
					inZeros = false;
					foundZeros = true;
				}
			}
		}

		if (buffer == start + 1) {
			*buffer++ = ':';
			*buffer++ = ':';
		}

		*buffer++ = ']';
	}
#endif
	else
		return NULL;

	// Append :[port] if desired
	if (includePort) {
		*buffer++ = ':';

		unsigned char show = 0;
		IP_ADDRESS_GET_DIGIT(port, buffer, 10000, show);
		IP_ADDRESS_GET_DIGIT(port, buffer, 1000, show);
		IP_ADDRESS_GET_DIGIT(port, buffer, 100, show);
		IP_ADDRESS_GET_DIGIT(port, buffer, 10, show);
		*buffer++ = static_cast<char>('0' + port);
	}

	*buffer = 0;

	return start;
}
