#include "Packer.h"

int packer::getContainerIndex(const char* text)
{
    for (int x = 0; x < CONTAINER_MAX; x++)
    {
        if (_stricmp(gContainerNames[x].c_str(), text) == 0)
            return x;    
    }
    return -1;
}
int packer::getSlotIndex(const char* text)
{
    for (int x = 0; x < SLOT_MAX; x++)
    {
        if (_stricmp(gSlotNames[x].c_str(), text) == 0)
            return x;
    }
    return -1;
}

void packer::waitForThread()
{
    if (!this->GetHandle())
        return;

    //Wait for thread to end
    std::chrono::time_point<std::chrono::steady_clock> benchmark = std::chrono::steady_clock::now();
    this->WaitFor(INFINITE);
    if (mConfig.EnableBenchmark)
    {
        std::chrono::duration elapsed = std::chrono::steady_clock::now() - benchmark;
        pOutput->debug_f("waitForThread Time: %dns", std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
    }
}

void packer::stopEvent()
{
    //Wait for any existing reparse event to finish, for thread safety.
    waitForThread();

    //Disable and interrupt any existing event.
    mGear.active = false;
    sendEventResponse("INTERRUPTED");
    mEventState.eventIsActive = false;
}

void packer::sendEventResponse(const char* text)
{
    if (mEventState.eventIsActive)
    {
        char buffer[512];
        sprintf_s(buffer, 512, "%s_%s_%s", mEventState.returnEventName, mEventState.eventName, text);
        pOutput->debug_f("Packer Event: %s", buffer);

        if (strcmp(mEventState.returnEventName, "SELF_EVENT") != 0)
        {
            m_AshitaCore->GetPluginManager()->RaiseEvent(buffer, NULL, 0);
        }
    }
}

bool packer::itemMatchesOrder(itemOrder_t order, Ashita::FFXI::item_t* item)
{
    if ((item->Id == 0) || (item->Count == 0))
        return false;

    if (item->Id != order.resource->Id)
    {
        IItem* resource = m_AshitaCore->GetResourceManager()->GetItemById(item->Id);
        if (_stricmp(resource->Name[0], order.name) != 0)
            return false;
    }

    if (!order.augment)
        return true;

    augmentData_t augment = createAugmentData(item);

    if ((order.augPath != -1) && (augment.path != order.augPath))
        return false;

    if ((order.augRank != -1) && (augment.rank != order.augRank))
        return false;

    if ((order.augTrial != -1) && (augment.trialNumber != order.augTrial))
        return false;

    for (std::vector<string>::iterator iter = order.augStrings.begin(); iter != order.augStrings.end(); iter++)
    {
        if (std::find(augment.stringAugments.begin(), augment.stringAugments.end(), *iter) == augment.stringAugments.end())
            return false;
    }

    return true;
}

string packer::getValidateString(itemOrder_t order, bool plural)
{
    stringstream stream;

    if (plural)
        stream << order.resource->LogNamePlural[0];
    else
        stream << order.resource->LogNameSingular[0];

    if (!order.augment)
        return stream.str();

    bool firstAugString = true;

    stream << " (";
    if (order.augPath != -1)
    {
        stream << "Path ";
        stream << (char)(order.augPath + 64);
        firstAugString = false;
    }

    if (order.augRank != -1)
    {
        if (!firstAugString)
            stream << ", ";
        stream << "Rank ";
        stream << order.augRank;
        firstAugString = false;
    }

    if (order.augTrial != -1)
    {
        if (!firstAugString)
            stream << ", ";
        stream << "Trial ";
        stream << order.augTrial;
        firstAugString = false;
    }

    for (std::vector<string>::iterator iter = order.augStrings.begin(); iter != order.augStrings.end(); iter++)
    {
        if (!firstAugString)
            stream << ", ";
        stream << *iter;
        firstAugString = false;
    }
    stream << ")";

    return stream.str();
}
void packer::checkMog()
{
    bool atNomad       = false;
    bool inMog         = false;

    DWORD zonepointer = Read32(pZoneFlags, 0);
    if (zonepointer != 0)
    {
        if (Read32(zonepointer, pZoneOffset) & 0x100)
        {
            inMog = true;
        }
    }


    for (int x = 0; x < 1024; x++)
    {
        if ((m_AshitaCore->GetMemoryManager()->GetEntity()->GetRawEntity(x)) && (m_AshitaCore->GetMemoryManager()->GetEntity()->GetRenderFlags0(x) & 0x200))
        {
            if ((m_AshitaCore->GetMemoryManager()->GetEntity()->GetDistance(x) < 36) && (strcmp(m_AshitaCore->GetMemoryManager()->GetEntity()->GetName(x), "Nomad Moogle") == 0))
            {
                atNomad = true;
            }
            if ((m_AshitaCore->GetMemoryManager()->GetEntity()->GetDistance(x) < 36) && (strcmp(m_AshitaCore->GetMemoryManager()->GetEntity()->GetName(x), "Pilgrim Moogle") == 0))
            {
                atNomad = true;
            }
        }
    }

    DWORD Memloc           = Read32(pWardrobe, 0);
    Memloc                 = Read32(Memloc, 0);
    uint8_t flags          = Read8(Memloc, 0xB4);

    mGear.hasContainer[0] = true; //Always have inventory.
    mGear.hasContainer[1] = (atNomad || inMog); //Safe
    mGear.hasContainer[2] = (inMog || (atNomad && mConfig.EnableNomadStorage)); //Storage
    mGear.hasContainer[3] = false; //Never have temp items.
    mGear.hasContainer[4] = (atNomad || inMog); //Locker
    mGear.hasContainer[5] = ((Read8(Memloc, 0xB4) & 0x01) != 0);  //Satchel
    mGear.hasContainer[6] = true; //Sack
    mGear.hasContainer[7] = true; //Case
    mGear.hasContainer[8] = true; //Wardrobe
    mGear.hasContainer[9] = (atNomad || inMog); //Safe2
    mGear.hasContainer[10] = true; //Wardrobe2
    mGear.hasContainer[11] = ((flags & 0x04) != 0);                //Wardrobe3
    mGear.hasContainer[12] = ((flags & 0x08) != 0);                //Wardrobe4
    mGear.hasContainer[13] = ((flags & 0x10) != 0);                //Wardrobe5
    mGear.hasContainer[14] = ((flags & 0x20) != 0);                //Wardrobe6
    mGear.hasContainer[15] = ((flags & 0x40) != 0);                //Wardrobe7
    mGear.hasContainer[16] = ((flags & 0x80) != 0);                //Wardrobe8

    for (std::list<int>::iterator iter = mConfig.ForceEnableBags.begin(); iter != mConfig.ForceEnableBags.end(); iter++)
    {
        mGear.hasContainer[*iter] = true;    
    }

    for (std::list<int>::iterator iter = mConfig.ForceDisableBags.begin(); iter != mConfig.ForceDisableBags.end(); iter++)
    {
        mGear.hasContainer[*iter] = false;
    }
}