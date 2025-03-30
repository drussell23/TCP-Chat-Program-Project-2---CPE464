#include "Dynamic_Array.h"
#include "BinarySearchHelper.h" // New helper for binary search functionality

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <exception>

using namespace std;

#define INITIAL_CAPACITY 10
#define RESIZE_FACTOR 2

// Private helper: Resizes the internal array to newCapacity.
void Dynamic_Array::resize(int newCapacity)
{
    try
    {
        Entry_Handle_Table *newArray = (Entry_Handle_Table *)calloc(newCapacity, sizeof(Entry_Handle_Table));
        if (newArray == NULL)
        {
            perror("calloc failed in resize");
            exit(EXIT_FAILURE);
        }
        // Copy the current sorted contents to the new array.
        memcpy(newArray, array, capacity * sizeof(Entry_Handle_Table));
        free(array);
        array = newArray;
        capacity = newCapacity;
    }
    catch (const std::exception &e)
    {
        cerr << "Exception in Dynamic_Array::resize: " << e.what() << endl;
        exit(EXIT_FAILURE);
    }
}

// Constructor: Initializes the dynamic array with INITIAL_CAPACITY.
Dynamic_Array::Dynamic_Array()
{
    try
    {
        // Validate that the initial capacity is a positive integer.
        if (INITIAL_CAPACITY <= 0)
        {
            throw std::runtime_error("INITIAL_CAPACITY must be greater than zero.");
        }

        capacity = INITIAL_CAPACITY;
        array = (Entry_Handle_Table *)calloc(capacity, sizeof(Entry_Handle_Table));

        if (array == nullptr)
        {
            perror("calloc failed in Dynamic_Array constructor");
            throw std::bad_alloc();
        }

        count = 0;

        // Debug log: Indicate successful construction.
#ifdef DEBUG
        std::cout << "[DEBUG] Dynamic_Array constructor: Created array with capacity "
                  << capacity << std::endl;
#endif
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in Dynamic_Array constructor: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
}

// Destructor: Frees the allocated array and logs cleanup.
Dynamic_Array::~Dynamic_Array()
{
    try
    {
        if (array != nullptr)
        {
            // Optionally log the number of elements before cleanup.
#ifdef DEBUG
            std::cout << "[DEBUG] Dynamic_Array destructor: Freeing array with "
                      << count << " element(s)." << std::endl;
#endif
            free(array);
            array = nullptr; // Prevent dangling pointer issues.
        }
#ifdef DEBUG
        else
        {
            std::cout << "[DEBUG] Dynamic_Array destructor: Array pointer already null." << std::endl;
        }
#endif
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in Dynamic_Array destructor: " << e.what() << std::endl;
    }
}

// Adds a new entry to the array in sorted order.
// Returns 1 on success, -1 if the handle already exists.
int Dynamic_Array::addElement(const Handling &handle, int newSocketNumber)
{
    try
    {
        // Use binary search to check for duplicate.
        int index = BinarySearchHelper::binarySearch(array, count, handle);

        if (index != -1)
        {
            return -1; // Duplicate found.
        }
        // Determine the correct insertion index to maintain sorted order.
        int insertIndex = BinarySearchHelper::findInsertionIndex(array, count, handle);

        // If the array is full, resize it.
        if (count == capacity)
        {
            resize(capacity * RESIZE_FACTOR);
        }
        // Shift elements to the right to create space.
        for (int i = count; i > insertIndex; i--)
        {
            array[i] = array[i - 1];
        }
        // Insert the new element.
        array[insertIndex].handle.handleLength = handle.handleLength;
        array[insertIndex].socketNumber = newSocketNumber;
        memcpy(array[insertIndex].handle.handle, handle.handle, handle.handleLength);
        // Null-terminate if possible.
        if (handle.handleLength < MAXIMUM_CHARACTERS)
            array[insertIndex].handle.handle[handle.handleLength] = '\0';
        count++;
        return 1;
    }
    catch (const std::exception &e)
    {
        cerr << "Exception in Dynamic_Array::addElement: " << e.what() << endl;
        return -1;
    }
}

// Removes an entry matching the provided handle name.
void Dynamic_Array::removeElement(const char *handleName)
{
    try
    {
        Handling target;
        target.handleLength = strlen(handleName);
        memcpy(target.handle, handleName, target.handleLength);
        // Use binary search to find the target index.
        int index = BinarySearchHelper::binarySearch(array, count, target);
        if (index != -1)
        {
            // Shift elements left to fill the gap.
            for (int i = index; i < count - 1; i++)
            {
                array[i] = array[i + 1];
            }
            // Clear the now-unused last element.
            array[count - 1].handle.handleLength = 0;
            memset(array[count - 1].handle.handle, 0, MAXIMUM_CHARACTERS);
            count--;
        }
    }
    catch (const std::exception &e)
    {
        cerr << "Exception in Dynamic_Array::removeElement: " << e.what() << endl;
    }
}

// Removes an entry by its socket number.
void Dynamic_Array::removeElementBySocket(int socketNumber)
{
    try
    {
        // Since sorting is based on handle, perform a linear search by socket.
        for (int i = 0; i < count; i++)
        {
            if (array[i].socketNumber == socketNumber)
            {
                // Shift elements left to remove the entry.
                for (int j = i; j < count - 1; j++)
                {
                    array[j] = array[j + 1];
                }
                array[count - 1].handle.handleLength = 0;
                memset(array[count - 1].handle.handle, 0, MAXIMUM_CHARACTERS);
                count--;
                break;
            }
        }
    }
    catch (const std::exception &e)
    {
        cerr << "Exception in Dynamic_Array::removeElementBySocket: " << e.what() << endl;
    }
}

// Searches for an entry matching the given handle name using binary search.
// Returns the corresponding socket number, or -1 if not found.
int Dynamic_Array::getSocketForHandle(const char *handleName) const
{
    try
    {
        Handling target;
        target.handleLength = strlen(handleName);
        memcpy(target.handle, handleName, target.handleLength);
        int index = BinarySearchHelper::binarySearch(array, count, target);
        if (index != -1)
        {
            return array[index].socketNumber;
        }
        return -1;
    }
    catch (const std::exception &e)
    {
        cerr << "Exception in Dynamic_Array::getSocketForHandle: " << e.what() << endl;
        return -1;
    }
}

// Compares two Handling structures.
// Returns true if they are identical (same length and same characters).
bool Dynamic_Array::compareHandles(const Handling &h1, const Handling &h2) const
{
    try
    {
        // Validate that handle lengths are within acceptable bounds.
        if (h1.handleLength < 0 || h1.handleLength > MAXIMUM_CHARACTERS ||
            h2.handleLength < 0 || h2.handleLength > MAXIMUM_CHARACTERS)
        {
            throw std::runtime_error("One or both handle lengths are out of range.");
        }

        // If both handles are empty, consider them equal.
        if (h1.handleLength == 0 && h2.handleLength == 0)
        {
#ifdef DEBUG
            std::cout << "[DEBUG] compareHandles: Both handles are empty." << std::endl;
#endif
            return true;
        }

        // If lengths differ, log and return false.
        if (h1.handleLength != h2.handleLength)
        {
#ifdef DEBUG
            std::cout << "[DEBUG] compareHandles: Handle lengths differ ("
                      << (int)h1.handleLength << " vs " << (int)h2.handleLength << ")." << std::endl;
#endif
            return false;
        }

        // Compare the handle contents using memcmp.
        int cmpResult = memcmp(h1.handle, h2.handle, h1.handleLength);
        if (cmpResult != 0)
        {
#ifdef DEBUG
            for (int i = 0; i < h1.handleLength; i++)
            {
                if (h1.handle[i] != h2.handle[i])
                {
                    std::cout << "[DEBUG] compareHandles: Difference at index " << i
                              << " (" << h1.handle[i] << " vs " << h2.handle[i] << ")." << std::endl;
                    break;
                }
            }
#endif
            return false;
        }
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in Dynamic_Array::compareHandles: " << e.what() << std::endl;
        return false;
    }
}

// Prints the contents of the table (for debugging).
void Dynamic_Array::printTable() const
{
    try
    {
        cout << "Dynamic Array Table (Count: " << count << ", Capacity: " << capacity << "):" << endl;
        for (int i = 0; i < count; i++)
        {
            cout << "Index " << i << ": Handle = " << array[i].handle.handle
                 << ", Socket = " << array[i].socketNumber << endl;
        }
    }
    catch (const std::exception &e)
    {
        cerr << "Exception in Dynamic_Array::printTable: " << e.what() << endl;
    }
}
