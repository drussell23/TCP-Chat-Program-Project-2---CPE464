/******************************************************************************
 * Name: Derek J. Russell
 *
 * This client program connects to the chat server, registers the client
 * using a handle, and then enters an asynchronous loop to process commands:
 *
 *   %M  – Send a message to one or more specific clients.
 *   %B  – Broadcast a message.
 *   %L  – Request the list of connected handles.
 *   %E  – Exit the client.
 *
 * It continuously monitors STDIN and the server socket using poll, and
 * logs debug information to help diagnose connection or packet issues.
 *****************************************************************************/

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <sstream> // For hex dump logging
#include <cctype>  // For isalpha()

#include <unistd.h>
#include <signal.h>

#include "pollLib.h"
#include "networks.h"
#include "PDU_Send_And_Recv.h"
#include "ConnectionStats.h"
#include "chatFlags.h"
#include "NLPProcessor.h" // Include the NLP module

// For simulation mode:
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <fstream>

using namespace std;

// ---------------------------------------------------------------------------
// Configuration and debug macros
// ---------------------------------------------------------------------------
#define DEBUG_FLAG 1

#if DEBUG_FLAG
#define LOG_DEBUG(msg) std::cout << "[DEBUG] " << msg << std::endl
#else
#define LOG_DEBUG(msg)
#endif

#define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl
#define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl

