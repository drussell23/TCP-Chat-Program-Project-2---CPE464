#include "NLPProcessor.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>
#include <vector>

// Utility function: Trim whitespace from both ends of a string.
static inline std::string trim(const std::string &s)
{
    auto start = s.find_first_not_of(" \t\n\r");
    
    if (start == std::string::npos)
        return "";
    auto end = s.find_last_not_of(" \t\n\r");
   
    return s.substr(start, end - start + 1);
}

NLPProcessor::NLPProcessor()
{
    // Initialize any NLP models or resources if needed.
}

NLPProcessor::~NLPProcessor()
{
    // Cleanup resources if necessary.
}

std::string NLPProcessor::processMessage(const std::string &message)
{
    // Run the message through the NLP pipeline:
    // Preprocess, then recognize intent, and finally generate the structured command.
    std::string cleaned = preprocess(message);
    std::string intent = recognizeIntent(cleaned);
    std::string command = generateResponse(intent, cleaned);
    return command;
}

std::string NLPProcessor::preprocess(const std::string &message)
{
    // Convert the input message to lowercase for easier keyword matching.
    std::string lower = message;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c)
                   { return std::tolower(c); });
    return trim(lower);
}

std::string NLPProcessor::recognizeIntent(const std::string &message)
{
    // Use simple heuristics to determine the user's intent.
    if (message.find("list") != std::string::npos && message.find("client") != std::string::npos)
    {
        return "list";
    }
    if (message.find("broadcast") != std::string::npos)
    {
        return "broadcast";
    }
    if (message.find("send") != std::string::npos && message.find("message") != std::string::npos && message.find("to") != std::string::npos)
    {
        return "send_message";
    }
    if (message.find("status") != std::string::npos)
    {
        return "status";
    }
    if (message.find("exit") != std::string::npos || message.find("quit") != std::string::npos)
    {
        return "exit";
    }
    return "unknown";
}

std::string NLPProcessor::generateResponse(const std::string &intent, const std::string &message)
{
    // Translate the recognized intent into a structured command.
    if (intent == "list")
    {
        // The %L command requests the list of connected clients.
        return "%L";
    }
    else if (intent == "broadcast")
    {
        // For broadcasting, extract the text after the keyword "broadcast".
        size_t pos = message.find("broadcast");
        std::string msgBody = "";
        if (pos != std::string::npos)
        {
            msgBody = trim(message.substr(pos + 9)); // Skip "broadcast " (8 letters + 1 space)
        }
        if (msgBody.empty())
        {
            return "Error: No broadcast message provided. Please include a message after 'broadcast'.";
        }
        std::ostringstream oss;
        oss << "%B " << msgBody;
        return oss.str();
    }
    else if (intent == "send_message")
    {
        // For sending messages, we assume a simple natural language format like:
        // "send a message to <destination> <message text>"
        // We use "to" as the separator.
        std::istringstream iss(message);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token)
        {
            tokens.push_back(token);
        }
        // Find the position of the keyword "to".
        int posTo = -1;
        for (size_t i = 0; i < tokens.size(); i++)
        {
            if (tokens[i] == "to")
            {
                posTo = static_cast<int>(i);
                break;
            }
        }
        if (posTo == -1 || posTo + 1 >= static_cast<int>(tokens.size()))
        {
            return "Error: No destination specified for message. Use 'send message to <destination> <message>'.";
        }
        // Assume the token immediately after "to" is the destination handle.
        std::string destination = tokens[posTo + 1];
        // The remainder of the tokens form the message text.
        std::ostringstream msgStream;
        for (size_t i = posTo + 2; i < tokens.size(); i++)
        {
            msgStream << tokens[i] << " ";
        }
        std::string msgBody = trim(msgStream.str());
        if (msgBody.empty())
        {
            return "Error: No message text provided. Please include the text to send after the destination.";
        }
        // Format the command as per assignment requirements:
        // %M <number of destinations> <destination handle> <message text>
        // Here, for simplicity, we assume one destination.
        std::ostringstream oss;
        oss << "%M 1 " << destination << " " << msgBody;
        return oss.str();
    }
    else if (intent == "status")
    {
        // The %S command requests the current connection status.
        return "%S";
    }
    else if (intent == "exit")
    {
        // The %E command instructs the client to exit.
        return "%E";
    }
    else
    {
        // If the intent is unrecognized, provide a helpful error message.
        return "Error: Unrecognized command. Try 'list clients', 'broadcast <message>', 'send message to <destination> <message>', 'status', or 'exit'.";
    }
}
