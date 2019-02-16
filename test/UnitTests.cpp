
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

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <iostream>
#include <string>

#define NON_LOCAL_TEST_HOST "github.com"

#include "smbb/DataView.h"

using namespace smbb;

SCENARIO("Data View Utilities Test", "[DataView], [Utilities]") {
	GIVEN("A buffer of bytes") {
		DataView::Byte buffer[64] = { 0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F };

		WHEN("A data view wraps the buffer") {
			THEN("The 64-bit value retrieved from the buffer matches the expected value") {
				int64_t valueStored = (int64_t(0xFEDCBA98) << 32) | 0x76543210;
				REQUIRE(DataView::Get<int64_t>(buffer) == valueStored);
			}

			THEN("The double value retrieved from the buffer matches the expected value") {
				REQUIRE(DataView::Get<double>(buffer + 8) == 1.0);
			}
		}

		WHEN("A 64-bit value is put into the buffer using a data view") {
			int64_t valueStored = (int64_t(0xFEDCBA98) << 32) | 0x76543210;
			DataView::Set<int64_t>(buffer + 8, valueStored);

			THEN("The values retrieved from the buffer match the expected values") {
				REQUIRE(DataView::Get<int64_t>(buffer + 8) == valueStored);

				REQUIRE(DataView::Get<int32_t>(buffer + 8) == int32_t(valueStored));
				REQUIRE(DataView::Get<int32_t>(buffer + 12) == int32_t(valueStored >> 32));

				REQUIRE(DataView::Get<int16_t>(buffer + 8) == int16_t(valueStored));
				REQUIRE(DataView::Get<int16_t>(buffer + 14) == int16_t(valueStored >> 48));

				REQUIRE(DataView::Get<int8_t>(buffer + 8) == int8_t(valueStored));
				REQUIRE(DataView::Get<int8_t>(buffer + 15) == int8_t(valueStored >> 56));
			}
		}

		WHEN("Boolean values are modified in the buffer using a data view") {
			REQUIRE(!DataView::GetBool<0>(buffer));
			REQUIRE(DataView::GetBool<4>(buffer));
			REQUIRE(!DataView::GetBool<0>(buffer + 1));
			REQUIRE(DataView::GetBool<1>(buffer + 1));

			DataView::SetBool<0>(buffer, true);
			DataView::SetBool<4>(buffer, false);
			DataView::SetTrue<0>(buffer + 1);
			DataView::SetFalse<1>(buffer + 1);

			THEN("The boolean values retrieved from the buffer match the expected values") {
				REQUIRE(DataView::GetBool<0>(buffer));
				REQUIRE(!DataView::GetBool<4>(buffer));
				REQUIRE(DataView::GetBool<0>(buffer + 1));
				REQUIRE(!DataView::GetBool<1>(buffer + 1));
			}
		}
	}
}

#include "smbb/SharedMemorySection.h"