// ---------------------------------------------------------------------------
// Hex dump helper function
// ---------------------------------------------------------------------------
static std::string hexDump(const uint8_t *buffer, int length)
{
	std::stringstream ss;
	ss << std::hex << std::setfill('0');
	for (int i = 0; i < length; i++)
	{
		ss << std::setw(2) << static_cast<int>(buffer[i]) << " ";
	}
	return ss.str();
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define MAXBUF 1024
#define MAX_NAME_LEN 100

// Command strings (case insensitive)
#define CMD_MESSAGE "%M"
#define CMD_BROADCAST "%B"
#define CMD_LIST "%L"
#define CMD_CURRENT_CONNECTION_STATUS "%S"
#define CMD_EXIT "%E"

// Packet flags
#define CLIENT_INIT_PACKET_TO_SERVER 1
#define BROADCAST_PACKET 4
#define MESSAGE_PACKET 5
#define EXIT_PACKET 8

#define LIST_REQUEST_PACKET 0x0A  // Sent by client to request list (%L)
#define LIST_RESPONSE_NUM 0x0B	  // Server sends: 4-byte number of handles
#define LIST_RESPONSE_HANDLE 0x0C // Server sends: one handle (1 byte length + handle)
#define LIST_RESPONSE_END 0x0D	  // Server sends: end-of-list marker

// The maximum total bytes allowed for the text portion in each packet (including null terminator).
#define MAX_TEXT_PER_PACKET 200

// Confirm good handle flag (registration confirmation).
#define CONFIRM_GOOD_HANDLE 2

// ---------------------------------------------------------------------------
// Global variables
// ---------------------------------------------------------------------------
// static string g_clientHandle;
thread_local string g_clientHandle;

static int g_socketNum = -1;
static ConnectionStats connStats; // Global instance for tracking connection stats.

// ---------------------------------------------------------------------------
// Function declarations
// ---------------------------------------------------------------------------
int readFromStdin(char *buffer);
void checkArgs(int argc, char *argv[]);
void connection_setup(int socketNum, const char *handle);
void processIncomingPacket(int socketNum);

void handleMessageCommand(int socketNum, const char *input);
void handleBroadcastCommand(int socketNum, const char *input);
void handleListCommand(int socketNum);
void handleExitCommand(int socketNum);

// New function
int readNBytes(int socketNum, uint8_t *buffer, int n);
void simulateClient(int clientId, const string &server, int port, int totalMessages, const vector<string> &simHandles);
// Helper function for receiver thread.
void receiverThread(int sock, ofstream &logFile, PDU_Send_And_Recv &pdu);
int randomDelay(int base, int range);

// Helper function for receiver thread.
void receiverThread(int sock, std::ofstream &logFile, PDU_Send_And_Recv &pdu)
{
	const int bufSize = 1024;
	uint8_t buffer[bufSize];
	int flag;

	while (true)
	{
		int len = pdu.recvBuf(sock, buffer, &flag);
		if (len <= 0)
			break;
		logFile << "[Received] Flag: " << flag << ", Len: " << len << ", Data: ";
		for (int i = 0; i < len; i++)
		{
			logFile << std::hex << (int)buffer[i] << " ";
		}
		logFile << std::dec << std::endl;
	}
	logFile << "[Receiver] Connection closed." << std::endl;
}

int randomDelay(int base, int range)
{
	return base + rand() % range;
}

// Simulation mode: Simulate a single client instance.
void simulateClient(int clientId, const std::string &server, int port, int totalMessages, const std::vector<std::string> &simHandles)
{
	// Use the handle provided in the simHandles vector.
	std::string handle = simHandles[clientId];
	// Set the thread-local client handle for this simulation.
	g_clientHandle = handle;

	// Prepare server address and port.
	char serverAddr[256];
	strncpy(serverAddr, server.c_str(), sizeof(serverAddr));
	serverAddr[sizeof(serverAddr) - 1] = '\0';
	char portStr[10];
	snprintf(portStr, sizeof(portStr), "%d", port);

	// Connect to the server.
	int sock = tcpClientSetup(serverAddr, portStr, 0);
	if (sock < 0)
	{
		std::cerr << handle << " failed to connect to server." << std::endl;
		return;
	}
	std::cout << handle << " connected on socket " << sock << std::endl;

	// Open a log file.
	std::ofstream logFile("simclient_" + std::to_string(clientId) + "_log.txt");
	logFile << "Client " << handle << " log start." << std::endl;

	// Send registration packet.
	uint8_t regPayload[256];
	uint8_t hLen = static_cast<uint8_t>(handle.size());
	regPayload[0] = hLen;
	memcpy(regPayload + 1, handle.c_str(), hLen);

	PDU_Send_And_Recv pdu;
	int regBytes = pdu.sendBuf(sock, regPayload, 1 + hLen, CLIENT_INIT_PACKET_TO_SERVER);
	logFile << "Sent registration packet: Handle = " << handle << ", Bytes sent = " << regBytes << std::endl;

	// NEW: Wait for a short period to allow all clients to register.
	std::this_thread::sleep_for(std::chrono::seconds(2));

	// Start a receiver thread to log incoming messages.
	std::thread recvThread(receiverThread, sock, std::ref(logFile), std::ref(pdu));

	// Create an NLPProcessor instance.
	NLPProcessor nlp;
	std::default_random_engine eng((unsigned)std::chrono::system_clock::now().time_since_epoch().count());
	std::uniform_int_distribution<int> recipientDist(0, simHandles.size() - 1);

	// Simulate sending totalMessages messages.
	for (int i = 0; i < totalMessages; i++)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(randomDelay(100, 400)));
		// Randomly decide whether to send a broadcast or a direct message.
		bool isBroadcast = (rand() % 2 == 0);
		std::string nlCommand;
		if (isBroadcast)
		{
			nlCommand = "broadcast good morning from " + handle;
		}
		else
		{
			// Pick a random recipient that is not this client.
			std::string recipient;
			do
			{
				recipient = simHandles[recipientDist(eng)];
			} while (recipient == handle);
			nlCommand = "send a message to " + recipient + " hello from " + handle;
		}

		logFile << "[Sent Raw] " << nlCommand << std::endl;
		std::string structuredCommand = nlp.processMessage(nlCommand);
		logFile << "[Converted] " << structuredCommand << std::endl;

		if (structuredCommand.substr(0, 2) == "%M")
			handleMessageCommand(sock, structuredCommand.c_str());
		else if (structuredCommand.substr(0, 2) == "%B")
			handleBroadcastCommand(sock, structuredCommand.c_str());
		else if (structuredCommand.substr(0, 2) == "%L")
			handleListCommand(sock);
		else
		{
			int flagToSend = 0;
			pdu.sendBuf(sock, reinterpret_cast<uint8_t *>(const_cast<char *>(structuredCommand.c_str())),
						structuredCommand.length(), flagToSend);
		}
		logFile << "[Sent Structured] " << structuredCommand << std::endl;
	}

	// Finally, send an exit command.
	std::string exitCommand = "%E";
	int exitBytes = pdu.sendBuf(sock, reinterpret_cast<uint8_t *>(const_cast<char *>(exitCommand.c_str())),
								exitCommand.length(), EXIT_PACKET);
	if (exitBytes != (int)(SIZE_CHAT_HEADER + exitCommand.length()))
	{
		LOG_ERROR("Failed to send exit command properly. Socket may have been closed.");
	}
	else
	{
		logFile << "Sent exit command." << std::endl;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	recvThread.join();
	close(sock);

	logFile << handle << " simulation complete." << std::endl;
}

