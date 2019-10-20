#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <crypt.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>
#include "toolbox.h"

#define numThreads 1 // Used to limit worker and handler threads. Supposed to be equal to number of cores on target system.

char * inputHash = "61v1CDwwP95bY"; // Target hash. Temporary until file inputs are implemented.

bool systemHalt = false; // Used to signal all pthreads to stop working. There's probably a better way of doing this...

/* Protoype declarations */
bool dictionaryAttack(); // Add file pointer etc.
bool bruteForce();
void * bfControl(void * characterCount); // Control routine
void * bfCrack(void * passedObject); // Worker routine
void * bfHandler(void * pthreadID); // Handler routine
void * bfTimeCheck(void * nothing);	// Used to provide time updates.

/* pthread IDs. Worker threads and their corresponding handler threads are limited by the constant numThreads, while
 * control and time threads need only one instance each.
 *
 */
pthread_t workerThreads[numThreads];
pthread_t handlerThread[numThreads];
pthread_t controlThread;
pthread_t timeThread;

pthread_cond_t waitCondition = PTHREAD_COND_INITIALIZER; //Used to halt the control thread until the worker threads wake up.
pthread_mutex_t waitMutex = PTHREAD_MUTEX_INITIALIZER; // Used to lock the wait signal while it is being modified.

bool threadActive[numThreads]; // Indicates which worker threads are active.

/* This structure holds each character set.
 *
 *  characterSet: A string of all the characters in the character set.
 *  characterSpace: The number of characters in the character set.
 *  active: Whether the user has opted to use this character set in their brute force attack. True if yes.
 */
struct charset
{
	char characterSet[95]; // Hardcoded at the limit for ASCII characters. Does not support weird Unicode characters.
	int characterSpace;
	bool active;
};

