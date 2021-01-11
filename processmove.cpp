#include "Packer.h"
#include <map>

void packer::processItemMovement()
{
    //If we have a thread going, we don't want to do anything.
    if ((GetHandle()) && (!this->WaitFor(0)))
        return;

    checkMog();
    //If we've called for a reparse, recheck inventory.
    if (mGear.reparse)
    {
        prepareMoveList();
        if (mGear.reparse)
            return;
    }

    //Otherwise, check if containers match up.
    else if (!verifyContainerInfo())
        return;

    mGear.moveCount  = 0;
    mGear.invWarning = false;

    //Check if we have anything to defragment, if defrag is enabled.
    if (mConfig.EnableDefrag)
        if (!processDefrag())
            return;

    //Check if we have any items to move for our move lists.
    if (!processMoves())
        return;

    //Success! All done, do output and signal calling plugin.
    if (strcmp(mEventState.eventName, "GEAR") == 0)
    {
        if (mConfig.EnableValidateAfterGear)
        {
            validate(&mGear.equipment, &mGear.other);
        }
        pOutput->message("Gear completed!");
    }
    else
    {
        pOutput->message("Organize completed!");
    }
    sendEventResponse("SUCCEEDED");
    mEventState.eventIsActive = false;
    mGear.active              = false;
}

