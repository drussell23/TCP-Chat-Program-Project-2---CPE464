/******************************************************************************
 * Name: Derek J. Russell
 * 	
 * This server acts as a router/forwarder for chat clients. It accepts client
 * connections, registers them (via flag 1), maintains a dynamic table mapping
 * client handles to socket descriptors, and forwards packets between clients.
 *
 * It handles:
 *   - Flag 1: Client registration.
 *   - Flag 4: Broadcast messages.
 *   - Flag 5: Direct messages.
 *   - Flag 8: Client exit.
 *   - Flag 10: List requests.
 *
 * For list requests, it sends:
 *   - Flag 0x0B: 4-byte number of handles.
 *   - Flag 0x0C: One packet per handle.
 *   - Flag 0x0D: End-of-list marker.
 *
 * It uses poll (via pollLib) to monitor sockets.
 *****************************************************************************/

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <sstream> // For hex dump logging

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "safeUtil.h"
#include "pollLib.h"
#include "networks.h"
#include "PDU_Send_And_Recv.h"
#include "Dynamic_Array.h" // Your dynamic handle table API

// Define a namespace for chat constants.
namespace ChatConstants
{
	constexpr int MaxNameLen = MAXIMUM_CHARACTERS; // Defined in Dynamic_Array.h (e.g., 100)
}

#define MAXBUF 1024
#define DEBUG_FLAG 1

// Logging macros.
#if DEBUG_FLAG
#define LOG_DEBUG(msg) std::cout << "[DEBUG] " << msg << std::endl
#else
#define LOG_DEBUG(msg)
#endif

#define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl
#define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl

// Flag definitions.
#define CLIENT_INIT_PACKET_TO_SERVER 1		// Registration packet from client
#define CONFIRM_GOOD_HANDLE 2				// Confirmation sent to client
#define ERROR_ON_INIT_PACKET 3				// Error on registration
#define BROADCAST_PACKET 4					// Broadcast message
#define MESSAGE_PACKET 5					// Direct message packet
#define CLIENT_TO_SERVER_EXIT 8				// Exit notification from client
#define CLIENT_TO_SERVER_LIST_OF_HANDLES 10 // List request from client

// List response flags.
#define LIST_RESPONSE_NUM 0x0B	  // Server sends: 4-byte number of handles
#define LIST_RESPONSE_HANDLE 0x0C // Server sends: one handle (1 byte length + handle)
#define LIST_RESPONSE_END 0x0D	  // Server sends: end-of-list marker
#define EXIT_ACK 9				  // Exit ACK from server

// VALID_ZERO_PAYLOAD used by recvBuf to indicate a valid packet with no payload.
#define VALID_ZERO_PAYLOAD -2

using namespace std;

// Global dynamic array for client handle management.
Dynamic_Array clientTable;

// Helper function: Produce a hex dump string from a buffer.
std::string hexDump(uint8_t *buffer, int length)
{
	std::stringstream ss;
	ss << std::hex << std::setfill('0');
	for (int i = 0; i < length; i++)
	{
		ss << std::setw(2) << static_cast<int>(buffer[i]) << " ";
	}
	return ss.str();
}

// Function prototypes.
int checkArgs(int argc, char *argv[]);
void talk_to_clients(int serverSocket);
void processNewClient(int serverSocket);
void processClientPacket(int clientSocket);
void processListRequest(int clientSocket);
void forwardBroadcast(int senderSocket, uint8_t *payload, int payloadLen);
void forwardDirectMessage(int senderSocket, uint8_t *payload, int payloadLen);
void processClientExit(int clientSocket);
void sendErrorForInvalidHandle(int senderSocket, const char *destHandle);
void removeClientBySocket(int clientSocket);
bool verifyPacketLength(int receivedLen, int expectedMin);
bool safeSend(int socketNum, uint8_t *payload, int payloadLen, int flag);

// Cleanup a client connection gracefully.
void cleanupClient(int clientSocket)
{
	removeClientBySocket(clientSocket);
	removeFromPollSet(clientSocket);
	close(clientSocket);
	LOG_INFO("Cleaned up client on socket " << clientSocket);
}

int main(int argc, char *argv[])
{
	int portNumber = checkArgs(argc, argv);
	int serverSocket = tcpServerSetup(portNumber);
	setupPollSet();
	addToPollSet(serverSocket);

	LOG_INFO("Server is using port " << portNumber);
	talk_to_clients(serverSocket);
	close(serverSocket);
	return 0;
}