/* All standard password sets are pre-computed, and are assembled together based on user commands. */
struct charset baseSets[4] = {{"abcdefghijklmnopqrstuvwxyz", 26, false}, {"ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26, false}, \
{"0123456789", 10, false}, {"`~!@#$%^&*()-_=+[{]}\\|;:'\",<.>/ ?", 33, false}};

/* This will be the completed character set used to brute force the passwords. */
struct charset newSet[numThreads];

/* This object is used to pass multiple inputs to pthreads, which can normally only take 1 input for some reason.
 *
 * numCharacters: The number of characters in the password for this current iteration.
 * threadNum: The thread number, used by the control thread.
 */
struct passObject
{
	int numCharacters;
	int threadNum;
};

struct passObject threadPass[numThreads]; // Prepares a thread payload for each worker thread.

/* List of output messages during the character selection process. */
const char * messageList[] = {"What's the maximum number of characters in the password? (Maximum of 20): ", \
"Does the password contain lower case letters? (y/n): ", "Does the password contain upper case letters? (y/n): ", \
"Does the password contain numbers? (y/n): ", "Does the password contain symbols? (y/n): ", \
"Invalid Input. Please try again.\n\n", "You have chosen no character sets. Please try again.\n\n", NULL};

int numCharMax = 0; // Max number of characters in the password.
int numCharCurrent; // Current character being worked on by the algorithm.

/* These are used by the control thread to determine how to assign work to worker threads. */
int controlSpace = 0;
int indexArrayControl[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/* currentWord is the current word to be tested. Character position is determined by the indexArrayWorker.
 * Each thread has its own copy of each to prevent threads from each trying to access the same object.
 * Hardcoded at a max of 20, with the current word size indicated by appropriately place null terminators.
 */
char currentWord[numThreads][20];
int indexArrayWorker[numThreads][20];

unsigned long timeCounter = 0;	// Number of password guesses made so far. Used for speed calculation.
unsigned long totalGuesses = 0; // Total number of guesses needed to run through the entire keyspace.

/*
 * Main loop.
 *
 * argv[0] is input file for hashes, argv[1] is input file for wordlist.
 * 	- Use default wordlist in same directory if no argv[1]
 * 	- If no argv[0], prompt user to enter file name or manually type in hashes.
 * 	- argc <= 2
 *
 * TODO
 *
 * More dynamic user control.
 *
 * Make GUI that has the same functionality and looks prettier.
 */
int main(int argc, char **argv) {

	printf("=========Slowpoke Password Cracker=========\n       \"Why fly when you can walk?\"\n\n");

	char methodChoice[2];

	puts("Choose your option: 1 for dictionary attack, 2 for brute force. Press any other key to exit.");
	fgets(methodChoice, 100 , stdin);

	if(methodChoice[0] == '1')
	{
		printf("This hasn't been implemented yet. Exiting.\n");
		return -1;
	}

	if(methodChoice[0] == '2')
	{
		printf("Brute force selected.\n\n");
		if(bruteForce())
		{
			printf("Brute forcing was successful. Exiting program. \n");
			return 0;
		}

		else
			printf("Brute forcing failed.Exiting program. \n");

		return -1;
	}

	else
		printf("Invalid input. Exiting program.\n");

	return -1;

}

/*
 * TODO
 *
 * Implement.
 */
bool dictionaryAttack()
{
	return false;
}

/* The brute forcing algorithm.
 *
 * This algorithm uses recursion and multithreading to brute force a range of characters up to a range of 20. Character max may
 * become unnecessary once we have quantum computers, but we don't...
 *
 * Currently operates on old DES hashes made with C's crypt(3) function, under the assumption that the hashed and salted password
 * has been retrieved from the user's etc/shadow file. DES character limit is 8.
 *
 * Phase 1: User enters maximum number of characters. Generates a character set for brute forcing that is determined by user input.
 *
 * Phase 2: Starts guessing all possible passwords for a given number of characters.
 *
 * Phase 3: Control thread begins iterating through each possible highest character for the given character number. ie: for 3
 * characters, control thread iterates through all 3rd characters. Worker threads are each assigned the set of permutations for such
 * a character. Because this method is recursive, we have to think backwards...
 *
 * Phase 4: When the control thread gets to the last highest character, it calls itself on it, then begins assigning worker
 * threads to the set of permutations for each second highest character.
 *
 * Phase 5: All permutations are exhausted, and the brute forcing algorithm moves to the next highest number of characters.
 *
 * Phase 6: If a successful password is guessed at any point, all threads halt and the algorithm finishes. Otherwise, the algorithm
 * runs through the entire keyspace and exits on a failure flag.
 *
 * TODO
 *
 * Confirmation message that will enable user to continue, change selections, or manually exit before starting algorithm.
 *
 * Properly implement multithreading.
 * 	-Sync issue with handler threads. Mutexes not working for some reason.
 *
 * Implement mask support.
 *
 * Eliminate need for handler threads?
 *
 * Better halt command implementation.
 *
 * More efficient hashing algorithm + support for more hashing algorithms.
 *
 * Support for hashes with unknown salts.
 * 	-
 * More efficient version of strcmp if necessary.
 *
 * Build and test iterative version to see what the speed difference is.
 *
 * Find way to clear STDIN of excess characters if the user decides to be a dumbass.
 *
 * General optimization.
 *
 */
bool bruteForce()
{

	bool inputFlag; // Used for the user input loop.

	/* Captures buffer from STDIN, hardcoded at 4 characters, as this program will never use more for its input. */
	char inputString[4];

	do // Loops until correct text input has been made by the user.
	{
		inputFlag = false;

		/* This section determines the maximum number of characters in password. */
		puts(messageList[0]);
		fgets(inputString, 100 , stdin);

		/* Highest character limit hardcoded at 20, as anything higher would be stupid. */
		int j = atoiRange(inputString, 1, 20);

		if(j != 0)
			numCharMax = j; // Max characters in password is determined.

		else
			inputFlag = true;

		/* Determines which character sets will be used. Loops until we've gone through all possibilities. */
		for(int i = 1; i < 5; i++)
		{
			if(inputFlag)
				break;

			puts(messageList[i]);
			fgets(inputString, 10 , stdin);

			j = parseConfirm(inputString);

			if(j == -1)
			{
				inputFlag = true;
				break;
			}

			else
				baseSets[i-1].active = j;

		}

		if(inputFlag)
		{
			printf(messageList[5]); // Error message if the user has entered incorrect input.
			numCharMax = 0; // resets number of characters

			for(int i = 0; i < 4; i++) // resets choice of character sets.
				baseSets[i].active = false;
		}

		/* If the user has selected no character sets, start over. */
		if(!inputFlag && (baseSets[0].active | baseSets[1].active | baseSets[2].active | baseSets[3].active) == false)
		{
			printf(messageList[6]);
			numCharMax = 0;
			inputFlag = true;
		}

	}while(inputFlag == true);

	/* Initializes character set for all threads. To ensure parallelism without needing mutexes, each thread will get its own copy
	 * of the completed character set. */

	for(int i = 0; i < 4; i++) // Sums number of possible characters from all included character sets
		newSet[0].characterSpace += baseSets[i].active * baseSets[i].characterSpace;

	for(int i = 0; i < numThreads; i++)
		newSet[i].characterSpace = (int)newSet[0].characterSpace; // Updates character space for all worker threads

	for(int i = 1; i <= numCharMax; i++) // Generates the total number of guesses for the given character range. For time thread.
		totalGuesses += pow(newSet[0].characterSpace, i);

	for(int h = 0; h < numThreads; h++)
	{
		int counter = 0; // Keeps track of index while copying different sets.

		for(int i = 0; i < 4; i++) // Copies all chosen character sets into each worker thread's character set.
		{
			if(baseSets[i].active)
			{
				for(int j = 0; j < baseSets[i].characterSpace; j++)
				{
					newSet[h].characterSet[counter] = baseSets[i].characterSet[j];
					counter++;
				}
			}
		}
	}

	for(int i = 0; i < numThreads; i++) // Initializes all worker threads to the inactive state.
		threadActive[i] = false;

	pthread_create(&timeThread, NULL, bfTimeCheck, NULL); // Starts time thread.

	printf("Brute forcing a password of %d characters. Hold on to your butts.\n", numCharMax);
	/* Brute forcing begins.  numCharMax is desired number of characters. */
	for(int i = 1; i <= numCharMax; i++) //DEBUG
	{
		/* Initializes all elements in the word and index arrays to their lowest value. */
		for(int j = 0; j < numThreads; j++)
		{
			for(int k = 0; k < i; k++)
			{
				indexArrayWorker[j][k] = 0;
				currentWord[j][k] = newSet[0].characterSet[0];
			}

			/* Null terminators are used to determine character length. */
			currentWord[j][i] = '\0';
		}

		/*
		for(int q = 0; q < numThreads; q++) // DEBUG
		{
			printf("Thread %d character set is: %s\n", q, newSet[0].characterSet);
			printf("Its current word is %s\n", &currentWord[q][0]);
		}
		 */

		int numControl = 0; // Needed to pass a pointer to the control thread.


		numControl = i - 1; // Begin by setting the control number to the highest character (valid for indexing).
		controlSpace = (int)newSet[0].characterSpace; // Control thread gets its own value for character space.

		numCharCurrent = i;

		pthread_create(&controlThread, NULL, bfControl, (void*)&numControl); // Starts control thread.

		pthread_join(controlThread, NULL); // Resumes execution once the control thread finishes.

		if(systemHalt) // If we have guessed a correct password, exit with success flag.
			return true;
	}

	return false;

}

/*
 * Control thread.
 */
void * bfControl(void * characterCountVoid)
{
	if(systemHalt)
		return NULL;

	int characterCount = *(int*)characterCountVoid;
	//printf("Beginning control with characters: %d\n", characterCount);
	//printf("IndexArray: %d controlSpace: %d\n",indexArrayControl[characterCount], controlSpace - 1);

	int handlerID = 0; // Used to properly identify handler processes.

	for(int i = 0; i < controlSpace - 1; i++) // Stops after penultimate element.
	{
		if(systemHalt)
			return NULL;

		/* Assigns pthreads until we get to the last call for this control character. Each thread is given
		 * a handler thread to report its status to the control thread. There is probably a better way of doing this. */
		bool assignThreadFlag = false;

		while(assignThreadFlag == false)
		{
			if(systemHalt)
				return NULL;

			//printf("characters in loop: %d, Loop iteration: %d\n", characterCount, i);
			for(int j = 0; j < numThreads && assignThreadFlag == false; j++)
			{
				if(threadActive[j] == false)
				{
					if(characterCount > 0)
						threadPass[j].numCharacters = characterCount - 1; // Worker threads are assigned permutations for highest char.

					else
						threadPass[j].numCharacters = characterCount; // Except in the base case.

					threadPass[j].threadNum = j;

					for(int k = 0; k <= characterCount; k++)	// Updates the worker index with the current permutation set.
						indexArrayWorker[j][k] = indexArrayControl[k];

					currentWord[j][characterCount] = newSet[j].characterSet[i]; // Updates the worker word to reflect new permutation set.

					//printf("About to pass %d characters to thread %d\n", threadPass[j].numCharacters, j);
					pthread_create(&workerThreads[j], NULL, bfCrack, (void*)&(threadPass[j]));
					//printf("Pthread %d ready for action!\n", j);
					threadActive[j] = true;
					handlerID = j;
					pthread_create(&handlerThread[j], NULL, bfHandler, (void*)&handlerID);
					assignThreadFlag = true;
					//printf("Handler thread %d ready for action!\n", j);
				}
			}	// HANDLER THREADS BLOCKING WORKER THREAD EXECUTION DURING MULTITHREADING - WHY?

			if(!assignThreadFlag) // Mutexes don't work for some reason, so this is a temporary workaround.
			{
				usleep(5);
				/*
				pthread_mutex_lock(&waitMutex);
				pthread_cond_wait(&waitCondition, &waitMutex);
				pthread_mutex_unlock(&waitMutex);
				*/
			}

			usleep(5);
		}

		if(characterCount == 0) // At the end of the base case, kill the thread.
			return NULL;

		if(systemHalt)
			return NULL;

		indexArrayControl[characterCount]++; // Our permutation is finished, so the control's index array must be updated.

	} // DEBUG: This isn't terminating at the right point.

	//printf("Character before threadPass: %c by %d\n", currentWord[0][indexArrayControl[characterCount]], indexArrayControl[characterCount]);

	characterCount -= 1;

	bfControl((void*)&characterCount); // On the last element for this section, we jump to the next smallest character.

	return NULL;
}

/*
 * Main cracking method for the brute forcing algorithm. Uses recursive calls to brute force the password.
 *
 * Must be void and take only one input because of pthread limitations.
 *
 * characterCount is desired number of characters - 1.
 *
 */
void * bfCrack(void * passedObject)
{
	if(systemHalt)
		return NULL;

	struct passObject passed = *(struct passObject*) passedObject;

	int characters = passed.numCharacters; // Pulls character number from structure passed to thread.

	int threadID = passed.threadNum; // Pulls ID from structure passed to thread.

	//printf("Thread %d is starting the crack subroutine for characters %d.\n", threadID, characters);

	/* This is the main loop that actually does all the cracking. */
	if(characters == 0)
	{
		for(int i = 0; i < newSet[threadID].characterSpace; i++)
		{

			currentWord[threadID][0] = newSet[threadID].characterSet[i];

			char salt[2] = {inputHash[0], inputHash[1]}; //Salt is first 2 chars of hash, as per CS50's case.

			//printf("Thread %d is about to hash %s, which has %d characters.\n", threadID, currentWord[threadID], characters);

			if(strcmp(crypt(currentWord[threadID], salt), inputHash) == 0)
			{
				systemHalt = true;
				printf("\nThe password for %s is %s\n\n", inputHash, currentWord[threadID]);

				//printf("\nThe password is %s\n", threadID, currentWord[threadID], crypt(currentWord[threadID], salt));
				return NULL;
			}

			//printf("Thread %d just guessed %s which hashed to %s\n", threadID, currentWord[threadID], crypt(currentWord[threadID], salt));
			timeCounter++;
		}
	}

	/* Makes more calls until we get to the base case. In this way, the entire keyspace is traversed. */
	else
	{
		for(int i = 0; i < newSet[threadID].characterSpace; i++)
		{
			if(systemHalt)	//Need to catch calls from higher pthreads.
				return NULL;

			//printf("before shift: %d\n", characters);
			struct passObject passMe = {characters - 1, threadID}; // Used so we can pass a pointer to the next function call.

			//printf("Thread %d on %d characters traversing to %d characters.\n", threadID, characters, characters - 1);

			bfCrack((void*)&(passMe));

			if(systemHalt)	// Needed to stop returns from branching calls here.
				return NULL;

			indexArrayWorker[threadID][characters] = i;
			currentWord[threadID][characters] = newSet[threadID].characterSet[i];
		}
	}


	//printf("Exiting method...\n");
	//printf("Characters: %d\n", characters);
	//printf("Character before exiting: %d %d\n", indexArrayWorker[threadID][characters], indexArrayWorker[threadID][characters + 1]);
	return NULL;

}

/*
 * Used to determine when a pthread is terminated so it can update the status bit and allow a new one to be created.
 *
 * Looking like mutexes are not needed.
 *
 * Pthreads automatically kill themselves when finishing original method call.
 *
 * processID: The assigned ID of the pThread.
 */

void * bfHandler(void * pthreadID)
{
	int processID = *(int*)pthreadID;
	//printf("Handler thread %d initiated!\n", processID);

	pthread_join(workerThreads[processID], NULL);
	/*
	pthread_mutex_lock(&waitMutex);
	pthread_cond_signal(&waitCondition);
	pthread_mutex_unlock(&waitMutex);

	printf("Thread %d is finished. \n", processID);
	*/

	threadActive[processID] = false; // No mutex necessary.
	return NULL;
}

/*
 * Used to give the user regular status updates on the speed of the cracker and the time remaining.
 *
 * The total number of guesses is computed before the
 * Needs a parameter because of pthread limitations. (Does it?)
 */
void * bfTimeCheck(void * nothing)
{
	unsigned long startPosition = 0, seekPosition = 0, numGuesses = 0;

	while(systemHalt == false)
	{
		numGuesses = 0;

		sleep(10);

		startPosition = timeCounter;
		sleep(1);
		seekPosition = timeCounter;

		numGuesses = seekPosition - startPosition;

		system("clear"); // clears the terminal.

		printf("On %d of %d characters. \n", numCharCurrent, numCharMax);
		printf("Performing %lu guesses per second\n", numGuesses);
		timeConvert(totalGuesses/numGuesses);

	}

	return NULL;
}
