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

#include "chatFlags.h"

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
void cleanupClient(int clientSocket);
int checkArgs(int argc, char *argv[]);
void talk_to_clients(int serverSocket);
static bool extractHandleFromRegistration(uint8_t* buffer, int len, char* handle, int maxNameLen, uint8_t &handleLen);
void processNewClient(int serverSocket);
static void dispatchPacket(int clientSocket, int flag, uint8_t *buffer, int len);
void processClientPacket(int clientSocket);
void processListRequest(int clientSocket);
void forwardBroadcast(int senderSocket, uint8_t *payload, int payloadLen);
static bool parseSenderAndDestinations(uint8_t *payload, int payloadLen, int &offset, char *sender, int maxSenderSize, int &numDest);
static bool getNextDestinationHandle(uint8_t *payload, int payloadLen, int &offset, char *dest, int maxDestSize);
void forwardDirectMessage(int senderSocket, uint8_t *payload, int payloadLen);
void processClientExit(int clientSocket);
void sendErrorForInvalidHandle(int senderSocket, const char *destHandle);
void removeClientBySocket(int clientSocket);
bool verifyPacketLength(int receivedLen, int expectedMin);
bool safeSend(int socketNum, uint8_t *payload, int payloadLen, int flag);
bool sendHandleEntry(int clientSocket, const char *handle, uint8_t handleLen);
bool sendEndOfListMarker(int clientSocket);

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
    try {
        // If more than one optional parameter is provided, it's an error.
        if (argc > 2) {
            throw std::invalid_argument("Usage: <program> [optional port number]");
        }

        int portNumber = 0;
        if (argc == 2) {
            // Use std::string and std::stoi for robust conversion.
            std::string arg(argv[1]);
            portNumber = std::stoi(arg);

            // Optionally, verify that the port number is within a valid range.
            if (portNumber < 1 || portNumber > 65535) {
                throw std::out_of_range("Port number must be between 1 and 65535.");
            }
        }
        return portNumber;
    } catch (const std::exception &e) {
        std::cerr << "Error processing command-line arguments: " << e.what() << std::endl;
        exit(-1);
    }
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

// Helper function: Extract the client handle from a registration packet payload.
// It reads the first byte for the handle length and then extracts the handle.
// Parameters:
// - buffer: the payload received from the client.
// - len: total length of the payload.
// - handle: buffer to store the extracted handle (must be large enough).
// - maxNameLen: maximum allowed length for the handle.
// - handleLen: reference to store the actual handle length.
// Returns true if the extraction is successful; false otherwise.
static bool extractHandleFromRegistration(uint8_t* buffer, int len, char* handle, int maxNameLen, uint8_t &handleLen)
{
    if (len < 1) {
        LOG_ERROR("Registration payload is too short.");
        return false;
    }
    handleLen = buffer[0];
    if (handleLen == 0 || handleLen > maxNameLen || len < (1 + handleLen)) {
        LOG_ERROR("Invalid handle length in registration packet: " << (int)handleLen
                  << ". Expected between 1 and " << maxNameLen << " bytes.");
        return false;
    }
    memcpy(handle, buffer + 1, handleLen);
    handle[handleLen] = '\0';
    return true;
}

// Revised processNewClient(): Accepts a new client connection, receives its registration packet,
// extracts the handle using the helper function, validates it, and registers the client.
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

    if (!verifyPacketLength(len, 1) || flag != CLIENT_INIT_PACKET_TO_SERVER) {
        LOG_ERROR("Invalid registration packet received. Expected flag " 
                  << CLIENT_INIT_PACKET_TO_SERVER << " but got flag " << flag);
        cleanupClient(clientSocket);
        return;
    }

    uint8_t handleLen;
    char handle[ChatConstants::MaxNameLen + 1] = {0};

    // Use helper function to extract the handle.
    if (!extractHandleFromRegistration(buffer, len, handle, ChatConstants::MaxNameLen, handleLen)) {
        pdu.sendBuf(clientSocket, nullptr, 0, ERROR_ON_INIT_PACKET);
        cleanupClient(clientSocket);
        return;
    }
    LOG_DEBUG("Parsed handle: " << handle);

    // Check for duplicate handles.
    if (clientTable.getSocketForHandle(handle) != -1) {
        pdu.sendBuf(clientSocket, nullptr, 0, ERROR_ON_INIT_PACKET);
        LOG_ERROR("Duplicate handle (" << handle << ") registration attempt on socket " << clientSocket);
        cleanupClient(clientSocket);
        return;
    }

    // Create a new entry and add it to the dynamic table.
    Handling newHandle;
    newHandle.handleLength = handleLen;
    memcpy(newHandle.handle, handle, handleLen);
    newHandle.handle[handleLen] = '\0';

    if (clientTable.addElement(newHandle, clientSocket) < 0) {
        pdu.sendBuf(clientSocket, nullptr, 0, ERROR_ON_INIT_PACKET);
        LOG_ERROR("Failed to add handle (" << handle << ") to dynamic table.");
        cleanupClient(clientSocket);
        return;
    }

    // Confirm registration and add the new client to the poll set.
    pdu.sendBuf(clientSocket, nullptr, 0, CONFIRM_GOOD_HANDLE);
    addToPollSet(clientSocket);
    LOG_INFO("New client registered: " << handle << " on socket " << clientSocket);
}