// Checks command-line arguments and returns port number.
int checkArgs(int argc, char *argv[])
{
	int portNumber = 0;
	
	if (argc > 2)
	{
		fprintf(stderr, "Usage: %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	return portNumber;
}

// Main loop: poll for new connections or activity on client sockets.
void talk_to_clients(int serverSocket)
{
	int readySocket;
	while (true)
	{
		readySocket = pollCall(-1); // Blocks until an FD is ready.
		if (readySocket == serverSocket)
		{
			processNewClient(serverSocket);
		}
		else
		{
			processClientPacket(readySocket);
		}
	}
}

// Accept a new client connection and process its registration packet.
void processNewClient(int serverSocket)
{
	int clientSocket = tcpAccept(serverSocket, DEBUG_FLAG);
	PDU_Send_And_Recv pdu;
	uint8_t buffer[MAXBUF] = {0};
	int flag;
	int len = pdu.recvBuf(clientSocket, buffer, &flag);

	LOG_DEBUG("Received registration packet on socket " << clientSocket
														<< " with flag " << flag << " and length " << len
														<< ". Data: " << hexDump(buffer, len));

	if (!verifyPacketLength(len, 1) || flag != CLIENT_INIT_PACKET_TO_SERVER)
	{
		LOG_ERROR("Invalid registration packet received. Expected flag "
				  << CLIENT_INIT_PACKET_TO_SERVER << " but got flag " << flag);
		cleanupClient(clientSocket);
		return;
	}

	uint8_t handleLen = buffer[0];
	LOG_DEBUG("Registration packet reports handle length: " << (int)handleLen);

	if (handleLen == 0 || handleLen > ChatConstants::MaxNameLen || len < (1 + handleLen))
	{
		pdu.sendBuf(clientSocket, nullptr, 0, ERROR_ON_INIT_PACKET);
		LOG_ERROR("Invalid handle length in registration packet. Received handleLen="
				  << (int)handleLen << ", expected between 1 and " << ChatConstants::MaxNameLen);
		cleanupClient(clientSocket);
		return;
	}

	char handle[ChatConstants::MaxNameLen + 1] = {0};
	memcpy(handle, buffer + 1, handleLen);
	handle[handleLen] = '\0';
	LOG_DEBUG("Parsed handle: " << handle);

	if (clientTable.getSocketForHandle(handle) != -1)
	{
		pdu.sendBuf(clientSocket, nullptr, 0, ERROR_ON_INIT_PACKET);
		LOG_ERROR("Duplicate handle (" << handle << ") registration attempt on socket " << clientSocket);
		cleanupClient(clientSocket);
		return;
	}

	Handling newHandle;
	newHandle.handleLength = handleLen;
	memcpy(newHandle.handle, handle, handleLen);
	newHandle.handle[handleLen] = '\0';

	if (clientTable.addElement(newHandle, clientSocket) < 0)
	{
		pdu.sendBuf(clientSocket, nullptr, 0, ERROR_ON_INIT_PACKET);
		LOG_ERROR("Failed to add handle (" << handle << ") to dynamic table.");
		cleanupClient(clientSocket);
		return;
	}

	// Confirm registration.
	pdu.sendBuf(clientSocket, nullptr, 0, CONFIRM_GOOD_HANDLE);
	addToPollSet(clientSocket);
	LOG_INFO("New client registered: " << handle << " on socket " << clientSocket);
}

// Processes a packet from an already connected client.
void processClientPacket(int clientSocket)
{
	PDU_Send_And_Recv pdu;
	uint8_t buffer[MAXBUF] = {0};
	int flag;
	int len = pdu.recvBuf(clientSocket, buffer, &flag);

	LOG_DEBUG("Received packet on socket " << clientSocket
										   << " with flag " << flag << " and length " << len
										   << ". Data: " << hexDump(buffer, (len < 16 ? len : 16)) << "...");

	// Only treat negative values (other than VALID_ZERO_PAYLOAD) as errors.
	if (len < 0 && len != VALID_ZERO_PAYLOAD)
	{
		LOG_INFO("Client on socket " << clientSocket << " terminated (len < 0).");
		cleanupClient(clientSocket);
		return;
	}
	// For a valid zero payload, set len to 0 for further processing.
	if (len == VALID_ZERO_PAYLOAD)
	{
		LOG_DEBUG("Received valid zero-length payload with flag=" << flag);
		len = 0;
	}

	switch (flag)
	{
	case BROADCAST_PACKET:
		forwardBroadcast(clientSocket, buffer, len);
		break;
	case MESSAGE_PACKET:
		forwardDirectMessage(clientSocket, buffer, len);
		break;
	case CLIENT_TO_SERVER_EXIT:
		LOG_DEBUG("Processing exit packet from socket " << clientSocket);
		processClientExit(clientSocket);
		break;
	case CLIENT_TO_SERVER_LIST_OF_HANDLES: // flag 10 (0x0A)
		LOG_DEBUG("Processing list request from socket " << clientSocket);
		processListRequest(clientSocket);
		break;
	default:
		LOG_ERROR("Unknown flag " << flag << " received from socket " << clientSocket
								  << ". Data: " << hexDump(buffer, len));
		break;
	}
}

// For broadcast messages, forward the packet to all clients except the sender.
void forwardBroadcast(int senderSocket, uint8_t *payload, int payloadLen)
{
	if (payloadLen < 1)
	{
		LOG_ERROR("Broadcast packet is too short.");
		return;
	}
	uint8_t senderLen = payload[0];
	if (payloadLen < (1 + senderLen))
	{
		LOG_ERROR("Broadcast packet length inconsistent with sender handle length.");
		return;
	}
	char sender[ChatConstants::MaxNameLen + 1] = {0};
	memcpy(sender, payload + 1, senderLen);
	sender[senderLen] = '\0';

	PDU_Send_And_Recv pdu;
	int cap = clientTable.getCapacity();
	Entry_Handle_Table *arr = clientTable.getArray();
	for (int i = 0; i < cap; i++)
	{
		if (arr[i].handle.handleLength != 0 && arr[i].socketNumber != senderSocket)
		{
			if (!safeSend(arr[i].socketNumber, payload, payloadLen, BROADCAST_PACKET))
				LOG_ERROR("Failed to forward broadcast to socket " << arr[i].socketNumber);
		}
	}
	LOG_INFO("Broadcast message from " << sender << " forwarded.");
}

// For direct messages, parse destination handles and forward the packet accordingly.
void forwardDirectMessage(int senderSocket, uint8_t *payload, int payloadLen)
{
	int offset = 0;
	if (payloadLen < 1)
	{
		LOG_ERROR("Direct message packet is too short.");
		return;
	}
	uint8_t senderLen = payload[offset++];
	if (payloadLen < (1 + senderLen + 1))
	{
		LOG_ERROR("Direct message packet too short for sender and destination count.");
		return;
	}
	char sender[ChatConstants::MaxNameLen + 1] = {0};
	memcpy(sender, payload + offset, senderLen);
	sender[senderLen] = '\0';
	offset += senderLen;
	uint8_t numDest = payload[offset++];
	LOG_INFO("Direct message from " << sender << " to " << (int)numDest << " destination(s).");

	PDU_Send_And_Recv pdu;
	for (int i = 0; i < numDest; i++)
	{
		if (offset >= payloadLen)
		{
			LOG_ERROR("Direct message packet truncated while reading destination handles.");
			return;
		}
		uint8_t destLen = payload[offset++];
		if (offset + destLen > payloadLen)
		{
			LOG_ERROR("Direct message packet length inconsistent with destination handle length.");
			return;
		}
		char dest[ChatConstants::MaxNameLen + 1] = {0};
		memcpy(dest, payload + offset, destLen);
		dest[destLen] = '\0';
		offset += destLen;
		int destSocket = clientTable.getSocketForHandle(dest);
		if (destSocket == -1)
		{
			sendErrorForInvalidHandle(senderSocket, dest);
		}
		else
		{
			if (!safeSend(destSocket, payload, payloadLen, MESSAGE_PACKET))
				LOG_ERROR("Failed to forward direct message to socket " << destSocket);
		}
	}
}

// Processes a client exit by sending an exit ACK and cleaning up.
void processClientExit(int clientSocket)
{
	PDU_Send_And_Recv pdu;
	safeSend(clientSocket, nullptr, 0, EXIT_ACK);
	removeClientBySocket(clientSocket);
	removeFromPollSet(clientSocket);
	close(clientSocket);
	LOG_INFO("Client on socket " << clientSocket << " has exited.");
}

// Sends an error packet (flag 7) to the sender for an invalid destination handle.
void sendErrorForInvalidHandle(int senderSocket, const char *destHandle)
{
	PDU_Send_And_Recv pdu;
	uint8_t payload[MAXBUF] = {0};
	uint8_t hLen = strlen(destHandle);
	payload[0] = hLen;
	memcpy(payload + 1, destHandle, hLen);
	safeSend(senderSocket, payload, 1 + hLen, 7);
	LOG_INFO("Sent error for invalid handle: " << destHandle << " to socket " << senderSocket);
}

// Removes a client from the dynamic table by its socket number.
void removeClientBySocket(int clientSocket)
{
	clientTable.removeElementBySocket(clientSocket);
}

// Verify that the received packet length is at least expectedMin.
bool verifyPacketLength(int receivedLen, int expectedMin)
{
	if (receivedLen < expectedMin)
	{
		LOG_ERROR("Packet length (" << receivedLen << ") is less than expected minimum (" << expectedMin << ").");
		return false;
	}
	return true;
}

// Revised safeSend() assumes a fixed 3-byte header.
bool safeSend(int socketNum, uint8_t *payload, int payloadLen, int flag)
{
	const int headerSize = 3; // fixed header size
	int totalBytesToSend = headerSize + payloadLen;
	LOG_DEBUG("safeSend: Sending total " << totalBytesToSend
										 << " bytes (header " << headerSize
										 << " + payload " << payloadLen
										 << ") with flag 0x" << std::hex << flag);

	PDU_Send_And_Recv pdu;
	int bytesSent = pdu.sendBuf(socketNum, payload, payloadLen, flag);
	if (bytesSent != totalBytesToSend)
	{
		LOG_ERROR("safeSend: Expected " << totalBytesToSend << " bytes but only sent " << bytesSent);
		return false;
	}
	LOG_DEBUG("safeSend: Successfully sent " << bytesSent << " bytes with flag 0x" << std::hex << flag);
	return true;
}

// Revised processListRequest(): sends a 7-byte PDU for handle count, then one PDU per handle, and finally an end marker.
void processListRequest(int clientSocket)
{
	PDU_Send_And_Recv pdu;

	// 1) Send the number of handles.
	uint32_t numHandles = clientTable.getCount();
	uint32_t netCount = htonl(numHandles);
	LOG_DEBUG("processListRequest: Sending handle count (" << numHandles
														   << ") with expected PDU size = " << (3 + 4)
														   << " bytes and flag 0x0B.");
	if (!safeSend(clientSocket, reinterpret_cast<uint8_t *>(&netCount), 4, LIST_RESPONSE_NUM))
	{
		LOG_ERROR("processListRequest: Failed to send handle count PDU.");
		return;
	}
	LOG_DEBUG("processListRequest: Handle count PDU sent successfully.");

	// 2) Send one PDU for each registered handle.
	int cap = clientTable.getCapacity();
	Entry_Handle_Table *arr = clientTable.getArray();
	for (int i = 0; i < cap; i++)
	{
		if (arr[i].handle.handleLength != 0)
		{
			uint8_t payload[1 + MAXIMUM_CHARACTERS] = {0};
			payload[0] = arr[i].handle.handleLength;
			memcpy(payload + 1, arr[i].handle.handle, arr[i].handle.handleLength);
			LOG_DEBUG("processListRequest: Sending handle '" << arr[i].handle.handle
															 << "' with payload size " << (1 + arr[i].handle.handleLength)
															 << " (expected PDU size = " << (3 + 1 + arr[i].handle.handleLength)
															 << " bytes, flag 0x0C).");
			if (!safeSend(clientSocket, payload, 1 + arr[i].handle.handleLength, LIST_RESPONSE_HANDLE))
			{
				LOG_ERROR("processListRequest: Failed to send handle PDU for '" << arr[i].handle.handle << "'.");
			}
			else
			{
				LOG_DEBUG("processListRequest: Handle PDU for '" << arr[i].handle.handle << "' sent successfully.");
			}
		}
	}

	// 3) Send the end-of-list marker.
	LOG_DEBUG("processListRequest: Sending end-of-list marker (expected PDU size = 3 bytes, flag 0x0D).");
	if (!safeSend(clientSocket, nullptr, 0, LIST_RESPONSE_END))
	{
		LOG_ERROR("processListRequest: Failed to send end-of-list marker.");
		return;
	}
	LOG_INFO("processListRequest: Completed list response for client socket " << clientSocket);
}
