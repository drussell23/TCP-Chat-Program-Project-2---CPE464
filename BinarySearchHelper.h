#ifndef BINARY_SEARCH_HELPER_H
#define BINARY_SEARCH_HELPER_H

#include "Dynamic_Array.h" // For the Entry_Handle_Table and Handling definitions
#include <cstring>
#include <stdexcept>
#include <iostream>

// Macro to log debug messages (only active in debug mode)
#ifdef DEBUG
#define LOG_DEBUG(msg) std::cerr << "[DEBUG] " << msg << std::endl
#else
#define LOG_DEBUG(msg)
#endif

// Macro to check if a pointer is null
#define CHECK_NULL(ptr)   \
    if ((ptr) == nullptr) \
    throw std::invalid_argument(#ptr " is null")

class BinarySearchHelper
{
public:
    // Compares two Handling structures lexicographically.
    // Returns negative if h1 < h2, zero if equal, positive if h1 > h2.
    static int comparesHandles(const Handling &h1, const Handling &h2)
    {
        // Determine the minimum length between the two handles.
        int minLen = (h1.handleLength < h2.handleLength) ? h1.handleLength : h2.handleLength;
        
        // Use case-insensitive comparison for the first minLen characters.
        int cmp = strncasecmp(h1.handle, h2.handle, minLen);
        
        // If they match for the common prefix, the shorter handle is considered "less".
        if (cmp == 0)
            return h1.handleLength - h2.handleLength;
        
        return cmp;
    }    

    // Performs a binary search on a sorted array of Entry_Handle_Table.
    // 'array' is assumed to be sorted from index 0 to count-1.
    // Returns the index if found; otherwise, returns -1.
    static int binarySearch(const Entry_Handle_Table *array, int count, const Handling &target)
    {
        try
        {
            CHECK_NULL(array);
            if (count < 0)
                throw std::invalid_argument("count is negative");
            int low = 0;
            int high = count - 1;

            while (low <= high)
            {
                int mid = low + (high - low) / 2;
                int cmp = comparesHandles(array[mid].handle, target);

                if (cmp == 0)
                {
                    return mid; // Found.
                }
                else if (cmp < 0)
                {
                    low = mid + 1;
                }
                else
                {
                    high = mid - 1;
                }
            }
            return -1; // Not found.
        }
        catch (const std::exception &e)
        {
            LOG_DEBUG("Exception in binarySearch: " << e.what());
            return -1;
        }
    }

    // Finds the insertion index for target in a sorted array.
    // Returns the index where the new element should be inserted to keep the array sorted.
    static int findInsertionIndex(const Entry_Handle_Table *array, int count, const Handling &target)
    {
        try
        {
            CHECK_NULL(array);
            if (count < 0)
                throw std::invalid_argument("count is negative");
            int low = 0;
            int high = count;

            while (low < high)
            {
                int mid = low + (high - low) / 2;
                int cmp = comparesHandles(array[mid].handle, target);

                if (cmp < 0)
                {
                    low = mid + 1;
                }
                else
                {
                    high = mid;
                }
            }
            return low;
        }
        catch (const std::exception &e)
        {
            LOG_DEBUG("Exception in findInsertionIndex: " << e.what());
            // Return 0 as a safe default if an error occurs.
            return 0;
        }
    }
};

#endif // BINARY_SEARCH_HELPER_H