// Helper function: Dispatch the packet based on its flag. 
static void dispatchPacket(int clientSocket, int flag, uint8_t *buffer, int len) {
	switch (flag) {
		case BROADCAST_PACKET:
			forwardBroadcast(clientSocket, buffer, len);
			break;

		case MESSAGE_PACKET:
			forwardDirectMessage(clientSocket, buffer, len);
			break;

		case CLIENT_TO_SERVER_EXIT:
			LOG_DEBUG("Dispatch: Processing exit packet from socket " << clientSocket);
			processClientExit(clientSocket);
			break;

		case CLIENT_TO_SERVER_LIST_OF_HANDLES: // flag 10 (0x0A)
			LOG_DEBUG("Dispatch: Processing list request from socket" << clientSocket);
			processListRequest(clientSocket);
			break;

		default: 
			LOG_ERROR("Dispatch: Unknown flag " << flag << " received from socket " << clientSocket << ". Data: " << hexDump(buffer, len));
			break;
	}
}

// Processes a packet from an already connected client.
// Revised processClientPacket() using helper functions.
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

    // Dispatch the packet to the appropriate handler.
    dispatchPacket(clientSocket, flag, buffer, len);
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

// Helper function: Parses sender handle and destination count from the payload.
// Parameters:
// - payload: the received direct message payload.
// - payloadLen: total length of the payload.
// - offset: reference to the current offset (starting at 0) in the payload; it will be updated.
// - sender: buffer to store the parsed sender handle (should have size at least maxSenderSize).
// - maxSenderSize: maximum allowed size for sender.
// - numDest: reference to an int to store the number of destination handles.
// Returns true if successful; false otherwise.
static bool parseSenderAndDestinations(uint8_t *payload, int payloadLen, int &offset, char *sender, int maxSenderSize, int &numDest) {
	if (payloadLen < 1) {
		LOG_ERROR("Payload length insufficient for sender length.");
		return false;
	}

	uint8_t senderLen = payload[offset++];

	if (payloadLen < (1 + senderLen + 1)) {
		LOG_ERROR("Payload too short for sender and destination count.");
		return false;
	}

	if (senderLen >= maxSenderSize) {
		LOG_ERROR("Sender length exceeds maximum allowed (" << maxSenderSize << ").");
		return false;
	}

	memcpy(sender, payload + offset, senderLen);
	sender[senderLen] = '\0';
	offset += senderLen;
	numDest = payload[offset++];

	return true;
}

// Helper function: Parses the next destination handle from the payload.
// Parameters:
// - payload: the received direct message payload.
// - payloadLen: total length of the payload.
// - offset: reference to the current offset in the payload; it will be updated.
// - dest: buffer to store the parsed destination handle (should have size at least maxDestSize).
// - maxDestSize: maximum allowed size for destination handle.
// Returns true if successful; false otherwise.
static bool getNextDestinationHandle(uint8_t *payload, int payloadLen, int &offset, char *dest, int maxDestSize) {
	if (offset >= payloadLen) {
		LOG_ERROR("Direct message packet truncated while reading destination handle length.");
		return false;
	}

	uint8_t destLen = payload[offset++];

	if (offset + destLen > payloadLen) {
		LOG_ERROR("Direct meesage packet length inconsistent with destination handle length.");
		return false;
	}

	if (destLen >= maxDestSize) {
		LOG_ERROR("Destination handle length exceeds maximum allowed (" << maxDestSize << ").");
		return false;
	}

	memcpy(dest, payload + offset, destLen);
	dest[destLen] = '\0';
	offset += destLen;

	return true;
}

// Revised forwardDirectMessage() using helper functions and ChatConstants::MaxNameLen.
void forwardDirectMessage(int senderSocket, uint8_t *payload, int payloadLen)
{
	int offset = 0;
	char sender[ChatConstants::MaxNameLen + 1] = {0};
	int numDest = 0;

	// Parse the sender handle and destination count.
	if (!parseSenderAndDestinations(payload, payloadLen, offset, sender, ChatConstants::MaxNameLen + 1, numDest)) {
		LOG_ERROR("Failed to parse sender and destination count.");
		return;
	}

	LOG_INFO("Direct message from " << sender << " to " << numDest << " destination(s).");

	// Process each destination.
	for (int i = 0; i < numDest; i++) {
		char dest[ChatConstants::MaxNameLen + 1] = {0};

		if (!getNextDestinationHandle(payload, payloadLen, offset, dest, ChatConstants::MaxNameLen + 1)) {
			LOG_ERROR("Failed to parse destination handle for destination " << i + 1);
			return;
		}

		int destSocket = clientTable.getSocketForHandle(dest);

		if (destSocket == -1) {
			sendErrorForInvalidHandle(senderSocket, dest);
		} else {
			if (!safeSend(destSocket, payload, payloadLen, MESSAGE_PACKET))
				LOG_ERROR("Failed to forward direct message to socket " << destSocket);
		}
	}
}