int main(int argc, char *argv[])
{
	// Ignore SIGPIPE to prevent termination when sending on a closed socket.
	signal(SIGPIPE, SIG_IGN);

	// Check for simulation mode: if extra arguments "--simulate" and number are provided.
	bool simulationMode = false;
	int numClients = 0;

	if (argc == 6 && std::string(argv[4]) == "--simulate")
	{
		simulationMode = true;
		numClients = atoi(argv[5]);
	}

	if (simulationMode)
	{
		// Build a list of simulated handles for all clients in lowercase.
		vector<std::string> simHandles;

		for (int i = 0; i < numClients; i++)
		{
			string handle = "SimClient_" + std::to_string(i);
			// Convert to lowercase:
			transform(handle.begin(), handle.end(), handle.begin(), ::tolower);
			simHandles.push_back(handle);
		}

		std::string server = argv[2];
		int port = atoi(argv[3]);
		std::vector<std::thread> clients;
		// For simulation, have each client send, say, 30 messages.
		int totalMessages = 30;

		for (int i = 0; i < numClients; i++)
		{
			clients.push_back(std::thread(simulateClient, i, server, port, totalMessages, simHandles));
		}

		for (auto &t : clients)
			t.join();

		return 0;
	}

	LOG_DEBUG("Client active socket (g_socketNum): " << g_socketNum);

	system("clear");
	checkArgs(argc, argv);

	// Additional check: ensure the handle begins with a letter.
	if (!isalpha(argv[1][0]))
	{
		LOG_ERROR("Handle must start with a letter.");
		exit(1);
	}
	if (strlen(argv[1]) > MAX_NAME_LEN)
	{
		LOG_ERROR("Handle exceeds maximum allowed length of " << MAX_NAME_LEN);
		exit(1);
	}

	// Store the client's handle globally.
	g_clientHandle = argv[1];
	LOG_DEBUG("Client handle set to: " << g_clientHandle);

	// Set up TCP connection using the provided server name and port.
	LOG_DEBUG("Attempting to connect to server: " << argv[2]
												  << ", port: " << argv[3]);
	g_socketNum = tcpClientSetup(argv[2], argv[3], 1);
	if (g_socketNum < 0)
	{
		LOG_ERROR("Failed to connect to server.");
		exit(1);
	}
	LOG_INFO("Connected to server on socket " << g_socketNum);

	// Add STDIN and the socket to the poll set.
	addToPollSet(STDIN_FILENO);
	addToPollSet(g_socketNum);

	// Register with the server using the client handle.
	LOG_DEBUG("Calling connection_setup(...) to register handle once.");
	connection_setup(g_socketNum, g_clientHandle.c_str());
	LOG_DEBUG("Registration packet sent to server.");

	char inputBuffer[MAXBUF] = {0};

	NLPProcessor nlp; // Create an NLPProcessor instance (could also be created on-demand).

	// Asynchronous loop: poll for events on STDIN or the socket.
	while (true)
	{
		int ready_fd = pollCall(-1); // Blocks until an event occurs.
		LOG_DEBUG("pollCall returned FD: " << ready_fd);

		if (ready_fd == STDIN_FILENO)
		{
			// Print a prompt here so the user knows to type a command.
			cout << "$: ";
			cout.flush();

			int len = readFromStdin(inputBuffer);

			if (len <= 0)
				continue;

			// If the input does not start with '%', assume natural language.
			if (inputBuffer[0] != '%')
			{
				// Process using NLP to convert into a structured command.
				string structuredCommand = nlp.processMessage(inputBuffer);

				// Display the converted structured command.
				cout << "Converted command: " << structuredCommand << endl;

				// If the NLP module returns an error message, display it.
				if (structuredCommand.find("Error:") == 0)
				{
					cout << structuredCommand << endl;
					continue;
				}
				else
				{
					// For debugging, show the structured command.
					LOG_DEBUG("NLP converted input to: " << structuredCommand);
					// Now, process the structured command as if the user had typed it.
					if (strncasecmp(structuredCommand.c_str(), CMD_MESSAGE, strlen(CMD_MESSAGE)) == 0)
					{
						handleMessageCommand(g_socketNum, structuredCommand.c_str());
					}
					else if (strncasecmp(structuredCommand.c_str(), CMD_BROADCAST, strlen(CMD_BROADCAST)) == 0)
					{
						handleBroadcastCommand(g_socketNum, structuredCommand.c_str());
					}
					else if (strncasecmp(structuredCommand.c_str(), CMD_LIST, strlen(CMD_LIST)) == 0)
					{
						handleListCommand(g_socketNum);
					}
					else if (strncasecmp(structuredCommand.c_str(), CMD_CURRENT_CONNECTION_STATUS, strlen(CMD_CURRENT_CONNECTION_STATUS)) == 0)
					{
						connStats.printStats();
					}
					else if (strncasecmp(structuredCommand.c_str(), CMD_EXIT, strlen(CMD_EXIT)) == 0)
					{
						handleExitCommand(g_socketNum);
						break;
					}
					else
					{
						cout << "Unknown structured command: " << structuredCommand << endl;
					}
				}
			}
			else // If input starts with '%', process as a strict command.
			{
				if (strncasecmp(inputBuffer, CMD_MESSAGE, strlen(CMD_MESSAGE)) == 0)
				{
					LOG_DEBUG("User typed %M command");
					handleMessageCommand(g_socketNum, inputBuffer);
				}
				else if (strncasecmp(inputBuffer, CMD_BROADCAST, strlen(CMD_BROADCAST)) == 0)
				{
					LOG_DEBUG("User typed %B command");
					handleBroadcastCommand(g_socketNum, inputBuffer);
				}
				else if (strncasecmp(inputBuffer, CMD_LIST, strlen(CMD_LIST)) == 0)
				{
					LOG_DEBUG("User typed %L command");
					handleListCommand(g_socketNum);
				}
				else if (strncasecmp(inputBuffer, CMD_CURRENT_CONNECTION_STATUS, strlen(CMD_CURRENT_CONNECTION_STATUS)) == 0)
				{
					connStats.printStats();
				}
				else if (strncasecmp(inputBuffer, CMD_EXIT, strlen(CMD_EXIT)) == 0)
				{
					LOG_DEBUG("User typed %E command");
					handleExitCommand(g_socketNum);
					break;
				}
				else
				{
					cout << "Invalid command" << endl;
				}
			}
		}
		else if (ready_fd == g_socketNum)
		{
			LOG_DEBUG("Processing incoming data on socket: " << g_socketNum);
			// Process incoming packet from the server.
			processIncomingPacket(g_socketNum);
		}
	}
	close(g_socketNum);
	return 0;
}

