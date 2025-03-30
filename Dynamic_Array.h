#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <cstddef>
#include <cstring>
#include <iostream>

using namespace std;

const int MAXIMUM_CHARACTERS = 100;

// Represents a client handle.
struct Handling
{
    char handleLength;               // Length of the handle (should be <= MAXIMUM_CHARACTERS)
    char handle[MAXIMUM_CHARACTERS]; // The handle name as a fixed-size char array
};

// Represents an entry in the handle table, mapping a client handle to a socket.
struct Entry_Handle_Table
{
    int socketNumber; // Socket descriptor for the client.
    Handling handle;  // Client handle information.
};

// A dynamic array to maintain a table of client handles and their corresponding sockets.
class Dynamic_Array
{
private:
    Entry_Handle_Table *array; // Dynamically allocated array of entries.
    int count;                 // Current number of active entries.
    int capacity;              // Total allocated capacity.

    // Resizes the internal array to the new capacity.
    void resize(int newCapacity);

    // Helper: Finds the index of an entry by its socket number; returns -1 if not found.
    int findElementIndexBySocket(int socketNumber) const;

public:
    // Constructor: Initializes the dynamic array with a default capacity.
    Dynamic_Array();

    // Destructor: Releases any allocated memory.
    ~Dynamic_Array();

    // Adds a new entry to the array.
    // Returns 1 on success, -1 if the handle already exists.
    int addElement(const Handling &handle, int newSocketNumber);

    // Removes an entry matching the provided handle name.
    void removeElement(const char *handleName);

    // Removes an entry by its socket number.
    void removeElementBySocket(int socketNumber);

    // Searches for the entry matching the given handle name.
    // Returns the corresponding socket number or -1 if not found.
    int getSocketForHandle(const char *handleName) const;

    // Compares two Handling structures.
    // Returns true if they are identical (same length and same characters).
    bool compareHandles(const Handling &h1, const Handling &h2) const;

    // Additional helper methods.
    int getCapacity() const { return capacity; }
    Entry_Handle_Table *getArray() const { return array; }
    int getCount() const { return count; }

    // Prints the contents of the table (for debugging).
    void printTable() const;
};

#endif // DYNAMIC_ARRAY_H
