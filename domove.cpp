#include "Packer.h"

int packer::doDefrag(IItem* resource, std::list<itemLoc_t> fragments)
{
    int stackSize   = resource->StackSize;
    int movesneeded = 4;
    int minsize     = 32;
    if (mConfig.EnableDirtyPackets)
    {
        movesneeded = 1;
        minsize     = 8;
    }

    int defragCount    = 0;
    bool movePerformed = true;
    while (movePerformed)
    {
        if ((mGear.moveCount + movesneeded) >= mConfig.MoveLimit)
            return 0;

        if ((mServer.outgoingChunkUsage + minsize) > MAX_PACKET_ENCRYPTED_BUFFER_SIZE)
            return 0;

        movePerformed            = false;
        itemLoc_t* firstFragment = NULL;
        itemLoc_t* lastFragment = NULL;

        for (std::list<itemLoc_t>::iterator iter = fragments.begin(); iter != fragments.end(); iter++)
        {
            int count = (stackSize - mGear.items[iter->container][iter->index].item.Count);
            if (count == 0) //Don't split up full stacks.
                continue;

            if (firstFragment == NULL)
                firstFragment = &(*iter);
            else if (iter->target != 0) //If the rest are required, we can't do shit.
                lastFragment = &(*iter);
        }

        if ((firstFragment) && (lastFragment))
        {
            int firstcount  = mGear.items[firstFragment->container][firstFragment->index].item.Count;
            int movecount   = min(lastFragment->target, stackSize - firstcount);

            if (mConfig.EnableDirtyPackets)
            {
                pkMoveItem_t packet         = {0};
                packet.originContainer      = lastFragment->container;
                packet.originIndex          = lastFragment->index;
                packet.destinationContainer = firstFragment->container;
                packet.destinationIndex     = firstFragment->index;
                packet.count                = movecount;

                pPacket->addOutgoingPacket_s(0x29, sizeof(pkMoveItem_t), &packet);
                if (mConfig.EnableDebug)
                    printDebug(&packet, resource, "Defrag");
                mGear.moveCount++;
            }
            else
            {
                int firstFreeSlot  = -1;
                int secondFreeSlot = -1;
                if (firstFragment->container == 0)
                    firstFreeSlot = firstFragment->index;
                if (lastFragment->container == 0)
                    secondFreeSlot = lastFragment->index;

                for (int x = 1; x < mGear.containermax[0]; x++)
                {
                    if (mGear.items[0][x].item.Count == 0)
                    {
                        if (firstFreeSlot == -1)
                            firstFreeSlot = x;
                        else if (secondFreeSlot == -1)
                            secondFreeSlot = x;
                    }
                }
                if ((firstFreeSlot == -1) || (secondFreeSlot == -1))
                    return 0;
                
                pkMoveItem_t packet         = {0};
                if (firstFragment->container != 0)
                {
                    //Move the target stack into inventory(firstFreeSlot).
                    packet.originContainer      = firstFragment->container;
                    packet.originIndex          = firstFragment->index;
                    packet.destinationContainer = 0;
                    packet.destinationIndex     = DEFAULT_MOVE_INDEX;
                    packet.count                = firstcount;
                    pPacket->addOutgoingPacket_s(0x29, sizeof(pkMoveItem_t), &packet);
                    if (mConfig.EnableDebug)
                        printDebug(&packet, resource, "Defrag");
                    mGear.moveCount++;
                }

                if (lastFragment->container != 0)
                {
                    //Move the second stack into inventory(secondFreeSlot)
                    packet.originContainer      = lastFragment->container;
                    packet.originIndex          = lastFragment->index;
                    packet.destinationContainer = 0;
                    packet.destinationIndex     = DEFAULT_MOVE_INDEX;
                    packet.count                = movecount;
                    pPacket->addOutgoingPacket_s(0x29, sizeof(pkMoveItem_t), &packet);
                    if (mConfig.EnableDebug)
                        printDebug(&packet, resource, "Defrag");
                    mGear.moveCount++;
                }

                //Move the second stack into the first stack.
                packet.originContainer      = 0;
                packet.originIndex          = secondFreeSlot;
                packet.destinationContainer = 0;
                packet.destinationIndex     = firstFreeSlot;
                packet.count                = movecount;
                pPacket->addOutgoingPacket_s(0x29, sizeof(pkMoveItem_t), &packet);
                if (mConfig.EnableDebug)
                    printDebug(&packet, resource, "Defrag");

                
                if (firstFragment->container != 0)
                {
                    //Move the whole bundle back into the first location.
                    packet.originContainer      = 0;
                    packet.originIndex          = firstFreeSlot;
                    packet.destinationContainer = firstFragment->container;
                    packet.destinationIndex     = DEFAULT_MOVE_INDEX;
                    packet.count                = firstcount + movecount;
                    pPacket->addOutgoingPacket_s(0x29, sizeof(pkMoveItem_t), &packet);
                    if (mConfig.EnableDebug)
                        printDebug(&packet, resource, "Defrag");
                    mGear.moveCount++;
                }
            }

            if ((!mConfig.EnableDirtyPackets) && (firstFragment->container != 0) && (mGear.firstfreespace[firstFragment->container] < firstFragment->index))
            {
                int targetIndex                                                                       = mGear.firstfreespace[firstFragment->container];
                mGear.items[firstFragment->container][mGear.firstfreespace[firstFragment->container]] = mGear.items[firstFragment->container][firstFragment->index];
                mGear.items[firstFragment->container][mGear.firstfreespace[firstFragment->container]].item.Count += movecount;
                mGear.items[firstFragment->container][mGear.firstfreespace[firstFragment->container]].isMoved = true;
                
                
                mGear.items[firstFragment->container][firstFragment->index] = itemSlotInfo_t();
                mGear.items[firstFragment->container][firstFragment->index].isMoved = true;

                mGear.firstfreespace[firstFragment->container] = DEFAULT_MOVE_INDEX;
                for (auto x = 1; x < mGear.containermax[firstFragment->container]; x++)
                {
                    if ((mGear.items[firstFragment->container][x].item.Count == 0) && (mGear.items[firstFragment->container][x].item.Id == 0))
                    {
                        mGear.firstfreespace[firstFragment->container] = x;
                        break;
                    }
                }
                firstFragment->index = targetIndex;
            }
            else
            {
                mGear.items[firstFragment->container][firstFragment->index].item.Count += movecount;
                mGear.items[firstFragment->container][firstFragment->index].isMoved = true;
            }

            lastFragment->target -= movecount;
            mGear.items[lastFragment->container][lastFragment->index].item.Count -= movecount;
            mGear.items[lastFragment->container][lastFragment->index].isMoved = true;
            if (mGear.items[lastFragment->container][lastFragment->index].item.Count == 0)
            {
                mGear.items[lastFragment->container][lastFragment->index]         = itemSlotInfo_t();
                mGear.items[lastFragment->container][lastFragment->index].isMoved = true;
                mGear.freecount[lastFragment->container]++;
                if (lastFragment->index < mGear.firstfreespace[lastFragment->container])
                    mGear.firstfreespace[lastFragment->container] = lastFragment->index;
                for (std::list<itemLoc_t>::iterator iter = fragments.begin(); iter != fragments.end();)
                {
                    if (&(*iter) == lastFragment)
                        iter = fragments.erase(iter);
                    else
                        iter++;
                }
            }
            defragCount++;
            movePerformed = true;
        }
    }
    return defragCount;
}
bool packer::moveItem(int container, int index, int count, int destination, IItem* pResource, const char* process)
{
    if (mGear.moveCount >= mConfig.MoveLimit)
        return false;

    if ((mServer.outgoingChunkUsage + 8) > MAX_PACKET_ENCRYPTED_BUFFER_SIZE)
        return false;

    //Set temporary index and container.
    //If we need to move item through inventory, the second move will use these.
    int tIndex     = index;
    int tContainer = container;

    //Check if we're moving item from another bag into inventory.
    if ((container != 0) && ((destination == 0) || (!mConfig.EnableDirtyPackets)))
    {
        pkMoveItem_t packet         = {0};
        packet.originContainer      = container;
        packet.originIndex          = index;
        packet.destinationContainer = 0;
        packet.destinationIndex     = DEFAULT_MOVE_INDEX;
        packet.count                = count;
        tContainer                  = 0;
        tIndex                      = mGear.firstfreespace[0];

        //If the item is headed for inventory, and we allow dirty packets, see if we can combine the stack.
        if ((mConfig.EnableDirtyPackets) && (pResource->StackSize > 1) && (count < pResource->StackSize))
        {
            for (auto x = 1; x < mGear.containermax[0]; x++)
            {
                if ((mGear.items[0][x].item.Id == pResource->Id) && ((mGear.items[0][x].item.Count + count) <= pResource->StackSize))
                {
                    //If our item matches, and both stacks fit in a slot, move it directly into the slot instead of taking a new one.
                    packet.destinationIndex = x;
                    break;
                }
            }
        }

        updateItems(&packet, destination);
        pPacket->addOutgoingPacket_s(0x29, sizeof(pkMoveItem_t), &packet);

        if (mConfig.EnableDebug)
            printDebug(&packet, pResource, process);

        mGear.moveCount++;
        if (mGear.moveCount >= mConfig.MoveLimit)
            return true;
        if ((mServer.outgoingChunkUsage + 8) > MAX_PACKET_ENCRYPTED_BUFFER_SIZE)
            return false;
    }

    //Check if we're moving item to a bag besides inventory.
    if (destination != 0)
    {
        pkMoveItem_t packet         = {0};
        packet.originContainer      = tContainer;
        packet.originIndex          = tIndex;
        packet.destinationContainer = destination;
        packet.destinationIndex     = DEFAULT_MOVE_INDEX;
        packet.count                = count;

        //If the item is headed for inventory, and we allow dirty packets, see if we can combine the stack.
        if ((mConfig.EnableDirtyPackets) && (pResource->StackSize > 1) && (count < pResource->StackSize))
        {
            for (auto x = 1; x < mGear.containermax[destination]; x++)
            {
                if ((mGear.items[destination][x].item.Id == pResource->Id) && ((mGear.items[destination][x].item.Count + count) <= pResource->StackSize))
                {
                    //If our item matches, and both stacks fit in a slot, move it directly into the slot instead of taking a new one.
                    packet.destinationIndex = x;
                    break;
                }
            }
        }

        updateItems(&packet, destination);
        pPacket->addOutgoingPacket_s(0x29, sizeof(pkMoveItem_t), &packet);

        if (mConfig.EnableDebug)
            printDebug(&packet, pResource, process);

        mGear.moveCount++;
    }

    return true;
}