// ---------------------------------------------------------------------------
// Reads a line from STDIN into buffer. Returns the length (excluding newline).
// ---------------------------------------------------------------------------
int readFromStdin(char *buffer)
{
	if (fgets(buffer, MAXBUF, stdin) == nullptr)
		return 0;

	size_t len = strlen(buffer);
	if (len > 0 && buffer[len - 1] == '\n')
	{
		buffer[len - 1] = '\0';
		len--;
	}
	return static_cast<int>(len);
}

// ---------------------------------------------------------------------------
// Checks command-line arguments.
// ---------------------------------------------------------------------------
void checkArgs(int argc, char *argv[])
{
	if (argc != 4)
	{
		LOG_ERROR("Usage: cclient [handle] [server-name] [server-port]");
		exit(1);
	}
}

// ---------------------------------------------------------------------------
// Sends the initial connection (registration) packet with the client handle.
// ---------------------------------------------------------------------------
void connection_setup(int socketNum, const char *handle)
{
	LOG_DEBUG("connection_setup: Using socket " << socketNum << " to send registration packet.");

	PDU_Send_And_Recv pdu;

	uint8_t buffer[MAXBUF] = {0};
	uint8_t nameLength = static_cast<uint8_t>(strlen(handle));

	// Build payload: [1 byte handle length][handle chars]
	buffer[0] = nameLength;
	memcpy(buffer + 1, handle, nameLength);

	uint16_t payload_len = 1 + nameLength;

	LOG_DEBUG("Sending registration packet: flag=" << CLIENT_INIT_PACKET_TO_SERVER
												   << ", payload_len=" << payload_len
												   << ", handle=" << handle);

	// Optional hex dump for debugging:
	LOG_DEBUG("Registration packet payload hex dump: "
			  << hexDump(buffer, payload_len));

	// Send the registration packet and record statistics.
	int bytesSent = pdu.sendBuf(socketNum, buffer, payload_len, CLIENT_INIT_PACKET_TO_SERVER);
	connStats.recordSent(bytesSent);
	connStats.recordMessageSent();
}