SCENARIO("Shared Memory Utilities Test", "[SharedMemory], [Utilities]") {
	GIVEN("Some file-backed shared memory") {
		SharedMemory testFile;

		REQUIRE(testFile.CreateFileBacked("Test 1", SharedMemorySection::GetOffsetSize() + 4096, true) == SharedMemory::LOAD_SUCCESS);

		SharedMemorySection section1(testFile, SharedMemorySection::GetOffsetSize() + 4096);

		REQUIRE(!section1.ReadOnly());

		WHEN("The shared memory is opened and the data modified") {
			SharedMemory testFile2;

			REQUIRE(testFile2.OpenFileBacked("Test 1") == SharedMemory::LOAD_SUCCESS);

			THEN("The memory in the created shared memory section matches the memory in the opened shared memory section") {
				SharedMemorySection section2(testFile2, 4096, SharedMemorySection::GetOffsetSize());
				SharedMemorySection section21(testFile2, 4096);

				REQUIRE(section2.ReadOnly());
				REQUIRE(section21.ReadOnly());

				REQUIRE(section21.Size() == 4096);
				REQUIRE(section21.Valid());

				strcpy((char *)section1.Data(), "Test String 1");
				strcpy((char *)section1.Data() + SharedMemorySection::GetOffsetSize(), "Test String 1");

				REQUIRE(std::string((const char *)section2.Data()) == "Test String 1");

				strcpy((char *)section1.Data() + SharedMemorySection::GetOffsetSize(), "Test String 2");

				REQUIRE(std::string((const char *)section21.Data()) == "Test String 1");
				REQUIRE(std::string((const char *)section2.Data()) == "Test String 2");
			}
		}

		WHEN("A shared memory section is created with no size") {
			SharedMemorySection section2(testFile, 0);

			THEN("The shared memory section is invalid") {
				REQUIRE(section2.Size() == 0);
				REQUIRE(!section2.Valid());
			}
		}
	}

	GIVEN("Some named shared memory") {
		SharedMemory testFile;

		REQUIRE(testFile.CreateNamed("Test 1", SharedMemorySection::GetOffsetSize() + 4096, true) == SharedMemory::LOAD_SUCCESS);

		SharedMemorySection section1(testFile, SharedMemorySection::GetOffsetSize() + 4096);

		REQUIRE(!section1.ReadOnly());

		WHEN("The shared memory is opened and the data modified") {
			SharedMemory testFile2;

			REQUIRE(testFile2.OpenNamed("Test 1") == SharedMemory::LOAD_SUCCESS);

			THEN("The memory in the created shared memory section matches the memory in the opened shared memory section") {
				SharedMemorySection section2(testFile2, 4096, SharedMemorySection::GetOffsetSize());
				SharedMemorySection section21(testFile2, 4096);

				REQUIRE(section2.ReadOnly());
				REQUIRE(section21.ReadOnly());

				REQUIRE(section21.Size() == 4096);
				REQUIRE(section21.Valid());

				strcpy((char *)section1.Data(), "Test String 1");
				strcpy((char *)section1.Data() + SharedMemorySection::GetOffsetSize(), "Test String 1");

				REQUIRE(std::string((const char *)section2.Data()) == "Test String 1");

				strcpy((char *)section1.Data() + SharedMemorySection::GetOffsetSize(), "Test String 2");

				REQUIRE(std::string((const char *)section21.Data()) == "Test String 1");
				REQUIRE(std::string((const char *)section2.Data()) == "Test String 2");
			}
		}
	}

	GIVEN("Bad attempts to create or open shared memory") {
		WHEN("The create or open calls are made") {
			SharedMemory testFile;

			THEN("The requests fail") {
				REQUIRE(testFile.CreateFileBacked(NULL, SharedMemorySection::GetOffsetSize() + 4096, true) != SharedMemory::LOAD_SUCCESS);
				REQUIRE(testFile.CreateFileBacked(NULL, -1, true) != SharedMemory::LOAD_SUCCESS);
				REQUIRE(testFile.OpenFileBacked(NULL) != SharedMemory::LOAD_SUCCESS);

				REQUIRE(testFile.CreateNamed(NULL, SharedMemorySection::GetOffsetSize() + 4096) != SharedMemory::LOAD_SUCCESS);
				REQUIRE(testFile.CreateNamed(NULL, -1) != SharedMemory::LOAD_SUCCESS);
				REQUIRE(testFile.OpenNamed(NULL) != SharedMemory::LOAD_SUCCESS);
			}
		}
	}
}

#include "smbb/Time.h"

SCENARIO("Time Test", "[Time], [Utilities]") {
	Time start = GetMonotonicTime();
	Time currentTime = GetUTCNs();
	Time elapsed = GetMonotonicTime() - start;

	REQUIRE(start != 0);
	REQUIRE(currentTime != 0);

	Time seconds = currentTime / 1000000000;
	Time day = seconds / 86400;
	seconds = seconds % 86400;
	Time hour = seconds / 3600;
	seconds = seconds % 3600;
	Time min = seconds / 60;
	seconds = seconds % 60;

	std::cout << "Got the current time (" << day << " " << hour << ":" << min << ":" << seconds << ") in " << (double(elapsed) / GetMonotonicFrequency()) << "s" << std::endl << std::endl;
}

#include "smbb/IPSocket.h"

