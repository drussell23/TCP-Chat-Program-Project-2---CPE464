#ifndef NLPPROCESSOR_H
#define NLPPROCESSOR_H

#include <string>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>
#include <vector>
#include <map>

using namespace std;

class NLPProcessor
{
public:
    // A simple enumeration for the dialogue state.
    enum DialogueState
    {
        Idle,
        AwaitingDestination,
        AwaitingMessage
    };

    // A structure to hold a pending command (only send_message is handled here).
    struct PendingCommand
    {
        string commandType; // For now, only "send_message" is used.
        string destination; // Destination handle (if provided).
        string messageText; // Message text (if provided).
    };

    NLPProcessor();
    ~NLPProcessor();

    // Processes an incoming message and returns either:
    // - A fully structured command (e.g., "%M 1 alice hello there")
    // - A clarifying prompt if the input is incomplete
    string processMessage(const string &message);

private:
    // Preprocesses the input (e.g., converts to lowercase, trims, tokenizes).
    string preprocess(const string &message);

    // Corrects common spelling mistakes and performs additional normalization.
    string correctSpelling(const string &message);

    // Recognizes the intent based on the preprocessed (and spell-corrected) message.
    // Now this function delegates to our ML-style classifier.
    string recognizeIntent(const string &message);

    // Our new ML-style function that classifies user intent.
    string classifyIntent(const string &message);

    // Generates a structured command or clarifying prompt based on the recognized intent.
    string generateResponse(const string &intent, const string &message);

    // Continues a pending command using new input.
    string continuePendingCommand(const string &newInput);

    // Resets the pending command and dialogue state.
    void resetPendingCommand();

    DialogueState currentState;
    PendingCommand pendingCommand;
};

#endif // NLPPROCESSOR_H