// ---------------------------------------------------------------------------
// Processes incoming packets from the server and displays messages or errors.
// ---------------------------------------------------------------------------
void processIncomingPacket(int socketNum)
{
	LOG_DEBUG("processIncomingPacket: Receiving data on socket " << socketNum);

	PDU_Send_And_Recv pdu;
	uint8_t dataBuffer[MAXBUF] = {0};
	int flag;
	int len = pdu.recvBuf(socketNum, dataBuffer, &flag);

	// Instead of assuming the header is 3 bytes, use SIZE_CHAT_HEADER.
	const int headerSize = SIZE_CHAT_HEADER; // Defined in PDU_Send_And_Recv.h

	if (len >= 0)
	{
		connStats.recordReceived(len + headerSize);
		if (flag == MESSAGE_PACKET || flag == BROADCAST_PACKET)
			connStats.recordMessageReceived();
	}

	// Log the flag both as a number and as a string with its description.
	LOG_DEBUG("Received packet from server: flag="
			  << flag << " (" << chatFlagToString(flag) << ") - " << chatFlagDescription(flag)
			  << ", len=" << len
			  << ", hex=" << hexDump(dataBuffer, (len < 16 ? len : 16)) << "...");

	if (len == -1)
	{
		cout << "Server terminated connection." << endl;
		exit(1);
	}

	// If we received a valid packet with zero-length payload...
	if (len == VALID_ZERO_PAYLOAD)
	{
		if (flag == CONFIRM_GOOD_HANDLE)
		{
			cout << "Registration confirmed by server." << endl;
		}
		else
		{
			LOG_DEBUG("Received valid zero-length payload with flag=" << flag);
		}
		// No further processing needed for this packet.
		return;
	}

	// Process based on the flag.
	if (flag == MESSAGE_PACKET || flag == BROADCAST_PACKET)
	{
		// Format: [1 byte sender handle length][sender handle][text message (null terminated)]
		if (len < 2)
		{
			LOG_ERROR("Invalid message/broadcast packet: too short");
			return;
		}

		uint8_t senderLen = dataBuffer[0];

		if (senderLen + 1 > (uint8_t)len)
		{
			LOG_ERROR("Sender length exceeds packet length");
			return;
		}

		char sender[MAX_NAME_LEN + 1] = {0};

		memcpy(sender, dataBuffer + 1, senderLen);
		sender[senderLen] = '\0';

		const char *message = (const char *)(dataBuffer + 1 + senderLen);

		cout << sender << ": " << message << endl;
	}
	else if (flag == LIST_RESPONSE_NUM || flag == LIST_RESPONSE_HANDLE || flag == LIST_RESPONSE_END)
	{
		// %L command responses are handled in handleListCommand(), so you might ignore them here.
		LOG_DEBUG("Received a list response packet (flag=" << flag << ").");
	}
	else if (flag == 7)
	{
		// Error packet for non-existent destination.
		if (len < 2)
		{
			LOG_ERROR("Invalid error packet: too short for handle length");
			return;
		}

		uint8_t destLen = dataBuffer[0];

		if (1 + destLen > (uint8_t)len)
		{
			LOG_ERROR("Destination handle length exceeds packet length");
			return;
		}

		char dest[MAX_NAME_LEN + 1] = {0};
		memcpy(dest, dataBuffer + 1, destLen);
		dest[destLen] = '\0';

		cout << "Error: Client with handle " << dest << " does not exist." << endl;
	}
	else if (flag == 9)
	{ // ACK for exit.
		cout << "Exit ACK received." << endl;
	}
	else
	{
		LOG_DEBUG("Received an unrecognized flag from server: " << flag);
		cout << "Received packet with flag " << flag << endl;
	}
}

