#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

// Use the same definitions as in your assignment.
#pragma pack(push, 1)
struct PDU_Header
{
    uint16_t PDU_Length; // in network byte order
    uint8_t flag;
};
#pragma pack(pop)

#define CLIENT_INIT_PACKET_TO_SERVER 1
#define MAXIMUM_CHARACTERS 100

using namespace std;

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        cerr << "Usage: " << argv[0] << " <server_ip> <port> <handle>" << endl;
        exit(1);
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    const char *handle = argv[3];

    // Create a TCP socket.
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        exit(1);
    }

    // Connect to the server.
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        exit(1);
    }
    cout << "Connected to " << server_ip << " on port " << port << endl;

    // Build the registration payload.
    uint8_t handleLen = strlen(handle);
    if (handleLen > MAXIMUM_CHARACTERS)
    {
        cerr << "Handle exceeds maximum allowed length." << endl;
        exit(1);
    }

    uint8_t payload[256];
    payload[0] = handleLen;
    memcpy(payload + 1, handle, handleLen);

    // Total PDU length = header (3 bytes) + payload (1 + handleLen).
    uint16_t totalLength = sizeof(PDU_Header) + 1 + handleLen;
    PDU_Header header;
    header.PDU_Length = htons(totalLength);
    header.flag = CLIENT_INIT_PACKET_TO_SERVER;

    // Build full packet.
    uint8_t packet[512];
    memcpy(packet, &header, sizeof(header));
    memcpy(packet + sizeof(header), payload, 1 + handleLen);

    // Send registration packet.
    if (send(sock, packet, totalLength, 0) < 0)
    {
        perror("send");
        exit(1);
    }
    cout << "Registration packet sent for handle: " << handle << endl;

    // Receive response.
    uint8_t respHeaderBuffer[sizeof(PDU_Header)];
    int bytesRead = recv(sock, respHeaderBuffer, sizeof(PDU_Header), MSG_WAITALL);
    if (bytesRead <= 0)
    {
        cout << "Server terminated connection." << endl;
        close(sock);
        exit(1);
    }

    PDU_Header *respHeader = reinterpret_cast<PDU_Header *>(respHeaderBuffer);
    uint16_t respPDU_Length = ntohs(respHeader->PDU_Length);
    uint8_t respFlag = respHeader->flag;
    cout << "Response received. Flag: " << (int)respFlag << endl;

    close(sock);
    return 0;
}
