#ifndef CHATBOTCLIENT_H
#define CHATBOTCLIENT_H

#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <unistd.h> // For close()

#include <sys/socket.h> // Socket functions
#include <arpa/inet.h> // For inet_pton(), etc.

#include "networks.h" // Provides tcpClientSetuo() and related network functions
#include "NLPProcessor.h"

using namespace std;

class ChatBotClient {
    public: 
        ChatBotClient(const string &serverAddress, int port, const string &botHandle);
        ~ChatBotClient();

        // Establish connection with the chat server. 
        bool connectToServer();

        // Main loop to receive messages, process them, and send responses.
        void run();

        // Send a message to the server.
        void sendMessage(const string &message);
    
    private:
        string serverAddress;
        int port;
        string botHandle;
        int socketNum; // Socket descriptor for network communication.
        NLPProcessor nlpProcessor; // NLP module to process incoming messages. 

        // Checks if a message is directed to the bot.
        bool processIncomingMessage(const string &message);
};

#endif // CHATBOTCLIENT_H