static void DumpAddress(const IPSocket::Address &address) {
	IPSocket::Address::String str;
	std::cout << (address.ToURI(str, true) ? str : "?") << std::endl;
}

static IPSocket::ResultLength DumpAddresses(const char *address, const char *port, bool bindable, IPSocket::AddressFamily family = IPSocket::FAMILY_UNSPECIFIED) {
	IPSocket::Address ipAddresses[16];
	IPSocket::ResultLength found = IPSocket::Address::Parse(ipAddresses, 16, address, port, bindable, family);

	for (IPSocket::ResultLength i = 0; i < found; i++)
		DumpAddress(ipAddresses[i]);

	std::cout << std::endl;
	return found;
}

static bool TestTCP(const char *address, const char *port, IPSocket::AddressFamily family = IPSocket::FAMILY_UNSPECIFIED) {
	IPSocket::Address ipAddress;
	IPSocket::ResultLength found = IPSocket::Address::Parse(&ipAddress, 1, address, port, true, family);

	if (found <= 0) {
		std::cerr << "Failed to find interfaces for " << address << ":" << port << std::endl;
		return false;
	}

	// Setup the listening socket
	IPSocket listenSocket(ipAddress, IPSocket::TCP, IPSocket::OPEN_BIND_AND_LISTEN);

	// Setup the peer
	IPSocket::Address peerAddress = IPSocket::Address(ipAddress, listenSocket.GetAddress().GetPort());
	IPSocket peer;

	if (!peer.SetImmediateSend()->SetNonblocking()->Open(peerAddress, IPSocket::TCP, IPSocket::OPEN_AND_CONNECT)) {
		std::cerr << "Failed to connect, error " << IPSocket::LastError() << std::endl;
		return false;
	}

	// Accept the connection
	IPSocket accepted;
	IPSocket::Address acceptedFrom;

	if (!listenSocket.Accept(accepted, &acceptedFrom)) {
		std::cerr << "Failed to accept, error " << IPSocket::LastError() << std::endl;
		return false;
	}

	std::cout << "Accepted connection from ";
	DumpAddress(acceptedFrom);

	if (peer.Connect(peerAddress) == IPSocket::CONNECT_FAILED) {
		std::cerr << "Failed to connect, error " << IPSocket::LastError() << std::endl;
		return false;
	}

	if (!accepted.SetImmediateSend() || !accepted.SetNonblocking()) {
		std::cerr << "Failed to set immediate send or non-blocking, error " << IPSocket::LastError() << std::endl;
		return false;
	}

	// Test sending data
	const char DATA[] = "This is a test string that is sent through the socket and should match what is received.";

	if (accepted.Send(DATA, static_cast<IPSocket::DataLength>(sizeof(DATA))).GetBytes() != sizeof(DATA)) {
		std::cerr << "Failed to write data, error " << IPSocket::LastError() << std::endl;
		return false;
	}

	// Wait for data to arrive
	IPSocket::SelectSet set;
	peer.AddToSelectSet(set, IPSocket::SELECT_CAN_READ);
	REQUIRE(peer.IsInSelectSet(set, IPSocket::SELECT_CHECK_ALL) == IPSocket::SELECT_CAN_READ);

	if (IPSocket::Select(set, 10000) <= 0) {
		std::cerr << "Failed to read data (none available), error " << IPSocket::LastError() << std::endl;
		return false;
	}

	// Receive data
	char receivedData[sizeof(DATA)];

	if (peer.Receive(receivedData, static_cast<IPSocket::DataLength>(sizeof(DATA))).GetBytes() != sizeof(DATA)) {
		std::cerr << "Failed to read data, error " << IPSocket::LastError() << std::endl;
		return false;
	}

	if (memcmp(DATA, receivedData, sizeof(DATA)) != 0) {
		std::cerr << "Read data does not match" << std::endl;
		return false;
	}

	// Test sending more data
	IPSocket::Buffer splitData[] = { IPSocket::Buffer::Make(&DATA[0], 21), IPSocket::Buffer::Make(&DATA[29], 59) };
	IPSocket::Message singleSplitData = IPSocket::Message::Make(splitData, 2);
	IPSocket::MultiMessagePart multipleSplitData = IPSocket::MultiMessagePart::Make(splitData, 2);
	const size_t SPLIT_DATA_SIZE = 21 + 59;

	if (accepted.Send(singleSplitData).GetBytes() != SPLIT_DATA_SIZE || (IPSocket::HasSendMultiple() && accepted.SendMultiple(&multipleSplitData, 1).GetBytes() != 1)) {
		std::cerr << "Failed to write data, error " << IPSocket::LastError() << std::endl;
		return false;
	}

	// Wait for data to arrive
	peer.AddToSelectSet(set, IPSocket::SELECT_CAN_READ);

	if (IPSocket::Select(set, 10000) <= 0) {
		std::cerr << "Failed to read data (none available), error " << IPSocket::LastError() << std::endl;
		return false;
	}

	// Receive data
	uint8_t recvDataBlocks[4][40];
	IPSocket::Buffer recvSplitData[] = { IPSocket::Buffer::Make(recvDataBlocks[0], 40), IPSocket::Buffer::Make(recvDataBlocks[1], 40), IPSocket::Buffer::Make(recvDataBlocks[2], 40), IPSocket::Buffer::Make(recvDataBlocks[3], 40) };
	IPSocket::Message recvSingle = IPSocket::Message::Make(recvSplitData, 2);
	IPSocket::MultiMessagePart recvMultiple = IPSocket::MultiMessagePart::Make(&recvSplitData[2], 2);

	if (peer.Receive(recvSingle).GetBytes() != 80 || IPSocket::HasReceiveMultiple() && (IPSocket::Select(set, 10000) <= 0 || peer.ReceiveMultiple(&recvMultiple, 1).GetBytes() != 1)) {
		std::cerr << "Failed to read data, error " << IPSocket::LastError() << std::endl;
		return false;
	}

	if (memcmp(DATA, recvDataBlocks[0], 21) != 0 || memcmp(&DATA[29], &recvDataBlocks[0][21], 19) != 0 || memcmp(&DATA[48], recvDataBlocks[1], 40) != 0 ||
			IPSocket::HasReceiveMultiple() && (memcmp(DATA, recvDataBlocks[2], 21) != 0 || memcmp(&DATA[29], &recvDataBlocks[2][21], 19) != 0 || memcmp(&DATA[48], recvDataBlocks[3], 40) != 0)) {
		std::cerr << "Read data does not match" << std::endl;
		return false;
	}

	std::cout << "Finished testing TCP for " << address << std::endl << std::endl;
	accepted.CloseTCPSend();
	peer.CloseTCPSend();

	return true;
}