// ---------------------------------------------------------------------------
// Handles the %M command (send message to specific clients).
// Expected format: %M <num-handles> <destHandle1> [destHandle2 ...] <message>
// ---------------------------------------------------------------------------
void handleMessageCommand(int socketNum, const char *input)
{
	char temp[MAXBUF];
	strncpy(temp, input, MAXBUF);
	temp[MAXBUF - 1] = '\0';

	// Tokenize the input.
	char *token = strtok(temp, " "); // Should be "%M"
	token = strtok(nullptr, " ");	 // Number of destination handles.

	if (token == nullptr)
	{
		cout << "Invalid %M command format" << endl;
		return;
	}

	int numHandles = atoi(token);
	LOG_DEBUG("handleMessageCommand: numHandles=" << numHandles);

	if (numHandles < 1 || numHandles > 9)
	{
		cout << "Invalid number of destination handles" << endl;
		return;
	}

	// Collect destination handles.
	char desHandles[10][MAX_NAME_LEN];
	for (int i = 0; i < numHandles; i++)
	{
		token = strtok(nullptr, " ");
		if (token == nullptr)
		{
			cout << "Insufficient destination handles" << endl;
			return;
		}
		strncpy(desHandles[i], token, MAX_NAME_LEN);
		desHandles[i][MAX_NAME_LEN - 1] = '\0';
	}

	// The rest of the input is the message text.
	token = strtok(nullptr, "\n");
	const char *message = (token != nullptr) ? token : "";

	// Build the header portion of the payload.
	// Format: [1 byte sender handle length][sender handle][1 byte num destinations]
	// For each destination: [1 byte dest handle length][dest handle]
	uint8_t headerPayload[MAXBUF];
	int headerOffset = 0;

	uint8_t senderLen = (uint8_t)g_clientHandle.length();
	headerPayload[headerOffset++] = senderLen;
	memcpy(headerPayload + headerOffset, g_clientHandle.c_str(), senderLen);
	headerOffset += senderLen;
	headerPayload[headerOffset++] = (uint8_t)numHandles;

	for (int i = 0; i < numHandles; i++)
	{
		uint8_t dLen = (uint8_t)strlen(desHandles[i]);
		headerPayload[headerOffset++] = dLen;
		memcpy(headerPayload + headerOffset, desHandles[i], dLen);
		headerOffset += dLen;
	}

	int headerSize = headerOffset;
	int messageLen = strlen(message);
	int pos = 0;
	const int MAX_SEGMENT = MAX_TEXT_PER_PACKET - 1;

	PDU_Send_And_Recv pdu;

	// If no message, send a packet with just the header plus a '\0'.
	if (messageLen == 0)
	{
		uint8_t packetPayload[MAXBUF];
		memcpy(packetPayload, headerPayload, headerSize);
		packetPayload[headerSize] = '\0';

		LOG_DEBUG("Sending empty message packet: flag=" << MESSAGE_PACKET
														<< ", totalLen=" << (headerSize + 1));
		LOG_DEBUG("Payload hex: " << hexDump(packetPayload, headerSize + 1));

		pdu.sendBuf(socketNum, packetPayload, headerSize + 1, MESSAGE_PACKET);
		return;
	}

	// Otherwise, segment the message if necessary.
	while (pos < messageLen)
	{
		int segmentLength = (messageLen - pos > MAX_SEGMENT) ? MAX_SEGMENT : (messageLen - pos);
		uint8_t packetPayload[MAXBUF];

		memcpy(packetPayload, headerPayload, headerSize);
		memcpy(packetPayload + headerSize, message + pos, segmentLength);
		packetPayload[headerSize + segmentLength] = '\0';

		int totalPacketLength = headerSize + segmentLength + 1;
		LOG_DEBUG("Sending message packet: flag=" << MESSAGE_PACKET
												  << ", segmentLength=" << segmentLength
												  << ", totalLen=" << totalPacketLength);
		LOG_DEBUG("Payload hex: " << hexDump(packetPayload, totalPacketLength));

		int bytesSent = pdu.sendBuf(socketNum, packetPayload, totalPacketLength, MESSAGE_PACKET);
		connStats.recordSent(bytesSent);
		connStats.recordMessageSent();
		pos += segmentLength;
	}
}

