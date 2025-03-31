#ifndef PDU_SEND_AND_RECV_H
#define PDU_SEND_AND_RECV_H

#include <cstdint>

using namespace std; 

// Use pragma pack to ensure the header is exactly 3 bytes.
#pragma pack(push, 1)
struct PDU_Header {
    uint16_t PDU_Length;    // Total length of the PDU (header + payload) in network order.
    uint8_t flag;           // Flag indicating the type of packet.
};
#pragma pack(pop)

// Ensure that the header is exactly 3 bytes.
static_assert(sizeof(PDU_Header) == 3, "PDU_Header must be 3 bytes");

// Define a flag indicating that PDU_Header is defined.
#define PDU_HEADER_DEFINED

// Helper macros to stringify macro values.
#define STRINGIZE_HELPER(x) #x
#define STRINGIZE(x) STRINGIZE_HELPER(x)

// Robust macro definitions for SIZE_CHAT_HEADER.
// If OVERRIDE_SIZE_CHAT_HEADER is defined, use it. Otherwise, if SIZE_CHAT_HEADER hasn't been defined,
// define it based on the size of PDU_Header (after ensuring PDU_Header is defined).
#if defined(OVERRIDE_SIZE_CHAT_HEADER)
    #define SIZE_CHAT_HEADER (OVERRIDE_SIZE_CHAT_HEADER)
#else
    #if !defined(SIZE_CHAT_HEADER)
        #if defined(PDU_HEADER_DEFINED)
            #define SIZE_CHAT_HEADER (sizeof(PDU_Header))
        #else
            #error "PDU_Header must be defined (and PDU_HEADER_DEFINED set) before SIZE_CHAT_HEADER"
        #endif
    #endif
#endif

// Emit a compile-time message showing the defined SIZE_CHAT_HEADER.
#pragma message ("SIZE_CHAT_HEADER defined as " STRINGIZE(SIZE_CHAT_HEADER))

// Verify at compile time that SIZE_CHAT_HEADER is exactly 3 bytes.
static_assert(SIZE_CHAT_HEADER == 3, "SIZE_CHAT_HEADER must equal 3 bytes. Please check PDU_Header or your override.");


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