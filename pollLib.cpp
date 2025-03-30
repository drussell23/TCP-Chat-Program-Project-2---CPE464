//
// Written Hugh Smith, Updated: April 2020
// Use at your own risk.  Feel free to copy, just leave my name in it.
//


#include <poll.h>
#include <stdlib.h>
#include <stdio.h>

#include "safeUtil.h"
#include "pollLib.h"


// Poll global variables 
static struct pollfd * pollFileDescriptors;
static int maxFileDescriptor = 0;
static int currentPollSetSize = 0;

static void growPollSet(int newSetSize);

// Poll functions (setup, add, remove, call)
void setupPollSet()
{
	currentPollSetSize = POLL_SET_SIZE;
	pollFileDescriptors = (struct pollfd *) sCalloc(POLL_SET_SIZE, sizeof(struct pollfd));
}


void addToPollSet(int socketNumber)
{
	
	if (socketNumber >= currentPollSetSize)
	{
		// needs to increase off of the biggest socket number since
		// the file desc. may grow with files open or sockets
		// so socketNumber could be much bigger than currentPollSetSize
		growPollSet(socketNumber + POLL_SET_SIZE);		
	}
	
	if (socketNumber + 1 >= maxFileDescriptor)
	{
		maxFileDescriptor = socketNumber + 1;
	}

	pollFileDescriptors[socketNumber].fd = socketNumber;
	pollFileDescriptors[socketNumber].events = POLLIN;
}

void removeFromPollSet(int socketNumber)
{
	pollFileDescriptors[socketNumber].fd = 0;
	pollFileDescriptors[socketNumber].events = 0;
}

int pollCall(int timeInMilliSeconds)
{
    int i = 0;
    int returnValue = -1;
    int pollValue = 0;
    
    pollValue = poll(pollFileDescriptors, maxFileDescriptor, timeInMilliSeconds);
    if (pollValue < 0)
    {
        perror("pollCall");
        exit(-1);
    }
    
    // Log the poll return value.
    printf("[DEBUG] pollCall: poll() returned %d. Checking file descriptors...\n", pollValue);
    
    if (pollValue > 0)
    {
        // Check which file descriptor(s) are ready.
        for (i = 0; i < maxFileDescriptor; i++)
        {
            if (pollFileDescriptors[i].revents > 0)
            {
                printf("[DEBUG] pollCall: FD %d has revents: 0x%x\n", i, pollFileDescriptors[i].revents);
                returnValue = i;
                break;
            }
        }
    }
    else
    {
        printf("[DEBUG] pollCall: No file descriptors ready (timeout).\n");
    }
    
    return returnValue;
}

static void growPollSet(int newSetSize)
{
	int i = 0;
	
	// just check to see if someone screwed up
	if (newSetSize <= currentPollSetSize)
	{
		printf("Error - current poll set size: %d newSetSize is not greater: %d\n",
			currentPollSetSize, newSetSize);
		exit(-1);
	}
	
	printf("Increasing poll set from: %d to %d\n", currentPollSetSize, newSetSize);
	pollFileDescriptors = (pollfd*)(srealloc(pollFileDescriptors, newSetSize * sizeof(struct pollfd)));	
	
	// zero out the new poll set elements
	for (i = currentPollSetSize; i < newSetSize; i++)
	{
		pollFileDescriptors[i].fd = 0;
		pollFileDescriptors[i].events = 0;
	}
	
	currentPollSetSize = newSetSize;
}