// ---------------------------------------------------------------------------
// Handles the %B command (broadcast message).
// Expected format: %B <message>
// ---------------------------------------------------------------------------
void handleBroadcastCommand(int socketNum, const char *input)
{
	LOG_DEBUG("handleBroadcastCommand called");
	char temp[MAXBUF];
	strncpy(temp, input, MAXBUF);
	temp[MAXBUF - 1] = '\0';

	char *token = strtok(temp, " "); // Skip command token
	token = strtok(nullptr, "\n");	 // The rest is the message
	const char *message = (token != nullptr) ? token : "";

	int messageLen = strlen(message);
	int pos = 0;
	const int MAX_SEGMENT = MAX_TEXT_PER_PACKET - 1;

	PDU_Send_And_Recv pdu;

	// Build the broadcast header: [1 byte sender handle length][sender handle]
	uint8_t headerPayload[MAXBUF];
	int headerOffset = 0;

	uint8_t senderLen = (uint8_t)g_clientHandle.length();
	headerPayload[headerOffset++] = senderLen;
	memcpy(headerPayload + headerOffset, g_clientHandle.c_str(), senderLen);
	headerOffset += senderLen;

	if (messageLen == 0)
	{
		// Send just the header plus '\0'.
		uint8_t packetPayload[MAXBUF];
		memcpy(packetPayload, headerPayload, headerOffset);
		packetPayload[headerOffset] = '\0';

		int totalLen = headerOffset + 1;
		LOG_DEBUG("Sending empty broadcast packet: flag=" << BROADCAST_PACKET
														  << ", totalLen=" << totalLen);
		LOG_DEBUG("Payload hex: " << hexDump(packetPayload, totalLen));

		pdu.sendBuf(socketNum, packetPayload, totalLen, BROADCAST_PACKET);
		return;
	}

	// Otherwise, segment the message if needed.
	while (pos < messageLen)
	{
		int segmentLength = (messageLen - pos > MAX_SEGMENT) ? MAX_SEGMENT : (messageLen - pos);
		uint8_t packetPayload[MAXBUF];

		memcpy(packetPayload, headerPayload, headerOffset);
		memcpy(packetPayload + headerOffset, message + pos, segmentLength);
		packetPayload[headerOffset + segmentLength] = '\0';

		int totalLen = headerOffset + segmentLength + 1;
		LOG_DEBUG("Sending broadcast packet: flag=" << BROADCAST_PACKET
													<< ", segmentLength=" << segmentLength
													<< ", totalLen=" << totalLen);
		LOG_DEBUG("Payload hex: " << hexDump(packetPayload, totalLen));

		int bytesSent = pdu.sendBuf(socketNum, packetPayload, totalLen, BROADCAST_PACKET);
		connStats.recordSent(bytesSent);
		connStats.recordMessageSent();
		pos += segmentLength;
	}
}

// ---------------------------------------------------------------------------
// Handles the %L command (list request).
// ---------------------------------------------------------------------------
// Reads exactly 'n' bytes from the socket using MSG_WAITALL.
// Returns 'n' on success or -1 if the connection is closed or an error occurs.
int readNBytes(int socketNum, uint8_t *buffer, int n)
{
	int totalRead = 0;
	while (totalRead < n)
	{
		int bytesRead = recv(socketNum, buffer + totalRead, n - totalRead, MSG_WAITALL);
		if (bytesRead <= 0)
		{
			return -1; // Connection closed or error
		}
		totalRead += bytesRead;
	}
	return totalRead;
}

