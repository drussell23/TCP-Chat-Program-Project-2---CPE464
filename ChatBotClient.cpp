// ChatBotClient.cpp
#include "ChatBotClient.h"
#include "networks.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cstdio>

ChatBotClient::ChatBotClient(const std::string &serverAddress, int port, const std::string &botHandle)
    : serverAddress(serverAddress), port(port), botHandle(botHandle), socketNum(-1), nlpProcessor()
{
}

ChatBotClient::~ChatBotClient()
{
    if (socketNum != -1)
    {
        close(socketNum);
    }
}

bool ChatBotClient::connectToServer()
{
    // Convert server address and port into C-style strings
    char serverAddr[256];
    std::strncpy(serverAddr, serverAddress.c_str(), sizeof(serverAddr));
    serverAddr[sizeof(serverAddr) - 1] = '\0';

    char portStr[10];
    std::snprintf(portStr, sizeof(portStr), "%d", port);

    // Use the tcpClientSetup() from networks.h to establish the TCP connection.
    socketNum = tcpClientSetup(serverAddr, portStr, 1);
    if (socketNum < 0)
    {
        std::cerr << "Error: Could not connect to server." << std::endl;
        return false;
    }
    std::cout << "Connected to server at " << serverAddress << ":" << port << std::endl;
    return true;
}

void ChatBotClient::run()
{
    std::string userInput;
    // Main loop: receive natural language commands from the user.
    while (true)
    {
        std::cout << "Enter command (or type 'exit' to quit): ";
        std::getline(std::cin, userInput);
        if (userInput.empty())
            continue;
        if (userInput == "exit")
            break;

        // Process the natural language input using the NLPProcessor.
        std::string structuredCommand = nlpProcessor.processMessage(userInput);

        // If the NLPProcessor returns an error message, display it.
        if (structuredCommand.find("Error:") == 0)
        {
            std::cout << structuredCommand << std::endl;
        }
        else
        {
            // Otherwise, send the structured command to the server.
            sendMessage(structuredCommand);
        }
    }
}

void ChatBotClient::sendMessage(const std::string &message)
{
    // Send the message over the socket.
    int bytesSent = send(socketNum, message.c_str(), message.length(), 0);
    if (bytesSent < 0)
    {
        std::cerr << "Error sending message." << std::endl;
    }
    else
    {
        std::cout << "Sent command: " << message << std::endl;
    }
}

bool ChatBotClient::processIncomingMessage(const std::string &message)
{
    // Check if the message is directed to the bot (e.g., starting with "@" followed by the bot handle)
    std::string trigger = "@" + botHandle;
    return (message.find(trigger) != std::string::npos);
}

// Main function for the chatbot executable.
int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <server address> <server port> <bot handle>" << std::endl;
        return 1;
    }

    std::string serverAddress(argv[1]);
    int port = std::atoi(argv[2]);
    std::string botHandle(argv[3]);

    ChatBotClient chatbot(serverAddress, port, botHandle);
    
    if (!chatbot.connectToServer())
    {
        std::cerr << "Failed to connect to server." << std::endl;
        return 1;
    }
    chatbot.run();
    return 0;
}
