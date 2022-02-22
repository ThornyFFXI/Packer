#include "Packer.h"
#include <chrono>

void packer::prepareMoveList()
{
    //Create fresh container info on game thread, so we don't have any read errors.
    createContainerInfo();

    //If threading is enabled, start the thread.
    if (mConfig.EnableThreading)
    {
        pOutput->message("Preparing item movement lists...");
        this->Start();
    }

    //Otherwise, do it on game thread.
    else
    {
        std::chrono::time_point<std::chrono::steady_clock> benchmark = std::chrono::steady_clock::now();

        flagMoveItems();

        if (mConfig.EnableBenchmark)
        {
            std::chrono::duration elapsed = std::chrono::steady_clock::now() - benchmark;
            pOutput->debug_f("flagMoveItems(game thread) Time: %dns", std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
        }
    }
}
uint32_t packer::ThreadEntry()
{
    std::chrono::time_point<std::chrono::steady_clock> benchmark = std::chrono::steady_clock::now();

    flagMoveItems();

    if (mConfig.EnableBenchmark)
    {
        std::chrono::duration elapsed = std::chrono::steady_clock::now() - benchmark;
        pOutput->debug_f("flagMoveItems(alt thread) Time: %dns", std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
    }
    return 0;
}
void packer::flagMoveItems()
{
    //Flag AC stuff only if we're in a GEAR event.
    if (mGear.useequipmentlist)
    {
        //Sort bags by priority to ensure the best outcome when selecting which items will be retrieved.
        //Valid bags where the item can't be removed, then valid bags where the item can be removed, then invalid bags where the item can't be removed, then invalid bags where the item can be removed.
        std::list<int> equipPriorities[4];
        for (std::list<containerInfo_t>::iterator iter = mConfig.containers.begin(); iter != mConfig.containers.end(); iter++)
        {
            if (std::find(mConfig.EquipBags.begin(), mConfig.EquipBags.end(), iter->containerIndex) != mConfig.EquipBags.end())
            {
                if (iter->canRetrieve)
                    equipPriorities[1].push_back(iter->containerIndex);
                else
                    equipPriorities[0].push_back(iter->containerIndex);
            }
            else
            {
                if (iter->canRetrieve)
                    equipPriorities[3].push_back(iter->containerIndex);
                else
                    equipPriorities[2].push_back(iter->containerIndex);
            }
        }

        //Iterate gear first, flagging items accordingly.
        for (std::list<itemOrder_t>::iterator iOrder = mGear.equipment.begin(); iOrder != mGear.equipment.end(); iOrder++)
        {
            int targetBag = -2;
            if ((mConfig.EnableWeaponPriority) && (iOrder->resource->Slots & 0x0F))
                targetBag = *mConfig.EquipBags.begin();

            iOrder->quantityFound = 0;
            for (int x = 0; x < 4; x++)
            {
                for (std::list<int>::iterator iBag = equipPriorities[x].begin(); iBag != equipPriorities[x].end(); iBag++)
                {
                    if (!mGear.hasContainer[*iBag])
                        continue;

                    if (parseBag(*iBag, &(*iOrder), targetBag))
                        break;
                }
                if ((!iOrder->all) && (iOrder->quantityFound >= iOrder->quantityNeeded))
                    break;
            }
        }

        //Iterate items next, flagging accordingly.
        for (std::list<itemOrder_t>::iterator iOrder = mGear.other.begin(); iOrder != mGear.other.end(); iOrder++)
        {
            iOrder->quantityFound = 0;

            //Go from last bag to be filled toward first, so we take items out of bags we want empty.
            for (std::list<containerInfo_t>::reverse_iterator iter = mConfig.containers.rbegin(); iter != mConfig.containers.rend(); iter++)
            {
                int x = iter->containerIndex;

                //Skip bags that can't contain items; waste of resources.
                if (std::find(gNoItemBags.begin(), gNoItemBags.end(), x) != gNoItemBags.end())
                    continue;

                //Skip bags we don't have access to, no point in it.
                if (!mGear.hasContainer[x])
                    continue;

                //Parse bag to try to satisfy order.
                if (parseBag(x, &(*iOrder), 0))
                    break;
            }
        }
    }

    //Now, flag organize items.
    for (std::list<containerInfo_t>::iterator iter = mConfig.containers.begin(); iter != mConfig.containers.end(); iter++)
    {
        //List of bags we will skip when iterating containers.
        std::list<int> noItemBags = {
            8,
            10,
            11,
            12};

        for (std::list<itemOrder_t>::iterator iOrder = iter->needed.begin(); iOrder != iter->needed.end(); iOrder++)
        {
            iOrder->quantityFound = 0;
            bool isEquip = (iOrder->resource->Flags & 0x800);

            //We want to check matching bag first since they won't have to be moved, past that it doesn't really matter.
            if (parseBag(iter->containerIndex, &(*iOrder), iter->containerIndex))
                continue;

            //Go from last bag to first so we take items out of bags we want empty.
            for (std::list<containerInfo_t>::reverse_iterator iter2 = mConfig.containers.rbegin(); iter2 != mConfig.containers.rend(); iter2++)
            {
                int x = iter2->containerIndex;

                //Skip matching bag, already been checked.
                if (x == iter->containerIndex)
                    continue;

                //If we're looking for an item, skip bags that can't contain items; waste of resources.
                if ((!isEquip) && (std::find(noItemBags.begin(), noItemBags.end(), x) != noItemBags.end()))
                    continue;

                //Skip bags we don't have access to, no point in it.
                if (!mGear.hasContainer[x])
                    continue;

                //Parse bag to try to satisfy order.
                if (parseBag(x, &(*iOrder), iter->containerIndex))
                    break;
            }
        }
    }

    mGear.reparse = false;
}
void packer::createContainerInfo()
{
    std::chrono::time_point<std::chrono::steady_clock> benchmark = std::chrono::steady_clock::now();

    //Create a copy of inventory with some extra parameters that we can modify as we move items.
    for (int x = 0; x < CONTAINER_MAX; x++)
    {
        mGear.containermax[x]   = pInv->GetContainerCountMax(x) + 1;
        mGear.freecount[x]      = 0;
        mGear.firstfreespace[x] = DEFAULT_MOVE_INDEX;

        if (x == 3)
            continue;

        mGear.items[x][0] = itemSlotInfo_t();
        for (int y = 1; y < mGear.containermax[x]; y++)
        {
            Ashita::FFXI::item_t* pItem = pInv->GetContainerItem(x, y);

            //Update item's information.
            if ((pItem->Id != 0) || (pItem->Count != 0))
                mGear.items[x][y] = itemSlotInfo_t(pItem, m_AshitaCore->GetResourceManager()->GetItemById(pItem->Id));
            else
            {
                mGear.items[x][y] = itemSlotInfo_t();
                mGear.freecount[x]++;
                if (y < mGear.firstfreespace[x])
                    mGear.firstfreespace[x] = y;
            }
        }
    }

    if (mConfig.EnableBenchmark)
    {
        std::chrono::duration elapsed = std::chrono::steady_clock::now() - benchmark;
        pOutput->debug_f("createContainerInfo Time: %dns", std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
    }
}
bool packer::verifyContainerInfo()
{
    std::chrono::time_point<std::chrono::steady_clock> benchmark = std::chrono::steady_clock::now();

    //Verify that all our moves have executed as intended.
    for (int x = 0; x < CONTAINER_MAX; x++)
    {
        if (x == 3)
            continue;

        for (int y = 1; y < mGear.containermax[x]; y++)
        {
            if (!mGear.items[x][y].isEqual(pInv->GetContainerItem(x, y)))
            {
                if (mConfig.EnableBenchmark)
                {
                    std::chrono::duration elapsed = std::chrono::steady_clock::now() - benchmark;
                    pOutput->debug_f("verifyContainerInfo Fail [%d:%d] Time: %d nanoseconds", x, y, std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
                }
                return false;
            }
        }
    }

    if (mConfig.EnableBenchmark)
    {
        std::chrono::duration elapsed = std::chrono::steady_clock::now() - benchmark;
        pOutput->debug_f("verifyContainerInfo Success Time: %dns", std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
    }
    return true;
}
bool packer::parseBag(int index, itemOrder_t* order, int targetBag)
{
    bool orderIsEquip     = (order->resource->Flags & 0x0800);
    bool targetBagIsEquip = (std::find(mConfig.EquipBags.begin(), mConfig.EquipBags.end(), targetBag) != mConfig.EquipBags.end());

    //Iterate the bag, making a list of matching items.
    std::list<itemSlotInfo_t*> matches;
    for (int y = 1; y < 81; y++)
    {
        //Get a pointer to our item information struct.
        itemSlotInfo_t* item = &mGear.items[index][y];
        if (item->isStuck)
        {
            //If item is stuck for a reason other than being equipped, we aren't going to flag it.
            if (item->item.Flags != 5)
                continue;

            else if ((targetBag != -2) && (targetBag != index)) //If it's equipped, we can still flag it, as long as it's in a correct bag.
                continue;
        }

        //Check if item matches order, and add to list if so.
        if (itemMatchesOrder(*order, &item->item))
            matches.push_back(item);
    }

    //If we're dealing with a stackable, and not taking all of them, sort to take partial stacks first.
    if ((!order->all) && (order->resource->StackSize != 1))
    {
        //Determine how many more we need.
        int count = order->quantityNeeded - order->quantityFound;

        //Perform sort.
        matches.sort([&count](const itemSlotInfo_t* a, const itemSlotInfo_t* b) {
            //If one of these matches the count exactly, we want that one.
            if ((a->item.Count == count) != (b->item.Count == count))
                return (a->item.Count == count);

            //If one of these is greater than the amount needed, we want that one.
            if ((a->item.Count > count) != (b->item.Count > count))
                return (a->item.Count > count);

            //If neither totals the required count, we want the one with a higher count.
            return (a->item.Count > b->item.Count);
        });
    }

    //Flag items until we satisfy the amount.
    for (std::list<itemSlotInfo_t*>::iterator iter = matches.begin(); iter != matches.end(); iter++)
    {
        //Determine how many of the item are left to be moved.
        int count = (*iter)->item.Count - (*iter)->targetCount;

        
        for (std::list<itemTarget_t>::iterator iter2 = (*iter)->targets.begin(); iter2 != (*iter)->targets.end(); iter2++)
        {
            //Check any movement targets already on item for matching locations.
            if ((iter2->container == targetBag) || ((iter2->container == -2) && (*iter)->isEquip && targetBagIsEquip))
            {
                iter2->container = targetBag; //Either retargeting arbitrary equip to match a specific bag request, or they are already equal.
                order->quantityFound += iter2->count; //Mark the amount already being moved as found.

                //Check if we can increase the amount in the movement target to satisfy more of the order.
                if ((count != 0) && (order->all ? false : (order->quantityFound < order->quantityNeeded)))
                {
                    int amount = order->all ? count : min(order->quantityNeeded - order->quantityFound, count);
                    iter2->count += amount;
                    count -= amount;
                    (*iter)->targetCount += amount;
                    order->quantityFound += amount;                
                }

                //Note that we flagged an item.
                if (mConfig.EnableDebug)
                {
                    string dest = "equipbags";
                    if (targetBag != -2)
                        dest = gContainerNames[targetBag];
                    pOutput->debug_f("Reflagged %d %s at %s[%d] for %s.", iter2->count, getValidateString(*order, (iter2->count > 1)).c_str(), gContainerNames[index].c_str(), (*iter)->item.Index, dest.c_str());
                }

                if ((!order->all) && (order->quantityFound >= order->quantityNeeded))
                    return true;
            }
        }

        if (count != 0)
        {
            int amount = order->all ? count : min(order->quantityNeeded - order->quantityFound, count);
            (*iter)->targets.push_back(itemTarget_t(targetBag, amount));
            (*iter)->targetCount += amount;
            order->quantityFound += amount;
            if (mConfig.EnableDebug)
            {
                string dest = "equipbags";
                if (targetBag != -2)
                    dest = gContainerNames[targetBag];
                pOutput->debug_f("Flagged %d %s at %s[%d] for %s.", amount, getValidateString(*order, (amount > 1)).c_str(), gContainerNames[index].c_str(), (*iter)->item.Index, dest.c_str());
            }

            if ((!order->all) && (order->quantityFound >= order->quantityNeeded))
                return true;
        }
    }
    return false;
}