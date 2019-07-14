
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

#include "smbb/SharedMemorySection.h"

using namespace smbb;

SCENARIO ("Shared Memory Utilities Test", "[SharedMemory], [Utilities]") {
	GIVEN ("Some file-backed shared memory") {
		SharedMemory testFile, testFileAutoDelete;
		char testFilePath[1024];

		REQUIRE(SharedMemory::GetRecommendedDirectory(testFilePath, 1024));
		std::string testFileName = std::string(testFilePath) + "/SMBB-Test 1";

		SharedMemory::DeleteFileBacked(testFileName.c_str());

		REQUIRE(testFile.CreateFileBacked(testFileName.c_str(), SharedMemorySection::GetOffsetSize() + 4096, false) == SharedMemory::LOAD_SUCCESS);
		REQUIRE(testFileAutoDelete.CreateFileBacked((testFileName + "-autodelete").c_str(), SharedMemorySection::GetOffsetSize() + 4096, true) == SharedMemory::LOAD_SUCCESS);

		WHEN ("The shared memory is opened and the data modified") {
			std::cout << "Temporary Directory: " << testFilePath << std::endl << std::endl;

			SharedMemorySection section1(testFile, SharedMemorySection::GetOffsetSize() + 4096);
			SharedMemory testFile2;

			REQUIRE(!section1.ReadOnly());
			REQUIRE(testFile2.OpenFileBacked(testFileName.c_str()) == SharedMemory::LOAD_SUCCESS);

			THEN ("The memory in the created shared memory section matches the memory in the opened shared memory section") {
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

		WHEN ("A shared memory section is created with no size") {
			SharedMemorySection section2(testFile, 0);

			THEN ("The shared memory section is invalid") {
				REQUIRE(section2.Size() == 0);
				REQUIRE(!section2.Valid());
			}
		}

		testFile.Close();
		REQUIRE(SharedMemory::DeleteFileBacked(testFileName.c_str()));
	}

	GIVEN ("Some named shared memory") {
		SharedMemory testFile, testFileAutoDelete;

		REQUIRE(testFile.CreateNamed("Test 1", SharedMemorySection::GetOffsetSize() + 4096, false) == SharedMemory::LOAD_SUCCESS);
		REQUIRE(testFileAutoDelete.CreateNamed("Test 1-autodelete", SharedMemorySection::GetOffsetSize() + 4096, true) == SharedMemory::LOAD_SUCCESS);

		WHEN ("The shared memory is opened and the data modified") {
			SharedMemorySection section1(testFile, SharedMemorySection::GetOffsetSize() + 4096);
			SharedMemory testFile2;

			REQUIRE(!section1.ReadOnly());
			REQUIRE(testFile2.OpenNamed("Test 1") == SharedMemory::LOAD_SUCCESS);

			THEN ("The memory in the created shared memory section matches the memory in the opened shared memory section") {
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

		testFile.Close();
		REQUIRE(SharedMemory::DeleteNamed("Test 1"));
	}

	GIVEN ("Bad attempts to create or open shared memory") {
		WHEN ("The create or open calls are made") {
			SharedMemory testFile;

			THEN ("The requests fail") {
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

#include "smbb/IPAddress.h"

static void DumpAddress(const IPAddress &address) {
	IPAddress::String str;
	std::cout << address.ToURI(str, true) << " (" << address.GetInterfaceIndex() << ")" << std::endl;
}

static void DumpAddresses(const char *address, const char *port, bool bindable, IPAddressFamily family = FAMILY_UNSPECIFIED) {
	IPAddress ipAddresses[16];
	int found = IPAddress::Parse(ipAddresses, 16, address, port, bindable, family);

	for (int i = 0; i < found; i++)
		DumpAddress(ipAddresses[i]);

	if (found > 0)
		std::cout << std::endl;
}

#include "smbb/IPSocket.h"

#ifdef SMBB_NO_IPV6
#define IPV6 IPV4
#endif

SCENARIO ("IP Address Test", "[IPAddress]") {
	REQUIRE(IPSocket::Initialize()); // IPAddress tests still require initializing sockets

	GIVEN ("A set of IP addresses and ports") {
		WHEN ("Dumping information about the addresses and ports") {
			THEN ("The appropriate information in displayed") {
				// General information about interfaces
				DumpAddresses(NULL, "12034", false);
				DumpAddresses(NULL, "1234", true);
				DumpAddresses("", "12034", false);
				DumpAddresses("", "1234", true);
				DumpAddresses("localhost", "", false);
				DumpAddresses("localhost", "", true);
				DumpAddresses("localhost", "12034", false);
				DumpAddresses("localhost", "1234", true);
				DumpAddresses("ip6-localhost", "12034", false);
				DumpAddresses("ip6-localhost", "1234", true);
				DumpAddresses("localhost", NULL, false, IPV4);
				DumpAddresses("localhost", NULL, true, IPV4);

				IPAddress empty;
				IPAddress::String uri;

				REQUIRE(empty.GetFamily() == FAMILY_UNSPECIFIED);
				REQUIRE(empty.GetInterfaceIndex() == 0);
				REQUIRE(empty.GetLength() == 0);
				REQUIRE(empty.GetPort() == 0);
				REQUIRE(!empty.IsAny());
				REQUIRE(!empty.IsLoopback());
				REQUIRE(!empty.IsMulticast());
				REQUIRE(!empty.IsValid());
				REQUIRE(!empty.ToURI(uri));
			}
		}
	}

	IPSocket::Finish();
}

static void TestTCP(const char *address, const char *port, IPAddressFamily family = FAMILY_UNSPECIFIED) {
	IPAddress ipAddress;
	REQUIRE(IPAddress::Parse(&ipAddress, 1, address, port, true, family) > 0);

	// Setup the listening socket
	AutoCloseIPSocket listenSocket(ipAddress, TCP, IPSocket::OPEN_BIND_AND_LISTEN);

	// Setup the peer
	IPAddress peerAddress = IPAddress(ipAddress, listenSocket.GetAddress().GetPort());
	AutoCloseIPSocket peer;
	REQUIRE(peer.SetImmediateSend()->SetNonblocking()->Open(peerAddress, TCP, IPSocket::OPEN_AND_CONNECT));
	REQUIRE(!peer.Open(peerAddress, UDP, IPSocket::OPEN_AND_CONNECT));

	// Accept the connection
	IPAddress acceptedFrom;
	AutoCloseIPSocket accepted(listenSocket.Accept(&acceptedFrom));
	REQUIRE(accepted.IsValid());

	std::cout << "Accepted connection from ";
	DumpAddress(acceptedFrom);

	REQUIRE(peer.Connect(peerAddress) != IPSocket::CONNECT_FAILED);
	REQUIRE((accepted.SetImmediateSend() && accepted.SetNonblocking()));

	// Test sending data
	const char DATA[] = "This is a test string that is sent through the socket and should match what is received.";
	REQUIRE(accepted.Send(DATA, static_cast<IPSocket::DataLength>(sizeof(DATA))).GetResult() == sizeof(DATA));

	// Wait for data to arrive
	IPSocket::SelectSets sets;
	sets.AddSocket(peer, IPSocket::SELECT_CAN_READ);

	REQUIRE(sets.Wait(16000) > 0);
	REQUIRE(sets.TestSocket(peer, IPSocket::SELECT_CHECK_ALL) == IPSocket::SELECT_CAN_READ);

	// Receive data
	char receivedData[sizeof(DATA)];
	REQUIRE(peer.Receive(receivedData, static_cast<IPSocket::DataLength>(sizeof(DATA))).GetResult() == sizeof(DATA));
	REQUIRE(memcmp(DATA, receivedData, sizeof(DATA)) == 0);

#ifndef SMBB_NO_SOCKET_MSG
	// Test sending more data
	IPSocket::Buffer splitData[] = { IPSocket::Buffer(&DATA[0], 21), IPSocket::Buffer(&DATA[29], 59) };
	IPSocket::Message singleSplitData = IPSocket::Message(splitData, 2);
	IPSocket::MultiMessagePart multipleSplitData = IPSocket::MultiMessagePart(splitData, 2);
	const size_t SPLIT_DATA_SIZE = 21 + 59;

	REQUIRE((accepted.Send(singleSplitData).GetResult() == SPLIT_DATA_SIZE && accepted.SendMultiple(&multipleSplitData, 1).GetResult() == 1));

	// Wait for data to arrive
	sets.AddSocket(peer, IPSocket::SELECT_CAN_READ);

	REQUIRE(sets.Wait(16000) > 0);

	// Receive data
	uint8_t recvDataBlocks[4][40];
	IPSocket::Buffer recvSplitData[] = { IPSocket::Buffer(recvDataBlocks[0], 40), IPSocket::Buffer(recvDataBlocks[1], 40), IPSocket::Buffer(recvDataBlocks[2], 40), IPSocket::Buffer(recvDataBlocks[3], 40) };
	IPSocket::Message recvSingle = IPSocket::Message(recvSplitData, 2);
	IPSocket::MultiMessagePart recvMultiple = IPSocket::MultiMessagePart(&recvSplitData[2], 2);

	REQUIRE((peer.Receive(recvSingle).GetResult() == 80 && sets.Wait(10000) > 0 && peer.ReceiveMultiple(&recvMultiple, 1).GetResult() == 1));
	REQUIRE((memcmp(DATA, recvDataBlocks[0], 21) == 0 && memcmp(&DATA[29], &recvDataBlocks[0][21], 19) == 0 && memcmp(&DATA[48], recvDataBlocks[1], 40) == 0 &&
		memcmp(DATA, recvDataBlocks[2], 21) == 0 && memcmp(&DATA[29], &recvDataBlocks[2][21], 19) == 0 && memcmp(&DATA[48], recvDataBlocks[3], 40) == 0));
#endif
	std::cout << "Finished testing TCP for " << address << std::endl << std::endl;
	accepted.CloseTCPSend();
	peer.CloseTCPSend();
}

static bool TestMulticastUDP(const char *receiveAddress, const char *multicastAddress, const char *sendAddress, IPAddressFamily family = FAMILY_UNSPECIFIED) {
	IPAddress ipAddress;
	REQUIRE(IPAddress::Parse(&ipAddress, 1, receiveAddress, NULL, true, family) > 0);

	// Setup the read socket
	AutoCloseIPSocket readSocket(ipAddress, UDP, IPSocket::OPEN_AND_BIND);

	std::cout << "Reading on ";
	DumpAddress(readSocket.GetAddress());

	IPAddress multicast;
	REQUIRE(IPAddress::Parse(&multicast, 1, multicastAddress, NULL, false, family) > 0);
	REQUIRE(multicast.IsMulticast());
	std::cout << "Subscribing to ";
	DumpAddress(multicast);

	REQUIRE(readSocket.SetMulticastLoopback()->SubscribeToMulticastAddress(multicast));

	// Setup the sender
	IPAddress sendToAddress = IPAddress(multicast, readSocket.GetAddress().GetPort());
	AutoCloseIPSocket sendSocket;

	std::cout << "Sending to ";
	DumpAddress(sendToAddress);

	REQUIRE(sendSocket.Open(sendToAddress, UDP, IPSocket::OPEN_ONLY));

	(void)sendSocket.SetMulticastLoopback()->SetMulticastHops(3);

	if (sendAddress) {
		if (IPAddress::Parse(&ipAddress, 1, sendAddress, NULL, true, family) <= 0) {
			std::cerr << "Failed to parse local address " << sendAddress << std::endl << std::endl;
			return false;
		}

		if (sendSocket.SetMulticastSendInterface(ipAddress))
			std::cout << "Using send address " << sendAddress << std::endl;
		else
			std::cerr << "Failed to use local interface " << sendAddress << std::endl;
	}

	// Test sending data
	const char DATA[] = "This is a test string that is sent through the socket and should match what is received.";
	IPSocket::SelectSets recvSets;
	size_t i = 0;

	for (i = 0; i < 10 && recvSets.Wait(16000) <= 0; i++) {
		REQUIRE(sendSocket.Send(DATA, static_cast<IPSocket::DataLength>(sizeof(DATA)), sendToAddress).GetResult() == sizeof(DATA));

		// Wait for data to arrive
		recvSets.AddSocket(readSocket, IPSocket::SELECT_CAN_READ);
	}

	// If this fails, we may not be able to receive loopback multicast messages on this address.
	if (i >= 10)
		return false;

	// Receive data
	char receivedData[sizeof(DATA)];

	REQUIRE(readSocket.Receive(receivedData, static_cast<IPSocket::DataLength>(sizeof(receivedData))).GetResult() == sizeof(receivedData));
	REQUIRE(memcmp(DATA, receivedData, sizeof(DATA)) == 0);

#ifndef SMBB_NO_SOCKET_MSG
	// Test sending too much data
	const char TOO_MUCH_DATA[33000] = { };
	IPSocket::Buffer tooMuchDataBuffers[2] = { IPSocket::Buffer(TOO_MUCH_DATA, sizeof(TOO_MUCH_DATA)), IPSocket::Buffer(TOO_MUCH_DATA, sizeof(TOO_MUCH_DATA)) };
	IPSocket::MessageResult tooMuchDataError = sendSocket.Send(IPSocket::Message(tooMuchDataBuffers, 2, &sendToAddress));

	REQUIRE(tooMuchDataError.GetError() == IP_SOCKET_ERROR(MSGSIZE));

	for (i = 0; i < 10 && recvSets.Wait(16000) <= 0; i++) {
		REQUIRE(sendSocket.Send(DATA, static_cast<IPSocket::DataLength>(sizeof(DATA)), sendToAddress).GetResult() == sizeof(DATA));

		// Wait for data to arrive
		recvSets.AddSocket(readSocket, IPSocket::SELECT_CAN_READ);
	}

	REQUIRE(i < 10);

	// Receive data
	IPAddress receiveFromAddress;
	IPSocket::Buffer msgSizeErrorBuffer = IPSocket::Buffer(receivedData, sizeof(receivedData) - 1);
	IPSocket::MessageResult msgSizeError = readSocket.Receive(IPSocket::Message(&msgSizeErrorBuffer, 1, &receiveFromAddress));

	REQUIRE(msgSizeError.GetResult() == sizeof(receivedData) - 1);
	REQUIRE(msgSizeError.HasSizeError());
	REQUIRE(sendSocket.GetAddress().GetPort() == receiveFromAddress.GetPort());

	REQUIRE(memcmp(DATA, receivedData, sizeof(receivedData) - 1) == 0);
#endif
	std::cout << "Finished testing multicast for " << multicastAddress << std::endl << std::endl;
	return true;
}

static bool TestSocketOptions(IPAddressFamily family, IPProtocol protocol) {
	IPAddress ipAddress = IPAddress::Loopback(family);
	IPAddress::String uri;
	AutoCloseIPSocket listener(ipAddress, protocol, IPSocket::OPEN_AND_BIND);

	if (protocol == TCP) {
		REQUIRE(listener.Listen(1));
		std::cout << "Testing " << listener.GetAddress().ToURI(uri, true) << " (TCP)" << std::endl;
	}
	else
		std::cout << "Testing " << listener.GetAddress().ToURI(uri, true) << " (UDP)" << std::endl;

	// Prepare the socket
	AutoCloseIPSocket socket(family, protocol);
	AutoCloseIPSocket accepted;

	REQUIRE(socket.SetNonblocking());
	REQUIRE(socket.Connect(listener.GetAddress()) != IPSocket::CONNECT_FAILED);

	if (protocol == TCP) {
		AutoCloseIPSocket temp(listener.Accept());
		accepted.Swap(temp);

		IPSocket::SelectSets sets;
		sets.AddSocket(socket, IPSocket::SELECT_IS_CONNECTED);
		REQUIRE(sets.Wait(100000) == 1);
	}

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

	if (protocol == TCP)
		socket.CloseTCPSend();

	return true;
}

#include <type_traits>

SCENARIO ("IP Socket Test", "[IPSocket]") {
	REQUIRE(IPSocket::Initialize());

#ifndef SMBB_NO_SOCKET_MSG
	REQUIRE(std::is_standard_layout<IPSocket::Buffer>::value);
	REQUIRE(std::is_standard_layout<IPSocket::Message>::value);
	REQUIRE(std::is_standard_layout<IPSocket::MultiMessagePart>::value);
#endif
	IPAddress ipAddress;
	REQUIRE(IPAddress::Parse(&ipAddress, 1, "127.0.0.1", "http", true, IPV4) >= 0);
	REQUIRE(IPAddress::Parse(&ipAddress, 1, "127.0.0.1", "ftp", true, IPV4) >= 0);
	REQUIRE(IPAddress::Parse(&ipAddress, 1, "127.0.0.1", "8080", true, IPV4) >= 0);
	REQUIRE(IPAddress::Parse(&ipAddress, 1, "127.0.0.1", "22", true, IPV4) >= 0);

	GIVEN ("A set of IP addresses and ports") {
		WHEN ("Testing TCP connections using the addresses and ports") {
			THEN ("The TCP test succeeds") {
#ifndef SMBB_NO_SOCKET_MSG
				std::cout << "Has multiple receive: " << IPSocket::HasNativeReceiveMultiple() << std::endl;
				std::cout << "Has multiple send: " << IPSocket::HasNativeSendMultiple() << std::endl << std::endl;
#endif
				// Test TCP
				TestTCP("127.0.0.1", NULL);
				TestTCP("127.0.0.1", NULL, IPV4);
#ifndef SMBB_NO_IPV6
				TestTCP("::1", NULL, IPV6);
#endif
			}
		}

		WHEN ("Testing multicast UDP connections using the addresses and ports") {
			THEN ("The multicast UDP test succeeds") {
				// Test Multicast
				REQUIRE(TestMulticastUDP(NULL, "239.192.2.3", NULL, IPV4));
#ifndef SMBB_NO_IPV6
				REQUIRE(TestMulticastUDP(NULL, "ff08::0001", NULL, IPV6));

				IPAddress ipAddresses[8];
				IPAddress::String ipString;
				IPSocket::ResultLength found = IPAddress::Parse(ipAddresses, 8, "", 0, false, IPV6);

				for (IPSocket::ResultLength i = 0; i < found; i++)
					TestMulticastUDP(NULL, "ff08::0001", ipAddresses[i].ToURI(ipString), IPV6);
#endif
			}
		}

		WHEN ("Testing socket options using the live addresses and ports") {
			THEN ("The socket options are displayed") {
				// Test Options
				REQUIRE(TestSocketOptions(IPV4, TCP));
				REQUIRE(TestSocketOptions(IPV4, UDP));
#ifndef SMBB_NO_IPV6
				REQUIRE(TestSocketOptions(IPV6, TCP));
				REQUIRE(TestSocketOptions(IPV6, UDP));
#endif
			}
		}
	}

	IPSocket::Finish();
}

SCENARIO ("Select Test", "[IPSocket]") {
	REQUIRE(IPSocket::Initialize());

	GIVEN ("A loopback address") {
		IPAddress ipv4Loopback = IPAddress::Loopback(IPV4);
		IPAddress ipv6Loopback = IPAddress::Loopback(IPV6);

		REQUIRE(ipv4Loopback.IsLoopback());
		REQUIRE(ipv6Loopback.IsLoopback());

		WHEN ("Connecting to a socket") {
			IPSocket::SelectSets sets;

			REQUIRE(sets.Wait(10000) == 0);

			AutoCloseIPSocket localIPv4Socket(ipv4Loopback, TCP, IPSocket::OPEN_AND_BIND);
			AutoCloseIPSocket localIPv6Socket(ipv6Loopback, TCP, IPSocket::OPEN_AND_BIND);

			AutoCloseIPSocket connectToIPv4(IPV4, TCP);
			AutoCloseIPSocket connectToIPv6(IPV6, TCP);

			AND_WHEN ("The socket is not listening") {
				THEN ("Waiting on the select sets identify the connection as failed") {
					REQUIRE(sets.Wait(100000) == 0);

					REQUIRE(connectToIPv4.SetNonblocking()->Connect(localIPv4Socket.GetAddress()) != IPSocket::CONNECT_SUCCESS);
					REQUIRE(connectToIPv6.SetNonblocking()->Connect(localIPv6Socket.GetAddress()) != IPSocket::CONNECT_SUCCESS);

					sets.AddSocket(connectToIPv4, IPSocket::SELECT_IS_CONNECTED | IPSocket::SELECT_CONNECT_FAILED);
					REQUIRE(sets.Wait(10000000) == 1); // Wait up to 10 seconds
					REQUIRE(sets.TestSocket(connectToIPv4, IPSocket::SELECT_IS_CONNECTED | IPSocket::SELECT_CONNECT_FAILED) == IPSocket::SELECT_CONNECT_FAILED);
					sets.RemoveSocket(connectToIPv4, IPSocket::SELECT_CONNECT_FAILED);

					sets.AddSocket(connectToIPv6, IPSocket::SELECT_IS_CONNECTED | IPSocket::SELECT_CONNECT_FAILED);
					REQUIRE(sets.Wait(10000000) == 1); // Wait up to 10 seconds
					REQUIRE(sets.TestSocket(connectToIPv6, IPSocket::SELECT_IS_CONNECTED | IPSocket::SELECT_CONNECT_FAILED) == IPSocket::SELECT_CONNECT_FAILED);
					sets.RemoveSocket(connectToIPv6, IPSocket::SELECT_CONNECT_FAILED);
				}
			}

			AND_WHEN ("The socket is listening") {
				THEN ("Waiting on the select sets identify the listening socket as ready to accept and the connecting socket as ready to write") {
					REQUIRE(localIPv4Socket.Listen(50));
					REQUIRE(localIPv6Socket.Listen(50));

					connectToIPv4.SetNonblocking()->Connect(localIPv4Socket.GetAddress());
					connectToIPv6.SetNonblocking()->Connect(localIPv6Socket.GetAddress());

					// Check that the connection is accepted
					sets.AddSocket(localIPv4Socket, IPSocket::SELECT_CHECK_ALL);
					REQUIRE(sets.Wait(100000) == 1); // Wait up to a tenth of a second
					REQUIRE(sets.TestSocket(localIPv4Socket, IPSocket::SELECT_CHECK_ALL) == IPSocket::SELECT_CAN_ACCEPT);
					sets.RemoveSocket(localIPv4Socket, IPSocket::SELECT_CAN_ACCEPT);

					sets.AddSocket(localIPv6Socket, IPSocket::SELECT_CHECK_ALL);
					REQUIRE(sets.Wait(100000) == 1); // Wait up to a tenth of a second
					REQUIRE(sets.TestSocket(localIPv6Socket, IPSocket::SELECT_CHECK_ALL) == IPSocket::SELECT_CAN_ACCEPT);
					sets.RemoveSocket(localIPv6Socket, IPSocket::SELECT_CAN_ACCEPT);

					AutoCloseIPSocket acceptedIPv4(localIPv4Socket.Accept());
					AutoCloseIPSocket acceptedIPv6(localIPv6Socket.Accept());

					// Check that the connection is successful and that the accepted socket can write
					sets.AddSocket(connectToIPv4, IPSocket::SELECT_IS_CONNECTED | IPSocket::SELECT_CONNECT_FAILED);
					REQUIRE(sets.Wait(100000) == 1); // Wait up to a tenth of a second
					REQUIRE(sets.TestSocket(connectToIPv4, IPSocket::SELECT_IS_CONNECTED | IPSocket::SELECT_CONNECT_FAILED) == IPSocket::SELECT_IS_CONNECTED);
					sets.RemoveSocket(connectToIPv4, IPSocket::SELECT_IS_CONNECTED);

					sets.AddSocket(connectToIPv6, IPSocket::SELECT_IS_CONNECTED | IPSocket::SELECT_CONNECT_FAILED);
					REQUIRE(sets.Wait(100000) == 1); // Wait up to a tenth of a second
					REQUIRE(sets.TestSocket(connectToIPv6, IPSocket::SELECT_IS_CONNECTED | IPSocket::SELECT_CONNECT_FAILED) == IPSocket::SELECT_IS_CONNECTED);
					sets.RemoveSocket(connectToIPv6, IPSocket::SELECT_IS_CONNECTED);

					sets.AddSocket(acceptedIPv4, IPSocket::SELECT_CAN_READ | IPSocket::SELECT_CAN_WRITE);
					REQUIRE(sets.Wait(100000) == 1); // Wait up to a tenth of a second
					REQUIRE(sets.TestSocket(acceptedIPv4, IPSocket::SELECT_CAN_READ | IPSocket::SELECT_CAN_WRITE) == IPSocket::SELECT_CAN_WRITE);
					sets.RemoveSocket(acceptedIPv4, IPSocket::SELECT_CAN_WRITE);

					sets.AddSocket(acceptedIPv6, IPSocket::SELECT_CAN_READ | IPSocket::SELECT_CAN_WRITE);
					REQUIRE(sets.Wait(100000) == 1); // Wait up to a tenth of a second
					REQUIRE(sets.TestSocket(acceptedIPv6, IPSocket::SELECT_CAN_READ | IPSocket::SELECT_CAN_WRITE) == IPSocket::SELECT_CAN_WRITE);
					sets.RemoveSocket(acceptedIPv6, IPSocket::SELECT_CAN_WRITE);

					AND_WHEN ("There is no data to read") {
						THEN ("Waiting on the select sets returns no results") {
							sets.AddSocket(acceptedIPv4, IPSocket::SELECT_CAN_READ);
							sets.AddSocket(connectToIPv4, IPSocket::SELECT_CAN_READ);
							REQUIRE(sets.Wait(100000) == 0); // Wait up to a tenth of a second
							REQUIRE(sets.TestSocket(acceptedIPv4, IPSocket::SELECT_CAN_READ) == 0);
							REQUIRE(sets.TestSocket(connectToIPv4, IPSocket::SELECT_CAN_READ) == 0);

							sets.AddSocket(acceptedIPv6, IPSocket::SELECT_CAN_READ);
							sets.AddSocket(connectToIPv6, IPSocket::SELECT_CAN_READ);
							REQUIRE(sets.Wait(100000) == 0); // Wait up to a tenth of a second
							REQUIRE(sets.TestSocket(acceptedIPv6, IPSocket::SELECT_CAN_READ) == 0);
							REQUIRE(sets.TestSocket(connectToIPv6, IPSocket::SELECT_CAN_READ) == 0);
						}
					}

					AND_WHEN ("There is data to read") {
						char data[] = "Test data";

						acceptedIPv4.Send(data, sizeof(data));
						acceptedIPv6.Send(data, sizeof(data));

						THEN ("Waiting on the select sets returns ready to read") {
							sets.AddSocket(acceptedIPv4, IPSocket::SELECT_CAN_READ);
							sets.AddSocket(connectToIPv4, IPSocket::SELECT_CAN_READ);
							REQUIRE(sets.Wait(100000) == 1); // Wait up to a tenth of a second
							REQUIRE(sets.TestSocket(acceptedIPv4, IPSocket::SELECT_CAN_READ) == 0);
							REQUIRE(sets.TestSocket(connectToIPv4, IPSocket::SELECT_CAN_READ) == IPSocket::SELECT_CAN_READ);
							sets.RemoveSocket(connectToIPv4, IPSocket::SELECT_CAN_READ);

							sets.AddSocket(acceptedIPv6, IPSocket::SELECT_CAN_READ);
							sets.AddSocket(connectToIPv6, IPSocket::SELECT_CAN_READ);
							REQUIRE(sets.Wait(100000) == 1); // Wait up to a tenth of a second
							REQUIRE(sets.TestSocket(acceptedIPv6, IPSocket::SELECT_CAN_READ) == 0);
							REQUIRE(sets.TestSocket(connectToIPv6, IPSocket::SELECT_CAN_READ) == IPSocket::SELECT_CAN_READ);
							sets.RemoveSocket(connectToIPv6, IPSocket::SELECT_CAN_READ);
						}
					}
				}
			}
		}
	}

	IPSocket::Finish();
}

#ifndef SMBB_NO_POLL
SCENARIO ("Poll Test", "[IPSocket]") {
	REQUIRE(IPSocket::Initialize());
	REQUIRE(std::is_standard_layout<IPSocket::PollItem>::value);

	GIVEN ("A loopback address") {
		IPAddress ipv4Loopback = IPAddress::Loopback(IPV4);
		IPAddress ipv6Loopback = IPAddress::Loopback(IPV6);

		REQUIRE(ipv4Loopback.IsLoopback());
		REQUIRE(ipv6Loopback.IsLoopback());

		WHEN ("Connecting to a socket") {
			IPSocket::PollItem pollSet[4];

			REQUIRE(IPSocket::Poll(pollSet, 0, 10) == 0);

			AutoCloseIPSocket localIPv4Socket(ipv4Loopback, TCP, IPSocket::OPEN_AND_BIND);
			AutoCloseIPSocket localIPv6Socket(ipv6Loopback, TCP, IPSocket::OPEN_AND_BIND);

			AutoCloseIPSocket connectToIPv4(IPV4, TCP);
			AutoCloseIPSocket connectToIPv6(IPV6, TCP);

			AND_WHEN ("The socket is not listening") {
				THEN ("Polling identifies the connection as failed") {
					REQUIRE(connectToIPv4.SetNonblocking()->Connect(localIPv4Socket.GetAddress()) != IPSocket::CONNECT_SUCCESS);
					REQUIRE(connectToIPv6.SetNonblocking()->Connect(localIPv6Socket.GetAddress()) != IPSocket::CONNECT_SUCCESS);

					pollSet[0] = IPSocket::PollItem::Make(connectToIPv4, IPSocket::POLL_IS_CONNECTED);
					REQUIRE(pollSet[0].GetMonitor() == IPSocket::POLL_IS_CONNECTED);
					REQUIRE(IPSocket::Poll(pollSet, 1, 2000) >= 0); // Wait up to 3 seconds
					REQUIRE(pollSet[0].HasFailedConnectionResult());

					pollSet[0].Disable();
					REQUIRE(!pollSet[0].IsEnabled());
					REQUIRE(IPSocket::Poll(pollSet, 1, 100) == 0);
					REQUIRE(!pollSet[0].HasFailedConnectionResult());

					pollSet[0].Enable();
					REQUIRE(pollSet[0].IsEnabled());
					pollSet[1] = IPSocket::PollItem::Make(connectToIPv6, IPSocket::POLL_IS_CONNECTED);
					REQUIRE(IPSocket::Poll(pollSet, 2, 0) >= 0); // Wait up to a second
					REQUIRE(pollSet[0].HasFailedConnectionResult());
					REQUIRE(pollSet[1].HasFailedConnectionResult());
				}
			}

			AND_WHEN ("The socket is listening") {
				THEN ("Polling identifies the listening socket as ready to accept and the connecting socket as ready to write") {
					REQUIRE(localIPv4Socket.Listen(50));
					REQUIRE(localIPv6Socket.Listen(50));

					connectToIPv4.SetNonblocking()->Connect(localIPv4Socket.GetAddress());
					connectToIPv6.SetNonblocking()->Connect(localIPv6Socket.GetAddress());

					// Check that the connection is accepted
					pollSet[0] = IPSocket::PollItem::Make(connectToIPv4, IPSocket::POLL_IS_CONNECTED);
					REQUIRE(IPSocket::Poll(pollSet, 1, 100) == 1); // Wait up to a tenth of a second
					REQUIRE((pollSet[0].GetResult() & IPSocket::POLL_ERROR) == 0);
					REQUIRE(pollSet[0].HasResult(IPSocket::POLL_IS_CONNECTED));

					pollSet[0] = IPSocket::PollItem::Make(connectToIPv6, IPSocket::POLL_IS_CONNECTED);
					REQUIRE(IPSocket::Poll(pollSet, 1, 100) == 1); // Wait up to a tenth of a second
					REQUIRE((pollSet[0].GetResult() & IPSocket::POLL_ERROR) == 0);
					REQUIRE(pollSet[0].HasResult(IPSocket::POLL_IS_CONNECTED));

					AutoCloseIPSocket acceptedIPv4(localIPv4Socket.Accept());
					AutoCloseIPSocket acceptedIPv6(localIPv6Socket.Accept());

					// Check that the connection is successful and that both sides can write
					pollSet[0] = IPSocket::PollItem::Make(connectToIPv4, IPSocket::POLL_CAN_READ | IPSocket::POLL_CAN_WRITE);
					REQUIRE(IPSocket::Poll(pollSet, 1, 100) == 1); // Wait up to a tenth of a second
					REQUIRE(pollSet[0].HasResult(IPSocket::POLL_CAN_WRITE));

					pollSet[0] = IPSocket::PollItem::Make(connectToIPv6, IPSocket::POLL_CAN_READ | IPSocket::POLL_CAN_WRITE);
					REQUIRE(IPSocket::Poll(pollSet, 1, 100) == 1); // Wait up to a tenth of a second
					REQUIRE(pollSet[0].HasResult(IPSocket::POLL_CAN_WRITE));

					pollSet[0] = IPSocket::PollItem::Make(acceptedIPv4, IPSocket::POLL_CAN_READ | IPSocket::POLL_CAN_WRITE);
					pollSet[1] = IPSocket::PollItem::Make(acceptedIPv6, IPSocket::POLL_CAN_READ | IPSocket::POLL_CAN_WRITE);
					REQUIRE(IPSocket::Poll(pollSet, 2, 100) == 2); // Wait up to a tenth of a second
					REQUIRE(pollSet[0].HasResult(IPSocket::POLL_CAN_WRITE));
					REQUIRE(pollSet[1].HasResult(IPSocket::POLL_CAN_WRITE));

					AND_WHEN ("There is no data to read") {
						THEN ("Polling times out") {
							pollSet[0] = IPSocket::PollItem::Make(connectToIPv4, IPSocket::POLL_CAN_READ);
							pollSet[1] = IPSocket::PollItem::Make(connectToIPv6, IPSocket::POLL_CAN_READ);
							pollSet[2] = IPSocket::PollItem::Make(acceptedIPv4, IPSocket::POLL_CAN_READ);
							pollSet[3] = IPSocket::PollItem::Make(acceptedIPv6, IPSocket::POLL_CAN_READ);
							REQUIRE(IPSocket::Poll(pollSet, 4, 100) == 0); // Wait up to a tenth of a second
						}
					}

					AND_WHEN ("There is data to read") {
						char data[] = "Test data";

						acceptedIPv4.Send(data, sizeof(data));
						acceptedIPv6.Send(data, sizeof(data));

						pollSet[0] = IPSocket::PollItem::Make(connectToIPv4, IPSocket::POLL_CAN_READ);
						pollSet[1] = IPSocket::PollItem::Make(connectToIPv6, IPSocket::POLL_CAN_READ);
						pollSet[2] = IPSocket::PollItem::Make(acceptedIPv4, IPSocket::POLL_CAN_READ);
						pollSet[3] = IPSocket::PollItem::Make(acceptedIPv6, IPSocket::POLL_CAN_READ);

						THEN ("Waiting on the select sets returns ready to read") {
							pollSet[1].Disable();
							REQUIRE(IPSocket::Poll(pollSet, 4, 100) == 1); // Wait up to a tenth of a second
							REQUIRE(pollSet[0].HasResult(IPSocket::POLL_CAN_READ));
							REQUIRE(pollSet[2].GetResult() == 0);

							pollSet[0].Disable();
							pollSet[1].Enable();
							REQUIRE(IPSocket::Poll(pollSet, 4, 100) == 1); // Wait up to a tenth of a second
							REQUIRE(pollSet[1].HasResult(IPSocket::POLL_CAN_READ));
							REQUIRE(pollSet[3].GetResult() == 0);
						}
					}

					AND_WHEN ("The sockets disconnect") {
						THEN ("Polling identifies the disconnect") {
							connectToIPv4.CloseTCPSend();
							connectToIPv6.CloseTCPSend();

							pollSet[0] = IPSocket::PollItem::Make(acceptedIPv4, IPSocket::POLL_CAN_READ);
							REQUIRE(IPSocket::Poll(pollSet, 1, 100) == 1); // Wait up to a tenth of a second
							REQUIRE(pollSet[0].HasResult(IPSocket::POLL_CAN_READ | IPSocket::POLL_DISCONNECTING));

							pollSet[0] = IPSocket::PollItem::Make(acceptedIPv6, IPSocket::POLL_CAN_READ);
							REQUIRE(IPSocket::Poll(pollSet, 1, 100) == 1); // Wait up to a tenth of a second
							REQUIRE(pollSet[0].HasResult(IPSocket::POLL_CAN_READ | IPSocket::POLL_DISCONNECTING));
						}
					}
				}
			}
		}
	}

	IPSocket::Finish();
}
#endif