static bool TestMulticastUDP(const char *receiveAddress, const char *multicastAddress, const char *sendAddress, IPSocket::AddressFamily family = IPSocket::FAMILY_UNSPECIFIED) {
	IPSocket::Address ipAddress;
	IPSocket::ResultLength found = IPSocket::Address::Parse(&ipAddress, 1, receiveAddress, NULL, true, family);

	if (found <= 0) {
		std::cerr << "Failed to find bindable interfaces for multicast" << std::endl;
		return false;
	}

	// Setup the read socket
	IPSocket readSocket(ipAddress, IPSocket::UDP, IPSocket::OPEN_AND_BIND);

	std::cout << "Reading on ";
	DumpAddress(readSocket.GetAddress());

	IPSocket::Address multicast;
	found = IPSocket::Address::Parse(&multicast, 1, multicastAddress, NULL, false, family);

	if (found <= 0) {
		std::cerr << "Failed to find interfaces for multicast address " << multicastAddress << std::endl;
		return false;
	}

	REQUIRE(multicast.IsMulticast());
	std::cout << "Subscribing to ";
	DumpAddress(multicast);

	if (!readSocket.SetMulticastLoopback()->SubscribeToMulticastAddress(multicast)) {
		std::cerr << "Failed to subscribe to multicast address " << multicastAddress << " (error: " << IPSocket::LastError() << ")" << std::endl;
		return false;
	}

	// Setup the sender
	IPSocket::Address sendToAddress = IPSocket::Address(multicast, readSocket.GetAddress().GetPort());
	IPSocket sendSocket;

	std::cout << "Sending to ";
	DumpAddress(sendToAddress);

	if (!sendSocket.SetMulticastLoopback()->SetMulticastHops(3)->Open(sendToAddress, IPSocket::UDP, IPSocket::OPEN_ONLY)) {
		std::cerr << "Failed to open, error " << IPSocket::LastError() << std::endl;
		return false;
	}

	if (sendAddress) {
		found = IPSocket::Address::Parse(&ipAddress, 1, sendAddress, NULL, true, family);

		if (found <= 0) {
			std::cerr << "Failed to find local interface " << sendAddress << std::endl;
			return false;
		}

		if (sendSocket.SetMulticastSendInterface(ipAddress))
			std::cout << "Using send address " << sendAddress << std::endl;
		else
			std::cerr << "Failed to use local interface " << sendAddress << std::endl;
	}

	// Test sending data
	const char DATA[] = "This is a test string that is sent through the socket and should match what is received.";
	IPSocket::SelectSet recvSet;
	size_t i = 0;

	do {
		if (sendSocket.Send(sendToAddress, DATA, static_cast<IPSocket::DataLength>(sizeof(DATA))).GetBytes() != sizeof(DATA)) {
			std::cerr << "Failed to write data, error " << IPSocket::LastError() << std::endl;
			return false;
		}

		// Wait for data to arrive
		readSocket.AddToSelectSet(recvSet, IPSocket::SELECT_CAN_READ);
	} while (i++ < 10 && IPSocket::Select(recvSet, 1000) <= 0);

	if (i >= 10) {
		std::cerr << "Failed to read data (none available), error " << IPSocket::LastError() << std::endl;
		return false;
	}

	// Receive data
	char receivedData[sizeof(DATA)];

	if (readSocket.Receive(receivedData, static_cast<IPSocket::DataLength>(sizeof(DATA))).GetBytes() != sizeof(DATA)) {
		std::cerr << "Failed to read data, error " << IPSocket::LastError() << std::endl;
		return false;
	}

	if (memcmp(DATA, receivedData, sizeof(DATA)) != 0) {
		std::cerr << "Read data does not match" << std::endl;
		return false;
	}

	std::cout << "Finished testing multicast for " << multicastAddress << std::endl << std::endl;

	return true;
}

