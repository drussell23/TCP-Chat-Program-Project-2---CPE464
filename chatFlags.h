#ifndef CHAT_FLAGS_H
#define CHAT_FLAGS_H

#include <cstring>

// -----------------------------------------------------------------------------
// X-macro table for chat packet flags.
// Each entry defines a flag name, its value, and a description of what it represents.
#define CHAT_FLAGS \
    /* Registration packet from client to server. */ \
    X(CLIENT_INIT_PACKET_TO_SERVER, 1, "Registration packet from client to server") \
    /* Confirmation packet sent from server to client indicating a successful registration. */ \
    X(CONFIRM_GOOD_HANDLE, 2, "Confirmation of good handle") \
    /* Error packet sent from server if registration fails (e.g., duplicate handle). */ \
    X(ERROR_ON_INIT_PACKET, 3, "Error on registration (e.g., duplicate handle)") \
    /* Packet sent from client to server to broadcast a message to all clients. */ \
    X(BROADCAST_PACKET, 4, "Broadcast message") \
    /* Direct message packet sent from client to server for forwarding to one or more clients. */ \
    X(MESSAGE_PACKET, 5, "Direct message") \
    /* Exit notification sent from client to server when the client is exiting. */ \
    X(CLIENT_TO_SERVER_EXIT, 8, "Exit notification") \
    /* List request packet sent from client to server to request the list of connected handles. */ \
    X(CLIENT_TO_SERVER_LIST_OF_HANDLES, 10, "List request") \
    /* Packet sent from server containing a 4-byte number indicating the count of connected handles. */ \
    X(LIST_RESPONSE_NUM, 0x0B, "List response: handle count") \
    /* Packet sent from server with one handle (1-byte length followed by the handle name). */ \
    X(LIST_RESPONSE_HANDLE, 0x0C, "List response: a single handle") \
    /* Packet sent from server signaling the end of the list response. */ \
    X(LIST_RESPONSE_END, 0x0D, "List response: end marker") \
    /* Exit acknowledgement sent from server to client confirming exit. */ \
    X(EXIT_ACK, 9, "Exit acknowledgement")

// -----------------------------------------------------------------------------
// Generate an enum for the chat flags using the X-macro.
enum ChatPacketFlag {
#define X(flag, value, desc) flag = value,
    CHAT_FLAGS
#undef X
};

// -----------------------------------------------------------------------------
// Structure to hold detailed information about each chat flag.
struct ChatFlagInfo {
    ChatPacketFlag flag;
    const char* name;         // Name of the flag, e.g. "MESSAGE_PACKET"
    const char* description;  // Description of what the flag represents.
};

// Create a constant array containing all chat flag information.
static const ChatFlagInfo chatFlagInfos[] = {
#define X(flag, value, desc) { flag, #flag, desc },
    CHAT_FLAGS
#undef X
};

// Calculate the number of chat flags.
static const int numChatFlags = sizeof(chatFlagInfos) / sizeof(ChatFlagInfo);

// -----------------------------------------------------------------------------
// Inline function: Check if a given flag value is one of the defined chat flags.
inline bool isValidChatFlag(int flag) {
    for (int i = 0; i < numChatFlags; ++i) {
        if (chatFlagInfos[i].flag == flag) {
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// Inline function: Convert a chat flag value to its string representation.
// Returns the flag name if found, or "UNKNOWN" if not recognized.
inline const char* chatFlagToString(int flag) {
    for (int i = 0; i < numChatFlags; ++i) {
        if (chatFlagInfos[i].flag == flag) {
            return chatFlagInfos[i].name;
        }
    }
    return "UNKNOWN";
}

// -----------------------------------------------------------------------------
// Inline function: Get the description of a given chat flag.
// Returns a human-readable description or a default message if not recognized.
inline const char* chatFlagDescription(int flag) {
    for (int i = 0; i < numChatFlags; ++i) {
        if (chatFlagInfos[i].flag == flag) {
            return chatFlagInfos[i].description;
        }
    }
    return "No description available";
}

// -----------------------------------------------------------------------------
// Inline function: Convert a flag name string to its corresponding chat flag value.
// Returns the flag value if found, or -1 if not recognized.
inline int chatFlagFromString(const char* flagStr) {
    for (int i = 0; i < numChatFlags; ++i) {
        if (std::strcmp(chatFlagInfos[i].name, flagStr) == 0) {
            return chatFlagInfos[i].flag;
        }
    }
    return -1; // Flag not found.
}

#endif // CHAT_FLAGS_H
