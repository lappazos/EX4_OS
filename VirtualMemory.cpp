#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <utility>
#include <cmath>

typedef std::pair<uint64_t, uint64_t> FrameTuple;

#define ROOT_TABLE_SIZE VIRTUAL_ADDRESS_WIDTH % OFFSET_WIDTH

FrameTuple findFrameFromPage(uint64_t page);

void clearTable(uint64_t frameIndex)
{
    for (uint64_t i = 0; i < PAGE_SIZE; ++ i)
    {
        PMwrite(frameIndex * PAGE_SIZE + i, 0);
    }
}

double cyclicDistanceCalc(uint64_t &pageA, uint64_t pageB)
{
    auto a = (int64_t) pageA;
    auto b = (int64_t) pageB;
    return std::fmin((int64_t) NUM_PAGES - std::abs(a - b), std::abs(a - b));
}

/**
 * @brief finds empty frame. if yes - delete the refrence to it. if not - update page to evict and maxFrameIndex
 * @param frameIndex
 * @param vAddress
 * @param maxFrameIndex
 * @param cyclicDistance
 * @param pageToEvict
 * @param targetPage
 * @param treeDepth
 * @param lastAddedFrame
 * @param father
 * @return
 */
uint64_t
traversTree(uint64_t frameIndex, uint64_t vAddress, uint64_t &maxFrameIndex, double &cyclicDistance,
            uint64_t &pageToEvict, uint64_t &targetPage, int treeDepth, uint64_t lastAddedFrame, uint64_t father)
{
    // check the page frame num
    if (frameIndex > maxFrameIndex)
    {
        maxFrameIndex = frameIndex;
    }
    // reached leaf - real page
    if (treeDepth == TABLES_DEPTH)
    {
        double dist = cyclicDistanceCalc(targetPage, (int64_t) vAddress);
        if (dist > cyclicDistance)
        {
            cyclicDistance = dist;
            pageToEvict = vAddress;
        }
        return 0;
    }
    bool emptyFrame = true;
    word_t nextFrameIndex;
    for (int i = 0; i < PAGE_SIZE; ++ i)
    {
        PMread(((uint64_t) frameIndex) * PAGE_SIZE + i, &nextFrameIndex);
        if (nextFrameIndex)
        {
            emptyFrame = false;
            uint64_t returnFrame = traversTree((uint64_t) nextFrameIndex,
                                               (vAddress << OFFSET_WIDTH) + i, maxFrameIndex,
                                               cyclicDistance, pageToEvict, targetPage,
                                               treeDepth + 1, lastAddedFrame, ((uint64_t) frameIndex) * PAGE_SIZE + i);
            // check if found an empty frame
            if (returnFrame)
            {
                return returnFrame;
            }
        }
    }
    if (emptyFrame && (frameIndex != lastAddedFrame))
    {
        PMwrite(father, 0);
        return frameIndex;

    }
    return 0;
}

/**
 * @brief find frame to fill and prepare accordingly
 * @param virtualAddress
 * @param lastAddedFrame
 * @return
 */
uint64_t getFrameToFill(uint64_t lastAddedFrame)
{
    uint64_t pageToEvict;
    uint64_t targetPage; // todo need to be reset
    double bestCyclicDistance = 0;
    uint64_t max_frame = 0;
    uint64_t emptyPage = traversTree(0, 0, max_frame, bestCyclicDistance, pageToEvict, targetPage,
                                     0, lastAddedFrame, 0);
    // 1 prio - if found empty with 0
    if (emptyPage)
    {
        // father cleared inside traverseTree
        return emptyPage;
    }
        // 2 prio - if there is unused frame
    else if (max_frame + 1 < NUM_FRAMES)
    {
        clearTable((uint64_t) max_frame + 1);
        return max_frame + 1;
    }
    // 3 prio - swap page
    FrameTuple frameTuple = findFrameFromPage(pageToEvict);
    PMevict(frameTuple.second, pageToEvict);
    clearTable(frameTuple.second);
    // clear father
    PMwrite(frameTuple.first, 0);
    return frameTuple.second;
}

/**
 * @brief
 * @param page virtual address without offset
 * @return
 */
FrameTuple findFrameFromPage(uint64_t page)
{
    word_t nextFrameIndex = 0;
    // uint64_t previousFrame; // todo check
    uint64_t father = 0;
    uint64_t lastAddedFrame = 0;
    bool shouldRestore = false;
    for (int i = 0; i < TABLES_DEPTH; i ++)
    {
        uint64_t currOffsetIndex = (uint64_t) page & (((1UL << OFFSET_WIDTH) - 1)
                << ((TABLES_DEPTH - 1 - i) * OFFSET_WIDTH));
        father = PAGE_SIZE * nextFrameIndex + currOffsetIndex;
        PMread(father, &nextFrameIndex);
        if (! nextFrameIndex)
        {
            shouldRestore = true;
            lastAddedFrame = getFrameToFill(lastAddedFrame);
            PMwrite(father, (word_t) lastAddedFrame);
            nextFrameIndex = (word_t) lastAddedFrame;
        }
    }
    if (shouldRestore)
    {
        PMrestore((uint64_t)nextFrameIndex, page);
    }
    return {father, (uint64_t) nextFrameIndex};


}


void VMinitialize()
{
    clearTable(0);
}

int VMread(uint64_t virtualAddress, word_t *value)
{
    if (virtualAddress > VIRTUAL_MEMORY_SIZE)
    {
        return 0;
    }
    FrameTuple frameTuple = findFrameFromPage(virtualAddress >> OFFSET_WIDTH);
    uint64_t frame = frameTuple.second;
    auto offset = (uint64_t) (virtualAddress & ((1UL << OFFSET_WIDTH) - 1));
    PMread(PAGE_SIZE * frame + offset, value);
    return 1;
}


int VMwrite(uint64_t virtualAddress, word_t value)
{
    if (virtualAddress > VIRTUAL_MEMORY_SIZE)
    {
        return 0;
    }
    FrameTuple frameTuple = findFrameFromPage(virtualAddress >> OFFSET_WIDTH);
    uint64_t frame = frameTuple.second;
    auto offset = (uint64_t) (virtualAddress & ((1UL << OFFSET_WIDTH) - 1));
    PMwrite(PAGE_SIZE * frame + offset, value);
    return 1;
}
