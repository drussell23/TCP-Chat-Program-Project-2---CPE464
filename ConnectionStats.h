#ifndef CONNECTION_STATS_H
#define CONNECTION_STATS_H

#include <chrono>
#include <iostream>

using namespace std;

class ConnectionStats
{
    public:
        // Constructor: Initialize counters and record the connection start time.
        ConnectionStats();

        // Record that a packet of 'bytes' has been sent. 
        void recordSent(int bytes);

        // Record that a packet of 'bytes' has been recieved. 
        void recordReceived(int bytes);

        // Record that a message (a complete PDU) was sent.
        void recordMessageSent();

        // Record that a message (a complete PDU) was received.
        void recordMessageReceived();

        // Get the elapsed time in seconds since the connection started. 
        double getUptimeSeconds() const;

        // Print all current statistics to standard output.
        void printStats() const;

    private:
        chrono::steady_clock::time_point startTime;
        int totalBytesSent;
        int totalBytesReceived;
        int totalMessagesSent;
        int totalMessagesReceived;
};

#endif // CONNECTION_STATS_H