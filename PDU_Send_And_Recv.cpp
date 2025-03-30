#include "PDU_Send_And_Recv.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

#define SIZE_CHAT_HEADER sizeof(PDU_Header)

// Special return value for valid packets with zero-length payload.
// The value “fffffffe” is the hexadecimal representation of –2, which in 
// the implementation is defined as VALID_ZERO_PAYLOAD. When a packet is 
// received with a zero-length payload (as expected for a registration 
// confirmation with flag 2), pdu.recvBuf() returns –2. This tells the 
// client that the packet is valid even though there’s no payload.
#define VALID_ZERO_PAYLOAD -2 

// Helper function to dump raw bytes in hex as a string.
static std::string hexDump(const uint8_t *buffer, int length)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < length; i++) {
        oss << std::setw(2) << static_cast<int>(buffer[i]) << " ";
    }
    return oss.str();
}

// Helper function to print raw bytes in hex directly.
void debugHexDump(const uint8_t *buffer, int length)
{
    cout << hexDump(buffer, length) << endl;
}

// Receives a PDU: first reads the header, then the payload.
int PDU_Send_And_Recv::recvBuf(int clientSocket, uint8_t *dataBuffer, int *flag)
{
    PDU_Header header;
    int bytesRead = recv(clientSocket, &header, SIZE_CHAT_HEADER, MSG_WAITALL);
    if (bytesRead < 0)
    {
        perror("recv error on header");
        exit(EXIT_FAILURE);
    }
    if (bytesRead == 0)
    {
        // Connection closed while reading header.
        return -1;
    }

    // Debug: Print the raw header bytes.
    cout << "[DEBUG] Raw header bytes: ";
    debugHexDump((uint8_t *)&header, SIZE_CHAT_HEADER);

    // Convert the PDU length from network byte order.
    uint16_t pduLength = ntohs(header.PDU_Length);
    *flag = header.flag;

    cout << "[DEBUG] Parsed header: PDU_Length = " << pduLength
         << ", flag = " << (int)header.flag << endl;

    int payloadLength = pduLength - SIZE_CHAT_HEADER;
    cout << "[DEBUG] Computed payload length = " << payloadLength << endl;

    if (payloadLength == 0)
    {
        // Valid packet with zero payload.
        cout << "PDU received:" << endl;
        cout << "PDU Size: " << pduLength << " Flag: " << (int)header.flag
             << " Payload: " << endl << endl;
        return VALID_ZERO_PAYLOAD;
    }

    int payloadBytes = recv(clientSocket, dataBuffer, payloadLength, MSG_WAITALL);
    if (payloadBytes < 0)
    {
        perror("recv error on payload");
        exit(EXIT_FAILURE);
    }
    if (payloadBytes == 0)
    {
        // Connection closed during payload reception.
        return -1;
    }

    // Logging the received PDU.
    cout << "PDU received:" << endl;
    cout << "PDU Size: " << pduLength << " Flag: " << (int)header.flag << " Payload: ";
    for (int i = 0; i < payloadLength; i++)
    {
        cout << hex << (int)dataBuffer[i] << " ";
    }
    cout << "\n\n";

    return payloadBytes;
}

// Sends a PDU: constructs a header, prepends it to the payload, and sends the complete buffer.
int PDU_Send_And_Recv::sendBuf(int socketNumber, uint8_t *dataBuffer, uint16_t lengthOfData, int flagValue)
{
    // Total PDU length = header size + payload length.
    uint16_t totalLength = lengthOfData + SIZE_CHAT_HEADER;

    // Construct header.
    PDU_Header header;
    header.PDU_Length = htons(totalLength);
    header.flag = static_cast<uint8_t>(flagValue);

    // Create a buffer to hold the complete PDU.
    uint8_t PDU_Buffer[totalLength];
    memcpy(PDU_Buffer, &header, SIZE_CHAT_HEADER);
    if (lengthOfData > 0)
    {
        memcpy(PDU_Buffer + SIZE_CHAT_HEADER, dataBuffer, lengthOfData);
    }

    // Debug: Dump the assembled header and payload.
    cout << "[DEBUG] Assembled complete PDU:" << endl;
    cout << "[DEBUG] Total PDU size: " << totalLength << " bytes" << endl;
    cout << "[DEBUG] Header bytes: ";
    debugHexDump((uint8_t *)&header, SIZE_CHAT_HEADER);
    if (lengthOfData > 0)
    {
        cout << "[DEBUG] Payload bytes: " << hexDump(dataBuffer, lengthOfData) << endl;
    }

    // Loop to ensure the entire PDU is sent.
    int totalSent = 0;
    while (totalSent < totalLength)
    {
        int bytesSent = send(socketNumber, PDU_Buffer + totalSent, totalLength - totalSent, 0);
        if (bytesSent < 0)
        {
            perror("send error");
            exit(EXIT_FAILURE);
        }
        if (bytesSent == 0)
        {
            perror("connection closed during send");
            exit(EXIT_FAILURE);
        }
        totalSent += bytesSent;
        cout << "[DEBUG] sendBuf: Sent " << bytesSent << " bytes, total sent: " 
             << totalSent << " of " << totalLength << " bytes" << endl;
    }

    // Final logging.
    cout << "PDU sent:" << endl;
    cout << "PDU Size: " << totalLength << " Flag: " << (int)header.flag << " Payload: ";
    for (int i = 0; i < lengthOfData; i++)
    {
        cout << hex << (int)dataBuffer[i] << " ";
    }
    cout << "\n\n";

    return totalSent;
}
