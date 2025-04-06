#include "NLPProcessor.h"

// Utility function: Trim whitespace from both ends of a string.
static inline std::string trim(const std::string &s)
{
    auto start = s.find_first_not_of(" \t\n\r");

    if (start == std::string::npos)
        return "";
    auto end = s.find_last_not_of(" \t\n\r");

    return s.substr(start, end - start + 1);
}

NLPProcessor::NLPProcessor() : currentState(Idle)
{
    // Initialize any NLP models or resources if needed.
    resetPendingCommand();
}

NLPProcessor::~NLPProcessor()
{
    // Cleanup if needed.
}

void NLPProcessor::resetPendingCommand()
{
    pendingCommand.commandType = "";
    pendingCommand.destination = "";
    pendingCommand.messageText = "";
    currentState = Idle;
}

string NLPProcessor::correctSpelling(const string &message)
{
    // A very simple correction using a lookup table.
    static const map<string, string> corrections = {
        {"helo", "hello"},
        {"teh", "the"},
        {"mesage", "message"},
        {"recieve", "receive"},
        {"adress", "address"}};

    istringstream iss(message);
    ostringstream oss;
    string word;
    bool first = true;

    while (iss >> word)
    {
        // Check for punctuation at the end.
        string punctuation = "";

        if (!word.empty() && ispunct(word.back()))
        {
            punctuation = word.back();
            word.pop_back();
        }

        auto it = corrections.find(word);

        if (it != corrections.end())
            word = it->second;

        if (!first)
            oss << " ";

        oss << word << punctuation;
        first = false;
    }
    return oss.str();
}

std::string NLPProcessor::processMessage(const std::string &message)
{
    // Preprocess the message.
    string cleaned = preprocess(message);

    // Perform spell correction.
    string corrected = correctSpelling(cleaned);

    // If there's a pending command, use new input to complete it.
    if (currentState != Idle)
        return continuePendingCommand(corrected);

    // Recognize intent and generate response.
    string intent = recognizeIntent(corrected);
    string command = generateResponse(intent, corrected);
    return command;
}

std::string NLPProcessor::preprocess(const std::string &message)
{
    // Convert the input message to lowercase for easier keyword matching.
    string lower = message;
    transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c)
              { return std::tolower(c); });
    return trim(lower);
}

std::string NLPProcessor::recognizeIntent(const string &message)
{
    // Instead of our old heuristic, we now delegate to the ML-style classifier.
    return classifyIntent(message);
}

string NLPProcessor::classifyIntent(const string &message)
{
    // A simple bag-of-words model as our ML-style classifier.
    // Define a mapping from intent names to a list of associated keywords.
    map<string, vector<string>> intentKeywords = {
        {"list", {"list", "client", "clients", "show", "display"}},
        {"broadcast", {"broadcast", "all", "everyone"}},
        {"send_message", {"send", "message", "to", "deliver"}},
        {"status", {"status", "connection", "info"}},
        {"exit", {"exit", "quit", "close", "bye"}}};

    // Tokenize the message.
    istringstream iss(message);
    vector<string> tokens;
    string token;

    while (iss >> token)
        tokens.push_back(token);

    // Score each intent by counting keyword matches.
    map<string, int> scores;

    for (const auto &pair : intentKeywords)
    {
        scores[pair.first] = 0;
        for (const auto &keyword : pair.second)
        {
            for (const auto &word : tokens)
            {
                if (word == keyword)
                    scores[pair.first]++;
            }
        }
    }

    // Select the intent with the highest score.
    string bestIntent = "unknown";
    int maxScore = 0;

    for (const auto &pair : scores)
    {
        if (pair.second > maxScore)
        {
            maxScore = pair.second;
            bestIntent = pair.first;
        }
    }
    return bestIntent;
}

string NLPProcessor::generateResponse(const string &intent, const string &message)
{
    // Reset state if complete command is formed.
    currentState = Idle;

    if (intent == "list")
    {
        return "%L";
    }
    else if (intent == "broadcast")
    {
        size_t pos = message.find("broadcast");
        string msgBody = "";

        if (pos != string::npos)
            msgBody = trim(message.substr(pos + 9)); // Skip "broadcast "

        if (msgBody.empty())
            return "Error: No broadcast message provided. Please include a message after 'broadcast'.";

        ostringstream oss;
        oss << "%B " << msgBody;
        return oss.str();
    }
    else if (intent == "send_message")
    {
        // Tokenize the input.
        istringstream iss(message);
        vector<string> tokens;
        string token;
        while (iss >> token)
            tokens.push_back(token);

        // Look for "to" as separator.
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
            // Missing destination: set pending state.
            currentState = AwaitingDestination;
            pendingCommand.commandType = "send_message";
            return "Please specify a destination handle for your message.";
        }

        // Set the destination. Here we trim any extra spaces.
        pendingCommand.commandType = "send_message";
        pendingCommand.destination = trim(tokens[posTo + 1]);

        std::cout << "[DEBUG] Extracted destination handle: '"
                  << pendingCommand.destination
                  << "' with length: " << pendingCommand.destination.size() << std::endl;

        // Get message text from remaining tokens.
        ostringstream msgStream;
        for (size_t i = posTo + 2; i < tokens.size(); i++)
            msgStream << tokens[i] << " ";
        string msgBody = trim(msgStream.str());

        if (msgBody.empty())
        {
            currentState = AwaitingMessage;
            return "Please provide the message text to send after specifying the destination.";
        }

        // Save the destination before resetting the pending command.
        string destination = pendingCommand.destination;
        resetPendingCommand();

        // Build and return the structured command.
        ostringstream oss;
        oss << "%M 1 " << destination << " " << msgBody;
        return oss.str();
    }
    else if (intent == "status")
    {
        return "%S";
    }
    else if (intent == "exit")
    {
        return "%E";
    }
    else
    {
        return "Error: Unrecognized command. Try 'list clients', 'broadcast <message>', 'send message to <destination> <message>', 'status', or 'exit'.";
    }
}

string NLPProcessor::continuePendingCommand(const string &newInput)
{
    // When awaiting destination, treat new input as the destination handle.
    if (currentState == AwaitingDestination)
    {
        string dest = trim(newInput);
        if (dest.empty())
            return "Destination cannot be empty. Please specify a valid handle.";
        pendingCommand.destination = dest;
        currentState = AwaitingMessage;
        return "Destination set to '" + dest + "'. Now, please provide the message text.";
    }

    // When awaiting message text, treat new input as the message text.
    if (currentState == AwaitingMessage)
    {
        string msgText = trim(newInput);

        if (msgText.empty())
            return "Message text cannot be empty. Please provide the text for your message.";

        pendingCommand.messageText = msgText;

        // Build the full structured command.
        ostringstream oss;
        oss << "%M 1 " << pendingCommand.destination << " " << pendingCommand.messageText;
        resetPendingCommand();

        return oss.str();
    }

    // If state is somehow invalid, reset.
    resetPendingCommand();

    return "Error: Unable to process pending command. Please try again.";
}
