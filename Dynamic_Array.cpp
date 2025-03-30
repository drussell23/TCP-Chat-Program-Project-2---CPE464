#include "Dynamic_Array.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
using namespace std;

#define INITIAL_CAPACITY 10
#define RESIZE_FACTOR 2

// Private helper: Resizes the internal array to newCapacity.
void Dynamic_Array::resize(int newCapacity)
{
    Entry_Handle_Table *newArray = (Entry_Handle_Table *)calloc(newCapacity, sizeof(Entry_Handle_Table));
    if (newArray == NULL)
    {
        perror("calloc failed in resize");
        exit(EXIT_FAILURE);
    }
    // Copy the current contents to the new array.
    memcpy(newArray, array, capacity * sizeof(Entry_Handle_Table));
    free(array);
    array = newArray;
    capacity = newCapacity;
}

// Constructor: Initializes the dynamic array with INITIAL_CAPACITY.
Dynamic_Array::Dynamic_Array()
{
    capacity = INITIAL_CAPACITY;
    array = (Entry_Handle_Table *)calloc(capacity, sizeof(Entry_Handle_Table));
    if (array == NULL)
    {
        perror("calloc failed in Dynamic_Array constructor");
        exit(EXIT_FAILURE);
    }
    count = 0;
}

// Destructor: Frees the allocated array.
Dynamic_Array::~Dynamic_Array()
{
    free(array);
}

// Adds a new entry to the array.
// Returns 1 on success, -1 if the handle already exists.
int Dynamic_Array::addElement(const Handling &handle, int newSocketNumber)
{
    // Check for duplicate handle.
    for (int i = 0; i < capacity; i++)
    {
        if (array[i].handle.handleLength != 0)
        { // Entry in use.
            if (compareHandles(array[i].handle, handle))
            {
                return -1; // Duplicate found.
            }
        }
    }
    // Look for an empty slot.
    for (int i = 0; i < capacity; i++)
    {
        if (array[i].handle.handleLength == 0)
        { // Empty slot.
            array[i].handle.handleLength = handle.handleLength;
            array[i].socketNumber = newSocketNumber;
            memcpy(array[i].handle.handle, handle.handle, handle.handleLength);
            // Null-terminate if possible.
            if (handle.handleLength < MAXIMUM_CHARACTERS)
                array[i].handle.handle[handle.handleLength] = '\0';
            count++;
            return 1;
        }
    }
    // If no empty slot was found and the array is full, resize.
    if (count == capacity)
    {
        resize(capacity * RESIZE_FACTOR);
    }
    // Insert element in the first available slot after resizing.
    for (int i = 0; i < capacity; i++)
    {
        if (array[i].handle.handleLength == 0)
        {
            array[i].handle.handleLength = handle.handleLength;
            array[i].socketNumber = newSocketNumber;
            memcpy(array[i].handle.handle, handle.handle, handle.handleLength);
            if (handle.handleLength < MAXIMUM_CHARACTERS)
                array[i].handle.handle[handle.handleLength] = '\0';
            count++;
            return 1;
        }
    }
    return 1; // Should never be reached.
}

// Removes an entry matching the provided handle name.
void Dynamic_Array::removeElement(const char *handleName)
{
    for (int i = 0; i < capacity; i++)
    {
        if (array[i].handle.handleLength != 0)
        {
            if (strcmp(array[i].handle.handle, handleName) == 0)
            {
                array[i].handle.handleLength = 0; // Mark as empty.
                memset(array[i].handle.handle, 0, MAXIMUM_CHARACTERS);
                count--;
                break; // Assuming handles are unique.
            }
        }
    }
}

// Helper: Finds the index of an entry by its socket number; returns -1 if not found.
int Dynamic_Array::findElementIndexBySocket(int socketNumber) const
{
    for (int i = 0; i < capacity; i++)
    {
        if (array[i].handle.handleLength != 0 && array[i].socketNumber == socketNumber)
        {
            return i;
        }
    }
    return -1;
}

// Removes an entry by its socket number.
void Dynamic_Array::removeElementBySocket(int socketNumber)
{
    int index = findElementIndexBySocket(socketNumber);
    if (index != -1)
    {
        array[index].handle.handleLength = 0;
        memset(array[index].handle.handle, 0, MAXIMUM_CHARACTERS);
        count--;
    }
}

// Searches for an entry matching the given handle name.
// Returns the corresponding socket number, or -1 if not found.
int Dynamic_Array::getSocketForHandle(const char *handleName) const
{
    for (int i = 0; i < capacity; i++)
    {
        if (array[i].handle.handleLength != 0)
        {
            if (strcmp(array[i].handle.handle, handleName) == 0)
            {
                return array[i].socketNumber;
            }
        }
    }
    return -1;
}

// Compares two Handling structures.
// Returns true if their lengths are equal and each character matches.
bool Dynamic_Array::compareHandles(const Handling &h1, const Handling &h2) const
{
    if (h1.handleLength != h2.handleLength)
        return false;

    for (int i = 0; i < h1.handleLength; i++)
    {
        if (h1.handle[i] != h2.handle[i])
            return false;
    }
    return true;
}

// Prints the contents of the dynamic array for debugging.
void Dynamic_Array::printTable() const
{
    cout << "Dynamic Array Table (Count: " << count << ", Capacity: " << capacity << "):" << endl;
    for (int i = 0; i < capacity; i++)
    {
        if (array[i].handle.handleLength != 0)
        {
            cout << "Index " << i << ": Handle = " << array[i].handle.handle
                 << ", Socket = " << array[i].socketNumber << endl;
        }
    }
}