bool packer::processMoves()
{
    //Iterate containers in order, first filling with mandatory items, then optional items.  Move out any items that belong elsewhere.
    for (std::list<containerInfo_t>::iterator iter = mConfig.containers.begin(); iter != mConfig.containers.end();)
    {
        int container = iter->containerIndex;
        if (!mGear.hasContainer[container])
        {
            iter++;
            continue;
        }

        std::list<itemLoc_t> fetchList = createFetchList(container);

        //Skip processing the rest once we're out of space to work with.
        if (mGear.moveCount >= mConfig.MoveLimit)
            break;
        int chunkSize = 16;
        if (mConfig.EnableDirtyPackets)
            chunkSize = 8;
        if ((mServer.outgoingChunkUsage + chunkSize) > MAX_PACKET_ENCRYPTED_BUFFER_SIZE)
            break;

        bool nextContainer = false;

        //If we have space, grab an item.
        if (mGear.freecount[container] > 0)
        {
            if (tryFetch(&fetchList, container, iter->canStoreEquipment, iter->canStoreItems))
                continue;
            else if (fetchList.size() == 0)
                nextContainer = true;
        }

        //If we have no space, but still have items in our fetchlist, we try to store an item to make space for them.
        else if (fetchList.size() > 0)
        {
            bool flexible = (fetchList.begin()->target == -2); //If the first item in fetch list is set for -2, it can go elsewhere and no error is needed yet.

            if (tryStore(container, !flexible, iter->canRetrieve))
                continue;
            else
            {
                if (!flexible)
                    pOutput->error_f("Could not create space to store every requested item in %s.  Continuing to next bag.", gContainerNames[container].c_str());
                else if (container == 0)
                    pOutput->error("Could not create space in equip-friendly bags to store every requested equipment.");

                if (mConfig.EnableDebug)
                {
                    for (int x = 0; x < CONTAINER_MAX; x++)
                    {
                        pOutput->debug_f("%s : %d open spaces", gContainerNames[x].c_str(), mGear.freecount[x]);
                    }
                }

                //Let the user know that we could not make enough space in this bag, and continue to the next.
                nextContainer = true;
            }
        }

        //If we have no space, and our fetch list is empty, we try to store items in higher priority bags only.
        else if (tryStore(container, false, iter->canRetrieve))
            continue;

        //If we have no space, and there is nothing that should be stored, we can advance to the next container.
        else
            nextContainer = true;

        //Continue to the next container.
        if (nextContainer)
            iter++;
    }

    if (mGear.moveCount > 0)
        pOutput->message_f("Performing $H%d$R item movements.", mGear.moveCount);

    return (mGear.moveCount == 0);
}
std::list<itemLoc_t> packer::createFetchList(int container)
{
    std::list<itemLoc_t> fetchList;
    //If we are in an equip friendly bag, add -2s.  Make sure to sort so they show up last.
    bool containerIsEquip = (std::find(mConfig.EquipBags.begin(), mConfig.EquipBags.end(), container) != mConfig.EquipBags.end());

    for (int x = 0; x < CONTAINER_MAX; x++)
    {
        bool currentIsEquip = (std::find(mConfig.EquipBags.begin(), mConfig.EquipBags.end(), x) != mConfig.EquipBags.end());

        if (x == 3)
            continue;

        if (x == container)
            continue;

        for (int y = 1; y < mGear.containermax[x]; y++)
        {
            itemSlotInfo_t* item = &mGear.items[x][y];

            for (std::list<itemTarget_t>::iterator iter = item->targets.begin(); iter != item->targets.end(); iter++)
            {
                if (iter->container == container)
                    fetchList.push_front(itemLoc_t(x, y, iter->container));
                else if ((iter->container == -2) && (containerIsEquip) && (!currentIsEquip) && (x != 0))
                    fetchList.push_back(itemLoc_t(x, y, iter->container));
            }
        }
    }

    return fetchList;
}
bool packer::tryFetch(std::list<itemLoc_t>* fetchList, int container, bool canStoreEquip, bool canStoreItems)
{
    bool containerIsEquip = (std::find(mConfig.EquipBags.begin(), mConfig.EquipBags.end(), container) != mConfig.EquipBags.end());

    //Prefer items that are on fetch list.
    for (std::list<itemLoc_t>::iterator iter = fetchList->begin(); iter != fetchList->end(); iter++)
    {
        itemSlotInfo_t* item = &mGear.items[iter->container][iter->index];
        for (std::list<itemTarget_t>::iterator iter2 = item->targets.begin(); iter2 != item->targets.end(); iter2++)
        {
            if (iter->target == iter2->container)
            {
                if (moveItem(iter->container, iter->index, iter2->count, container, item->resource, "Retrieve"))
                {
                    iter = fetchList->erase(iter);
                    return true;
                }
            }
        }
    }

    //If we can neither put equipment nor items in this container, and nothing on fetchlist worked, we're done.
    if (!canStoreEquip && !canStoreItems)
        return false;

    //Iterate containers backwards, to try to find an item to push into this bag.
    for (std::list<containerInfo_t>::reverse_iterator iter = mConfig.containers.rbegin(); iter != mConfig.containers.rend(); iter++)
    {
        int x = iter->containerIndex;
        //Once we reach this bag, we're done because we don't want to move items from higher priority bags into here.
        if (x == container)
            break;

        if (!mGear.hasContainer[x])
            continue;

        for (int y = 1; y < mGear.containermax[x]; y++)
        {
            itemSlotInfo_t* item = &mGear.items[x][y];
            if (item->isStuck)
                continue;

            if ((item->targetCount != 0) || (item->item.Count == 0))
                continue;

            if (item->isEquip && !canStoreEquip)
                continue;

            if (!item->isEquip && !canStoreItems)
                continue;

            if (moveItem(x, y, item->item.Count, container, item->resource, "Retrieve"))
                return true;
        }
    }

    return false;
}
bool packer::tryStore(int container, bool force, bool canRetrieve)
{
    //If force is set, we can store items in higher priority bags or store items flagged -2 since an item is required to go in this bag.
    //We still prefer not to, since it may create more movements later.
    bool containerIsEquip = (std::find(mConfig.EquipBags.begin(), mConfig.EquipBags.end(), container) != mConfig.EquipBags.end());
    int storeIndex        = -1;
    int storeContainer    = -1;
    int storeCount        = 0;
    int x                 = container;

    int equipcontainer = -1;
    int othercontainer = -1;

    //Iterate containers forwards, to find one with empty space.
    for (std::list<containerInfo_t>::iterator iter = mConfig.containers.begin(); iter != mConfig.containers.end(); iter++)
    {
        //Once we reach this bag, we're done because we don't want to move items from higher priority bags into here.
        if (iter->containerIndex == container)
        {
            //If force is enabled, we can store in bags that are required to fill before this one.  Otherwise, we cannot.
            if (force)
                continue;
            else
                break;
        }

        if (!mGear.hasContainer[iter->containerIndex])
            continue;

        if (iter->containerIndex == 0)
        {
            if ((!iter->canStoreEquipment) && (!iter->canStoreItems))
                continue;

            if (mGear.freecount[iter->containerIndex] > 2)
            {
                if ((iter->canStoreEquipment) && (equipcontainer == -1))
                    equipcontainer = iter->containerIndex;
                if ((iter->canStoreItems) && (othercontainer == -1))
                    othercontainer = iter->containerIndex;
            }
            else if (!mGear.invWarning)
            {
                pOutput->message_f("Warning: Inventory is down to %d spaces.  Try to maintain at least 2 open spaces to avoid problems.", mGear.freecount[0]);
                mGear.invWarning = true;
            }
        }

        //We found a bag with space.  Mark it.
        else if (mGear.freecount[iter->containerIndex] > 0)
        {
            if ((iter->canStoreEquipment) && (equipcontainer == -1))
                equipcontainer = iter->containerIndex;
            if ((iter->canStoreItems) && (othercontainer == -1))
                othercontainer = iter->containerIndex;
        }

        if ((equipcontainer != -1) && (othercontainer != -1))
            break;
    }

    //First iteration, check for only items with a targetcount for a bag with space, since they are more useful moves.
    for (int y = 1; y < mGear.containermax[x]; y++)
    {
        itemSlotInfo_t* item = &mGear.items[x][y];
        if ((item->targetCount == 0) || (item->isStuck))
            continue;

        for (std::list<itemTarget_t>::iterator iter = item->targets.begin(); iter != item->targets.end(); iter++)
        {
            storeIndex = -1;

            //If any of the target count belongs here, we don't really want to move it because it's not going to make any more space unless we move it all.
            if ((iter->container == container) || ((iter->container == -2) && containerIsEquip))
                break;

            //If the item is equippable, we already checked if this is an equip friendly container, see if there is an equip friendly container with space for it.
            if (iter->container == -2)
            {
                for (std::list<int>::iterator iter2 = mConfig.EquipBags.begin(); iter2 != mConfig.EquipBags.end(); iter2++)
                {
                    if (!mGear.hasContainer[*iter2])
                        continue;

                    if ((*iter2) == 0)
                    {
                        if (mGear.freecount[0] > 2)
                        {
                            storeContainer = *iter2;
                            storeIndex     = y;
                            storeCount     = iter->count;
                            break;
                        }
                        else if (!mGear.invWarning)
                        {
                            pOutput->message_f("Warning: Inventory is down to %d spaces.  Try to maintain at least 2 open spaces to avoid problems.", mGear.freecount[0]);
                            mGear.invWarning = true;
                        }
                    }

                    else if (mGear.freecount[*iter2] > 0)
                    {
                        storeContainer = *iter2;
                        storeIndex     = y;
                        storeCount     = iter->count;
                        break;
                    }
                }
            }

            //Otherwise, check if the container it's specified for has space.
            else if (iter->container == 0)
            {
                if (mGear.freecount[0] > 2)
                {
                    storeContainer = 0;
                    storeIndex     = y;
                    storeCount     = iter->count;
                    break;
                }
                else if (!mGear.invWarning)
                {
                    pOutput->message_f("Warning: Inventory is down to %d spaces.  Try to maintain at least 2 open spaces to avoid problems.", mGear.freecount[0]);
                    mGear.invWarning = true;
                }
            }

            else if (mGear.freecount[iter->container] > 0)
            {
                storeContainer = iter->container;
                storeIndex     = y;
                storeCount     = iter->count;
            }

            if (storeIndex == -1)
                break;
        }

        if (storeIndex != -1)
            break;
    }

    //Second iteration, check for items that aren't locked in here.
    //We can take items out of the bag that nothing else requested only if canRetrieve is allowed.
    //Hopefully dumbass user didn't set canRetrieve to false on all their wardrobes.
    if ((storeIndex == -1) && (canRetrieve))
    {
        for (int y = 1; y < mGear.containermax[x]; y++)
        {
            itemSlotInfo_t* item = &mGear.items[x][y];
            if (item->isStuck)
                continue;

            if ((item->targetCount == 0) && (item->item.Count != 0))
            {
                if ((item->isEquip) && (equipcontainer != -1))
                {
                    storeContainer = equipcontainer;
                    storeIndex     = y;
                    storeCount     = item->item.Count;
                    break;
                }

                else if ((!item->isEquip) && (othercontainer != -1))
                {
                    storeContainer = othercontainer;
                    storeIndex     = y;
                    storeCount     = item->item.Count;
                    break;
                }
            }
        }
    }

    //Final iteration, if we have force enabled we can move equippables out or move items to bags other than target(wasted move).
    if ((storeIndex == -1) && (force) && (containerIsEquip))
    {
        for (int y = 1; y < mGear.containermax[x]; y++)
        {
            itemSlotInfo_t* item = &mGear.items[x][y];
            if ((item->targetCount == 0) || (item->isStuck))
                continue;

            if ((item->isEquip) && (equipcontainer != -1))
            {
                storeContainer = equipcontainer;
                storeIndex     = y;
                storeCount     = item->item.Count;
            }

            else if ((!item->isEquip) && (othercontainer != -1))
            {
                storeContainer = othercontainer;
                storeIndex     = y;
                storeCount     = item->item.Count;
            }

            else
                continue;

            for (std::list<itemTarget_t>::iterator iter = item->targets.begin(); iter != item->targets.end(); iter++)
            {
                if (iter->container == container)
                {
                    storeIndex = -1;
                    break;
                }
            }

            if (storeIndex != -1)
                break;
        }
    }

    //If none of the iterations found anything to store, we can't store anything so return false.
    if (storeIndex == -1)
        return false;

    //Try to move the item.
    itemSlotInfo_t* item = &mGear.items[x][storeIndex];
    if (moveItem(x, storeIndex, storeCount, storeContainer, item->resource, force ? "Store" : "Compress"))
        return true;

    return false;
}
bool packer::processDefrag()
{
    if ((mConfig.MoveLimit < 4) && (!mConfig.EnableDirtyPackets))
    {
        pOutput->error("Defragmenting requires movelimit to be set to 4 or higher or dirtypackets to be enabled.");
        return true;
    }

    int fragcount = 0;

    //USING TARGET FIELD FOR COUNT LIKE A TOOL.  PLEASE BE AWARE.
    std::map<uint16_t, list<itemLoc_t>> stackables;

    for (std::list<containerInfo_t>::iterator iter = mConfig.containers.begin(); iter != mConfig.containers.end(); iter++)
    {
        int x = iter->containerIndex;

        if (!mGear.hasContainer[x])
            continue;

        for (int y = 1; y < mGear.containermax[x]; y++)
        {
            itemSlotInfo_t* item = &mGear.items[x][y];
            int count            = item->item.Count;
            if ((item->targetCount != 0) || (count == 0) || (count == item->resource->StackSize))
                continue;

            if (count != 0)
            {
                std::map<uint16_t, list<itemLoc_t>>::iterator iter3 = stackables.find(item->item.Id);
                if (iter3 != stackables.end())
                    iter3->second.push_back(itemLoc_t(x, y, count));
                else
                    stackables.insert(std::make_pair(item->item.Id, std::list<itemLoc_t>{itemLoc_t(x, y, count)}));
            }
        }
    }

    for (std::map<uint16_t, list<itemLoc_t>>::iterator iter = stackables.begin(); iter != stackables.end(); iter++)
    {
        IItem* resource = m_AshitaCore->GetResourceManager()->GetItemById(iter->first);
        int stacksize   = resource->StackSize;
        int slotcount   = 0;
        int itemcount   = 0;
        for (std::list<itemLoc_t>::iterator iter2 = iter->second.begin(); iter2 != iter->second.end(); iter2++)
        {
            slotcount++;
            itemcount += iter2->target;
        }
        int minslotcount = (itemcount / stacksize) + ((itemcount % stacksize) != 0);

        if (minslotcount < slotcount)
        {
            fragcount += doDefrag(resource, iter->second);
        }
    }

    if (fragcount > 0)
        pOutput->message_f("Defragmenting $H%d$R items.", fragcount);

    return (fragcount == 0);
}