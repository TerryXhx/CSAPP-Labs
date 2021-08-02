#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "cachelab.h"

struct cacheLine {
    int valid;
    unsigned long long tag;
    int lastUsedTime;
};

void cacheVisit (struct cacheLine *cacheSet, unsigned long long tag, int E, int time, int *pHit, int *pMiss, int *pEviction, int verboseFlag) {
    int vacantLineIndex = -1;
    for (int i = 0; i < E; ++i) {
        if (cacheSet[i].valid) {
            if (cacheSet[i].tag == tag) {
                ++(*pHit);
                cacheSet[i].lastUsedTime = time;

                if (verboseFlag)
                    printf(" hit");
                return;
            }
        } else 
            vacantLineIndex = i;
    }

    ++(*pMiss);
    if (verboseFlag)
        printf(" miss");    
    if (vacantLineIndex != -1) {
        cacheSet[vacantLineIndex].valid = 1;
        cacheSet[vacantLineIndex].tag = tag;
        cacheSet[vacantLineIndex].lastUsedTime = time;
        return;
    }

    ++(*pEviction);
    if (verboseFlag)
        printf(" eviction");
    int minIndex = -1, minTime = 0x7fffffff;
    for (int i = 0; i < E; ++i) {
        if (cacheSet[i].lastUsedTime < minTime) {
            minIndex = i;
            minTime = cacheSet[i].lastUsedTime;
        }
    }
    cacheSet[minIndex].tag = tag;
    cacheSet[minIndex].lastUsedTime = time;

    return;
}

int main(int argc, char **argv) {
    // get cache args from command-line args and open file
    int opt;
    int helpFlag, verboseFlag;
    int s, E, b;
    FILE *traceFile;
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
            case 'h':
                helpFlag = 1;
                break;
            case 'v':
                verboseFlag = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                traceFile = fopen(optarg, "r");
                break;
            default:
                printf("Usage: ./csim [-hv] -s <s> -E <E> -b <b> -t <tracefile>");
                exit(EXIT_FAILURE);
       }
    }

    // display help info and exit
    if (helpFlag) {
        printf(
            "Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n"
            "Options:\n"
            "-h         Print this help message.\n"
            "-v         Optional verbose flag.\n"
            "-s <num>   Number of set index bits.\n"
            "-E <num>   Number of lines per set.\n"
            "-b <num>   Number of block offset bits.\n"
            "-t <file>  Trace file.\n"
            "\n"
            "Examples:\n"
            "linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n"
            "linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n" 
        );

        return 1;
    }

    // cache initialization
    struct cacheLine cache[1 << s][E];
    for (int i = 0; i < (1 << s); ++i) 
        for (int j = 0; j < E; ++j) 
            cache[i][j].valid = 0;
    
    char operation;
    unsigned long long address;
    int size;
    
    // time and counters initialization
    int time = 0, hits = 0, misses = 0, evictions = 0;

    // simulation over tracefile input
    while (fscanf(traceFile, "%c %llx, %d\n", &operation, &address, &size) > 0) {
        unsigned long long tag = address >> (b + s);
        int setIndex = (address >> b) & ((1 << s) - 1);

        if (verboseFlag) 
            printf("%c %llx, %d", operation, address, size);
                
        switch (operation) {
            case 'I':
                break;
            case 'L':
                cacheVisit(cache[setIndex], tag, E, time, &hits, &misses, &evictions, verboseFlag);
                break;
            case 'S':
                cacheVisit(cache[setIndex], tag, E, time, &hits, &misses, &evictions, verboseFlag); 
                break;
            case 'M':
                cacheVisit(cache[setIndex], tag, E, time, &hits, &misses, &evictions, verboseFlag);
                ++hits;
                if (verboseFlag)
                    printf(" hit");
                break;
        }

        if (verboseFlag)
            printf("\n");
        ++time;
    }
    fclose(traceFile);

    printSummary(hits, misses, evictions);
    return 0;
}