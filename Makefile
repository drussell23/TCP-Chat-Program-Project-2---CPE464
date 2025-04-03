# Revised Makefile for CPE464 TCP Chat Program
# Written by Hugh Smith (original) and modified by Derek and ChatBot integration

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -ggdb -MMD -MP \
           -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1 \
           -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include

# Define any additional libraries here if needed.
LIBS = 

# Object files for the original client and server targets.
# Updated CLIENT_OBJS now includes NLPProcessor.o since cclient.cpp uses NLP functions.
CLIENT_OBJS = cclient.o NLPProcessor.o networks.o gethostbyname.o PDU_Send_And_Recv.o pollLib.o safeUtil.o ConnectionStats.o
SERVER_OBJS = server.o networks.o gethostbyname.o PDU_Send_And_Recv.o pollLib.o safeUtil.o Dynamic_Array.o

# Object files for the NLP-based ChatBotClient.
CHATBOT_OBJS = ChatBotClient.o NLPProcessor.o networks.o gethostbyname.o pollLib.o safeUtil.o ConnectionStats.o

# Object files for the test_register target.
TEST_REGISTER_OBJS = test_register.o networks.o gethostbyname.o PDU_Send_And_Recv.o pollLib.o safeUtil.o

# Build all targets.
all: cclient server chatbot test_register

cclient: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o cclient $(CLIENT_OBJS) $(LIBS)

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o server $(SERVER_OBJS) $(LIBS)

chatbot: $(CHATBOT_OBJS)
	$(CXX) $(CXXFLAGS) -o chatbot $(CHATBOT_OBJS) $(LIBS)

test_register: $(TEST_REGISTER_OBJS)
	$(CXX) $(CXXFLAGS) -o test_register $(TEST_REGISTER_OBJS) $(LIBS)

# Pattern rule to compile .cpp files into .o files.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f cclient server chatbot test_register *.o *.d

# Run "make rebuild" in the terminal will first execute the clean target and then build everything. 
# Alternatively, we can do the commands from the terminal with "make clean && make".
# Rebuild target: clean then build everything. 
rebuild: clean all 

.PHONY: all clean rebuild
