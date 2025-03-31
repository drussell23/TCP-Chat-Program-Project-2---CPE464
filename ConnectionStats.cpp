#include "ConnectionStats.h"

ConnectionStats::ConnectionStats() : totalBytesSent(0), totalBytesReceived(0), totalMessagesSent(0), totalMessagesReceived(0) {
    startTime = chrono::steady_clock::now();
}

void ConnectionStats::recordSent(int bytes) {
    totalBytesSent += bytes;
}

void ConnectionStats::recordReceived(int bytes) {
    totalBytesReceived += bytes;
}

void ConnectionStats::recordMessageSent() {
    totalMessagesSent++;
}

void ConnectionStats::recordMessageReceived() {
    totalMessagesReceived++;
}

double ConnectionStats::getUptimeSeconds() const {
    chrono::duration<double> uptime = chrono::steady_clock::now() - startTime;
    return uptime.count();
}

void ConnectionStats::printStats() const {
    std::cout << "---------------------------" << std::endl;
    std::cout << "Connection Statistics:" << std::endl;
    std::cout << "Uptime: " << getUptimeSeconds() << " seconds" << std::endl;
    std::cout << "Total Bytes Sent: " << totalBytesSent << std::endl;
    std::cout << "Total Bytes Received: " << totalBytesReceived << std::endl;
    std::cout << "Total Messages Sent: " << totalMessagesSent << std::endl;
    std::cout << "Total Messages Received: " << totalMessagesReceived << std::endl;
    std::cout << "---------------------------" << std::endl;
}
