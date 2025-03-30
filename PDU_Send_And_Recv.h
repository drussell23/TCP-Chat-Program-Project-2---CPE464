#ifndef PDU_SEND_AND_RECV_H
#define PDU_SEND_AND_RECV_H

#include <cstdint>

using namespace std; 

// Use pragma pack to ensure the header is exactly 3 bytes. 
#pragma pack(push, 1)
struct PDU_Header {
    uint16_t PDU_Length;    // Total length of the PDU (header + payload) in network order. (2 bytes)
    uint8_t flag;           // Flag indicating the type of packet. (1 byte) 
};
# pragma pack (pop)

static_assert(sizeof(PDU_Header) == 3, "PDU_Header must be 3 bytes");

class PDU_Send_And_Recv {
    public:
        // Receives a PDU from clientSocket. 
        // The payload (excluding header) is stored in dataBuffer.
        // The flag from the header is output via *flag.
        // Returns the number of payload bytes received or 0 if the connection is closed. 
        int recvBuf(int clientSocket, uint8_t *dataBuffer, int *flag);

        // Sends a PDU on socketNumber using the given payload and flag.
        // lengthOfData is the number of payload bytes.
        // Returns the total number of bytes sent.
        int sendBuf(int socketNumber, uint8_t *dataBuffer, uint16_t lengthOfData, int flag);
};

#endif // PDU_SEND_AND_RECV_H