void packer::updateItems(pkMoveItem_t* packet, int destination)
{
    bool destinationIsEquip     = (std::find(mConfig.EquipBags.begin(), mConfig.EquipBags.end(), destination) != mConfig.EquipBags.end());
    itemSlotInfo_t* oldItemSlot = &mGear.items[packet->originContainer][packet->originIndex];

    if (packet->destinationIndex == DEFAULT_MOVE_INDEX)
    {
        int newindex                = mGear.firstfreespace[packet->destinationContainer];
        itemSlotInfo_t* newItemSlot = &mGear.items[packet->destinationContainer][newindex];
        newItemSlot->isMoved        = true;

        if (packet->count == oldItemSlot->item.Count)
        {
            *newItemSlot         = *oldItemSlot;
            newItemSlot->isMoved = true;
        }
        else
        {
            newItemSlot->isMoved    = true;
            newItemSlot->isStuck    = false;
            newItemSlot->isEquip    = oldItemSlot->isEquip;
            newItemSlot->item       = oldItemSlot->item;
            newItemSlot->item.Count = packet->count;
            newItemSlot->resource   = oldItemSlot->resource;

            for (std::list<itemTarget_t>::iterator iter = oldItemSlot->targets.begin(); iter != oldItemSlot->targets.end(); iter++)
            {
                if ((*iter).count == packet->count)
                {
                    if (((*iter).container == destination) || (((*iter).container == -2) && (destinationIsEquip)))
                    {
                        newItemSlot->targetCount = iter->count;
                        newItemSlot->targets.clear();
                        newItemSlot->targets.push_back(*iter);
                        oldItemSlot->targetCount -= iter->count;
                        oldItemSlot->targets.erase(iter);
                        break;
                    }
                }
            }
        }

        mGear.freecount[packet->destinationContainer]--;
        mGear.firstfreespace[packet->destinationContainer] = DEFAULT_MOVE_INDEX;
        for (auto x = newindex; x < mGear.containermax[packet->destinationContainer]; x++)
        {
            if ((mGear.items[packet->destinationContainer][x].item.Id == 0) && (mGear.items[packet->destinationContainer][x].item.Count == 0))
            {
                mGear.firstfreespace[packet->destinationContainer] = x;
                break;
            }
        }
    }

    else //We only move into a specific index if there's a stack to be combined.
    {
        itemSlotInfo_t* newItemSlot = &mGear.items[packet->destinationContainer][packet->destinationIndex];
        newItemSlot->isMoved        = true;
        newItemSlot->item.Count += packet->count;

        for (std::list<itemTarget_t>::iterator iter = oldItemSlot->targets.begin(); iter != oldItemSlot->targets.end(); iter++)
        {
            if ((*iter).count == packet->count)
            {
                if (((*iter).container == destination) || (((*iter).container == -2) && (destinationIsEquip)))
                {
                    newItemSlot->targetCount += iter->count;
                    newItemSlot->targets.push_back(*iter);
                    oldItemSlot->targetCount -= iter->count;
                    oldItemSlot->targets.erase(iter);
                    break;
                }
            }
        }
    }

    if (packet->count == oldItemSlot->item.Count)
    {
        *oldItemSlot = itemSlotInfo_t();
        mGear.freecount[packet->originContainer]++;
        if (packet->originIndex < mGear.firstfreespace[packet->originContainer])
            mGear.firstfreespace[packet->originContainer] = packet->originIndex;
    }
    else
        oldItemSlot->item.Count -= packet->count;

    oldItemSlot->isMoved = true;
}

void packer::printDebug(pkMoveItem_t* pPacket, IItem* pResource, const char* process)
{
    if (pPacket->count == 1)
        pOutput->debug_f("[%s] Moving a %s from %s[%d] to %s[%d].", process, pResource->LogNameSingular[0], gContainerNames[pPacket->originContainer].c_str(), pPacket->originIndex, gContainerNames[pPacket->destinationContainer].c_str(), pPacket->destinationIndex);
    else
        pOutput->debug_f("[%s] Moving %d %s from %s[%d] to %s[%d].", process, pPacket->count, pResource->LogNamePlural[0], gContainerNames[pPacket->originContainer].c_str(), pPacket->originIndex, gContainerNames[pPacket->destinationContainer].c_str(), pPacket->destinationIndex);
}