void handleListCommand(int socketNum)
{
	LOG_DEBUG("handleListCommand: Using socket " << socketNum << " to send list request (flag 0x0A).");
	PDU_Send_And_Recv pdu;
	// Send the list request (header-only PDU).
	if (pdu.sendBuf(socketNum, nullptr, 0, LIST_REQUEST_PACKET) != 3)
	{
		LOG_ERROR("handleListCommand: Failed to send list request.");
		return;
	}

	// Read the 3-byte header for the list count PDU.
	uint8_t header[3] = {0};
	if (readNBytes(socketNum, header, 3) != 3)
	{
		LOG_ERROR("handleListCommand: Failed to read list count header.");
		return;
	}
	uint16_t pduLen = ntohs(*(uint16_t *)header);
	int receivedFlag = header[2];
	int payloadLen = pduLen - 3;
	LOG_DEBUG("handleListCommand: List count header received: PDU length = "
			  << pduLen << ", flag = 0x" << std::hex << receivedFlag
			  << ", payload length = " << payloadLen);
	if (receivedFlag != LIST_RESPONSE_NUM || payloadLen != 4)
	{
		LOG_ERROR("handleListCommand: Expected flag 0x0B with 4 payload bytes, but got flag 0x"
				  << std::hex << receivedFlag << " and payload length " << payloadLen);
		return;
	}

	uint8_t countBuffer[4] = {0};
	if (readNBytes(socketNum, countBuffer, 4) != 4)
	{
		LOG_ERROR("handleListCommand: Failed to read list count payload.");
		return;
	}
	uint32_t numHandles;
	memcpy(&numHandles, countBuffer, sizeof(uint32_t));
	numHandles = ntohl(numHandles);
	cout << "Number of clients: " << numHandles << endl;

	// Receive each handle PDU.
	for (uint32_t i = 0; i < numHandles; i++)
	{
		if (readNBytes(socketNum, header, 3) != 3)
		{
			LOG_ERROR("handleListCommand: Failed to read handle header.");
			return;
		}
		pduLen = ntohs(*(uint16_t *)header);
		receivedFlag = header[2];
		payloadLen = pduLen - 3;
		LOG_DEBUG("handleListCommand: Handle header received: PDU length = "
				  << pduLen << ", flag = 0x" << std::hex << receivedFlag
				  << ", payload length = " << payloadLen);
		if (receivedFlag != LIST_RESPONSE_HANDLE || payloadLen < 1)
		{
			LOG_ERROR("handleListCommand: Expected flag 0x0C with at least 1 payload byte, but got flag 0x"
					  << std::hex << receivedFlag << " and payload length " << payloadLen);
			continue;
		}
		uint8_t handleBuffer[MAX_NAME_LEN + 1] = {0};
		if (readNBytes(socketNum, handleBuffer, payloadLen) != payloadLen)
		{
			LOG_ERROR("handleListCommand: Failed to read handle payload.");
			return;
		}
		uint8_t handleLen = handleBuffer[0];
		if (payloadLen < (1 + handleLen))
		{
			LOG_ERROR("handleListCommand: Incomplete handle payload received.");
			continue;
		}
		char handle[MAX_NAME_LEN + 1] = {0};
		memcpy(handle, handleBuffer + 1, handleLen);
		handle[handleLen] = '\0';
		cout << handle << endl;
	}

	// Receive the end-of-list marker.
	if (readNBytes(socketNum, header, 3) != 3)
	{
		LOG_ERROR("handleListCommand: Failed to read end-of-list header.");
		return;
	}
	pduLen = ntohs(*(uint16_t *)header);
	receivedFlag = header[2];
	payloadLen = pduLen - 3;
	LOG_DEBUG("handleListCommand: End-of-list header received: PDU length = "
			  << pduLen << ", flag = 0x" << std::hex << receivedFlag
			  << ", payload length = " << payloadLen);
	if (receivedFlag != LIST_RESPONSE_END || payloadLen != 0)
	{
		LOG_ERROR("handleListCommand: Expected end marker flag 0x0D with no payload, but got flag 0x"
				  << std::hex << receivedFlag << " and payload length " << payloadLen);
	}
	else
	{
		LOG_DEBUG("handleListCommand: End-of-list marker received successfully.");
	}
}

// ---------------------------------------------------------------------------
// Handles the %E command (exit).
// ---------------------------------------------------------------------------
void handleExitCommand(int socketNum)
{
	LOG_DEBUG("Sending exit command packet: flag=" << EXIT_PACKET);
	PDU_Send_And_Recv pdu;
	pdu.sendBuf(socketNum, nullptr, 0, EXIT_PACKET);

	int flag;
	uint8_t dataBuffer[MAXBUF] = {0};
	int len = pdu.recvBuf(socketNum, dataBuffer, &flag);
	LOG_DEBUG("Received response to exit, flag=" << flag << ", len=" << len);

	if (flag == 9)
	{
		cout << "Exit ACK received. Closing connection." << endl;
	}
	else
	{
		cout << "Expected exit ACK (flag 9), but received flag " << flag << endl;
	}
}
