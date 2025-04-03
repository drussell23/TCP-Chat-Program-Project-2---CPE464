#ifndef NLPPROCESSOR_H
#define NLPPROCESSOR_H

#include <string>

using namespace std;

class NLPProcessor {
    public:
        NLPProcessor();
        ~NLPProcessor();

        // Processes an incoming message and returns an appropriate response. 
        string processMessage(const string &message);

    private:
        // Preprocesses the input (e.g., converts to lowercase, trims, tokenizes).
        string preprocess(const string &message);

        // Recognizes the intent based on the preprocessed message. 
        string recognizeIntent(const string &message);

        // Generates a response based on the recognized intent. 
        string generateResponse(const string &intent, const string &message);
};

#endif // NLPPROCESSOR_H