// Processes a client exit by sending an exit ACK and cleaning up.
void processClientExit(int clientSocket)
{
	safeSend(clientSocket, nullptr, 0, EXIT_ACK);
	removeClientBySocket(clientSocket);
	removeFromPollSet(clientSocket);
	close(clientSocket);
	LOG_INFO("Client on socket " << clientSocket << " has exited.");
}

// Sends an error packet (flag 7) to the sender for an invalid destination handle.
void sendErrorForInvalidHandle(int senderSocket, const char *destHandle)
{
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

// Helper function: Sends the list count PDU.
bool sendListCount(int clientSocket, uint32_t numHandles)
{
	uint32_t netCount = htonl(numHandles);

	LOG_DEBUG("sendListCount: Sending handle count (" << numHandles << ") with expected PDU size = " << (SIZE_CHAT_HEADER + 4) << " bytes and flag 0x0B.");

	if (!safeSend(clientSocket, reinterpret_cast<uint8_t *>(&netCount), 4, LIST_RESPONSE_NUM))
	{
		LOG_ERROR("sendListCount: Failed to send handle count PDU.");
		return false;
	}

	LOG_DEBUG("sendListCount: Handle count PDU send successfully.");
	return true;
}

// Helper function: Sends a handle entry PDU.
bool sendHandleEntry(int clientSocket, const char *handle, uint8_t handleLen)
{
	uint8_t payload[1 + MAXIMUM_CHARACTERS] = {0};
	payload[0] = handleLen;
	memcpy(payload + 1, handle, handleLen);

	LOG_DEBUG("sendHandleEntry: Sending handle '" << handle
												  << "' with payload size " << (1 + handleLen)
												  << " (expected PDU size = " << (SIZE_CHAT_HEADER + 1 + handleLen)
												  << " bytes, flag 0x0C).");

	if (!safeSend(clientSocket, payload, 1 + handleLen, LIST_RESPONSE_HANDLE))
	{
		LOG_ERROR("sendHandleEntry: Failed to send handle PDU for '" << handle << "'.");
		return false;
	}

	LOG_DEBUG("sendHandleEntry: Handle PDU for '" << handle << "' sent successfully.");
	return true;
}

// Helper function: Sends the end-of-list marker PDU.
bool sendEndOfListMarker(int clientSocket)
{
	LOG_DEBUG("sendEndOfListMarker: Sending end-of-list marker (expected PDU size = " << SIZE_CHAT_HEADER << " bytes, flag 0x0D).");

	if (!safeSend(clientSocket, nullptr, 0, LIST_RESPONSE_END))
	{
		LOG_ERROR("sendEndOfListMarker: Failed to send end-of-list marker.");
		return false;
	}

	LOG_DEBUG("sendEndOfListMarker: End-of-list marker sent successfully.");
	return true;
}

// Revised processListRequest(): Uses helper functions to modularize the code.
void processListRequest(int clientSocket)
{
	// 1) Send the number of handles.
	uint32_t numHandles = clientTable.getCount();

	if (!sendListCount(clientSocket, numHandles))
		return;

	// 2) Send one PDU for each registered handle.
	int cap = clientTable.getCapacity();
	Entry_Handle_Table *arr = clientTable.getArray();
	int failureCount = 0;

	for (int i = 0; i < cap; i++)
	{
		if (arr[i].handle.handleLength != 0)
		{
			if (!sendHandleEntry(clientSocket, arr[i].handle.handle, arr[i].handle.handleLength))
			{
				// Log failure for this particular handle and increment failure counter.
				LOG_ERROR("processListRequest: Failed to send handle entry for '" << arr[i].handle.handle << "'.");
				failureCount++;
				// Optionally, you could break out of the loop if failureCount exceeds a threshold.
				// if (failureCount >= SOME_THRESHOLD) break;
			}
		}
	}

	if (failureCount > 0)
	{
		LOG_ERROR("processListRequest: " << failureCount << " handle entries failed to send.");
		// Optionally, take further action here (e.g., abort the list response or notify an administrator).
	}

	// 3) Send the end-of-list marker.
	if (!sendEndOfListMarker(clientSocket))
		return;

	LOG_INFO("processListRequest: Completed list response for client socket " << clientSocket);
}