static bool TestSocketOptions(const char *address, const char *service, IPSocket::AddressFamily family, IPSocket::Protocol protocol) {
	IPSocket::Address ipAddress;
	IPSocket::ResultLength found = IPSocket::Address::Parse(&ipAddress, 1, address, service, true, family);

	if (found <= 0) {
		std::cerr << "Failed to find interfaces for " << address << std::endl;
		return false;
	}

	IPSocket::Address::String uri;
	std::cout << "Testing " << ipAddress.ToURI(uri, true) << (protocol == IPSocket::TCP ? " (TCP)" : " (UDP)") <<  std::endl;

	// Prepare the socket
	IPSocket socket(ipAddress, protocol, IPSocket::OPEN_AND_CONNECT);

#define PRINT_RESULTS(STR, GET, SET) do { std::cout << STR << ": "; std::cout << GET << " > "; std::cout << SET << " => "; std::cout << GET << std::endl; } while (0)
	std::cout << "Disable checksum: " << socket.SetDisableChecksum() << std::endl;
	PRINT_RESULTS("Immediate send", socket.GetImmediateSend(), socket.SetImmediateSend());
	PRINT_RESULTS("Keep alive", socket.GetKeepAlive(), socket.SetKeepAlive());
	PRINT_RESULTS("Linger time (ms)", socket.GetLingerTime(), socket.SetLingerTime(5678));
	std::cout << "MTU: " << socket.GetMTU() << std::endl;
	PRINT_RESULTS("MTU Discover", socket.GetMTUDiscover(), socket.SetMTUDiscover(IPSocket::MTU_DISCOVER_FULL));
	PRINT_RESULTS("Multicast hops", socket.GetMulticastHops(), socket.SetMulticastHops(5));
	PRINT_RESULTS("Multicast loopback", socket.GetMulticastLoopback(), socket.SetMulticastLoopback());
	PRINT_RESULTS("Receive buffer size", socket.GetReceiveBufferSize(), socket.SetReceiveBufferSize(1024 * 128));
	PRINT_RESULTS("Reuse address", socket.GetReuseAddress(), socket.SetReuseAddress());
	PRINT_RESULTS("Reuse port", socket.GetReusePort(), socket.SetReusePort());
	PRINT_RESULTS("Send buffer size", socket.GetSendBufferSize(), socket.SetSendBufferSize(1024 * 32));
	IPSocket::DSCPData data(ipAddress);
	PRINT_RESULTS("DSCP", (int)socket.GetDSCP(data), socket.SetDSCP(IPSocket::DSCP_AF_CLASS_3 | IPSocket::DSCP_AF_MEDIUM_DROP, data));
	PRINT_RESULTS("TOS", (int)socket.GetTOS(), socket.SetTOS(IPSocket::TOS_PRECEDENCE_MEDIUM | IPSocket::TOS_LOW_DELAY));
	std::cout << std::endl;

	if (protocol == IPSocket::TCP)
		socket.CloseTCPSend();

	return true;
}

