# Revised Makefile for CPE464 TCP Chat Program
# Written by Hugh Smith (original) and modified by Derek

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -ggdb -MMD -MP \
           -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1 \
           -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include

# Define any additional libraries here if needed.
LIBS = 

# List of object files for each target.
# Make sure the file "networks.cpp" exists with exactly this name!
CLIENT_OBJS = cclient.o networks.o gethostbyname.o PDU_Send_And_Recv.o pollLib.o safeUtil.o
SERVER_OBJS = server.o networks.o gethostbyname.o PDU_Send_And_Recv.o pollLib.o safeUtil.o Dynamic_Array.o

all: cclient server

cclient: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o cclient $(CLIENT_OBJS) $(LIBS)

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o server $(SERVER_OBJS) $(LIBS)

# ---- NEW SECTION FOR test_register ----
TEST_REGISTER_OBJS = test_register.o networks.o gethostbyname.o PDU_Send_And_Recv.o pollLib.o safeUtil.o
test_register: $(TEST_REGISTER_OBJS)
	$(CXX) $(CXXFLAGS) -o test_register $(TEST_REGISTER_OBJS) $(LIBS)

# Pattern rule to compile .cpp files into .o files.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f cclient server test_register *.o *.d

.PHONY: all clean