SCENARIO("IP Socket Test", "[IPSocket]") {
	REQUIRE(IPSocket::Initialize());

	GIVEN("A set of IP addresses and ports") {
		WHEN("Dumping information about the addresses and ports") {
			THEN("The appropriate information in displayed") {
				// General information about interfaces
				DumpAddresses(NULL, "12034", false);
				DumpAddresses(NULL, "1234", true);
				DumpAddresses("", "12034", false);
				DumpAddresses("", "1234", true);
				DumpAddresses("localhost", "", false);
				DumpAddresses("localhost", "", true);
				DumpAddresses("localhost", "12034", false);
				DumpAddresses("localhost", "1234", true);
				DumpAddresses("localhost", NULL, false, IPSocket::IPV4);
				DumpAddresses("localhost", NULL, true, IPSocket::IPV4);
			}
		}

		WHEN("Testing TCP connections using the addresses and ports") {
			THEN("The TCP test suceeds") {
				// Test TCP
				REQUIRE(TestTCP("localhost", NULL));
				REQUIRE(TestTCP("localhost", NULL, IPSocket::IPV6));
				REQUIRE(TestTCP("localhost", NULL, IPSocket::IPV4));
			}
		}

		WHEN("Testing multicast UDP connections using the addresses and ports") {
			THEN("The multicast UDP test suceeds") {
				// Test Multicast
				REQUIRE(TestMulticastUDP(NULL, "239.192.2.3", NULL, IPSocket::IPV4));
				REQUIRE(TestMulticastUDP(NULL, "ff08::0001", NULL, IPSocket::IPV6));

				IPSocket::Address ipAddresses[8];
				IPSocket::Address::String ipString;
				IPSocket::ResultLength found = IPSocket::Address::Parse(ipAddresses, 8, "", 0, false, IPSocket::IPV6);

				for (IPSocket::ResultLength i = 0; i < found; i++)
					TestMulticastUDP(NULL, "ff08::0001", ipAddresses[i].ToURI(ipString), IPSocket::IPV6);
			}
		}

		WHEN("Testing socket options using the live addresses and ports") {
			THEN("The socket options are displayed") {
				// Test Options
				REQUIRE(TestSocketOptions(NON_LOCAL_TEST_HOST, "http", IPSocket::IPV6, IPSocket::TCP));
				REQUIRE(TestSocketOptions(NON_LOCAL_TEST_HOST, "ftp", IPSocket::IPV4, IPSocket::TCP));
				REQUIRE(TestSocketOptions(NON_LOCAL_TEST_HOST, NULL, IPSocket::IPV6, IPSocket::UDP));
				REQUIRE(TestSocketOptions(NON_LOCAL_TEST_HOST, NULL, IPSocket::IPV4, IPSocket::UDP));
			}
		}
	}

	IPSocket::Finish();
}