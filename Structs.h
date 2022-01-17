#ifndef __ASHITA_packerStructs_H_INCLUDED__
#define __ASHITA_packerStructs_H_INCLUDED__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "C:\Ashita 4\plugins\sdk\Ashita.h"
#include "..\common\thirdparty\rapidxml.hpp"
#include <chrono>
#include <vector>
using namespace std;
using namespace rapidxml;

struct packerPluginEvent_t
{
    char fileName[512];
    char returnEvent[256];
    xml_document<>* document;
};

struct singleAugment_t
{
    int32_t table;
    int32_t realId;
    string stat;
    int32_t value;
    bool percent;
};

struct itemLoc_t
{
    int container;
    int index;
    int target;

    itemLoc_t(int container, int index, int target)
        : container(container)
        , index(index)
        , target(target)
    {}
};

struct itemTarget_t
{
    int container;
    int count;

    itemTarget_t(int container, int count)
        : container(container)
        , count(count)
    {}
};

struct lacItemOrder_t
{
    char Name[32];
    int32_t Quantity;
    int32_t AugPath;
    int32_t AugRank;
    int32_t AugTrial;
    int32_t AugCount;
    char AugString[5][100];
};
struct lacPackerEvent_t
{
    int32_t EntryCount;
    lacItemOrder_t Entries[400];
};

struct itemOrder_t
{
    char name[32];
    IItem* resource;
    int quantityNeeded;
    int quantityFound;
    bool all;
    bool augment;
    int32_t augPath;
    int32_t augRank;
    int32_t augTrial;
    vector<string> augStrings;

    itemOrder_t(lacItemOrder_t order, IItem* pResource)
    {
        strcpy_s(name, 32, order.Name);
        resource = pResource;
        quantityFound = 0;
        augPath  = order.AugPath;
        augRank = order.AugRank;
        augTrial = order.AugTrial;
        for (int x = 0; x < order.AugCount; x++)
        {
            augStrings.push_back(std::string(order.AugString[x]));
        }
        if (order.Quantity == -1)
        {
            quantityNeeded = 1;
            all            = true;
        }
        else
        {
            quantityNeeded = order.Quantity;
            all            = false;
        }
        augment = ((augPath != -1) || (augRank != -1) || (augTrial != -1) || (augStrings.size() > 0));
    }

    itemOrder_t(xml_node<>* node, IItem* pResource)
    {
        strcpy_s(name, 32, node->value());
        resource       = pResource;
        quantityNeeded = 1;
        quantityFound  = 0;
        all            = false;
        augment        = false;

        augPath               = -1;
        xml_attribute<>* attr = node->first_attribute("augpath");
        if (attr)
        {
            if (attr->value()[0] == 'A')
                augPath = 1;
            else if (attr->value()[0] == 'B')
                augPath = 2;
            else if (attr->value()[0] == 'C')
                augPath = 3;
            else if (attr->value()[0] == 'D')
                augPath = 4;
            else
                augPath = 5;

            augment = true;
        }

        augRank = -1;
        attr    = node->first_attribute("augrank");
        if (attr)
        {
            augRank = atoi(attr->value());
            augment = true;
        }

        augTrial = -1;
        attr     = node->first_attribute("augtrial");
        if (attr)
        {
            augTrial = atoi(attr->value());
            augment  = true;
        }

        attr = node->first_attribute("augment");
        if ((attr) && (strlen(attr->value()) > 0))
        {
            stringstream stream(attr->value());
            while (stream.good())
            {
                string augment;
                getline(stream, augment, ',');
                augStrings.push_back(augment);
            }
            augment = true;
        }

        if (_stricmp(node->name(), "item") == 0)
        {
            attr = node->first_attribute("quantity");
            if (attr)
            {
                if (_stricmp(attr->value(), "all") == 0)
                {
                    all            = true;
                    quantityNeeded = 1;
                }
                else
                    quantityNeeded = atoi(attr->value());
            }
        }
    }

    bool operator==(const itemOrder_t& other)
    {
        if (_stricmp(name, other.name) != 0)
            return false;

        if (augment != other.augment)
            return false;

        if (!augment)
            return true;

        if ((augPath != other.augPath) || (augRank != other.augRank) || (augTrial != other.augTrial))
            return false;

        if (augStrings.size() != other.augStrings.size())
            return false;

        for (std::vector<string>::iterator iter = augStrings.begin(); iter != augStrings.end(); iter++)
        {
            if (std::find(other.augStrings.begin(), other.augStrings.end(), *iter) == other.augStrings.end())
                return false;
        }

        return true;
    }
};

struct augmentData_t
{
    uint32_t path;
    uint32_t rank;
    uint32_t trialNumber;
    vector<string> stringAugments;
    vector<singleAugment_t> rawAugments;
};

struct augmentResource_t
{
    uint32_t id;
    uint32_t realid;
    string stat;
    int32_t offset;
    int32_t multiplier;
    bool percent;
};

struct itemInfo_t
{
    Ashita::FFXI::item_t item;
    IItem* pResource;
};

struct containerInfo_t
{
    int containerIndex;
    bool canRetrieve;       //When enabled, items can be moved out of this targetContainer.
    bool canStoreItems;     //When enabled, non-equippable items can be stored in this targetContainer.
    bool canStoreEquipment; //When enabled, equipment can be stored in this targetContainer.
    list<itemOrder_t> needed;

    containerInfo_t()
        : canRetrieve(true)
        , canStoreItems(false)
        , canStoreEquipment(false)
    {}
};
struct packerConfig_t
{
public:
    std::list<int32_t> EquipBags; //Lists which bags can be equipped from.  Order only matters for EnableWeaponPriority.
    std::list<int32_t> ForceEnableBags; //Forces bags to show as accessible.  Primarily for DSP usage.
    std::list<int32_t> ForceDisableBags; //Forces bags to read as inaccessible.  Primarily for DSP usage.
    bool EnableThreading; //When enabled, initial move lists will be created in a background thread.
    bool EnableWeaponPriority;    //When enabled, all weapons(main/sub/range/ammo) in your AC xml will be stored in the first equipbag listed.
    bool EnableDirtyPackets;      //When enabled, packer will use item move packets to directly combine stacks and bypass inventory.
    bool EnableDefrag;           //When enabled, packer will combine like stacks after organizing.
    bool EnableBenchmark;            //When enabled, packer will benchmark the more time consuming actions.
    bool EnableDebug;            //When enabled, packer will log every item move.
    bool EnableNomadStorage;      //When enabled, packer will access storage at nomad moogles.
    bool EnableValidateAfterGear; //When enabled, a validate will be printed after completing a gear command.
    int MoveLimit;
    std::list<containerInfo_t> containers;

    packerConfig_t()
        : EquipBags({8, 10, 11, 12, 0})
        , ForceEnableBags(std::list<int32_t>())
        , ForceDisableBags(std::list<int32_t>())
        , EnableThreading(true)
        , EnableWeaponPriority(true)
        , EnableDirtyPackets(false)
        , EnableDefrag(true)
        , EnableBenchmark(true)
        , EnableDebug(true)
        , EnableNomadStorage(false)
        , EnableValidateAfterGear(true)
        , MoveLimit(200)
    {}
};

struct eventState_t
{
    bool eventIsActive;
    char eventName[256];
    char returnEventName[256];

    eventState_t()
        : eventIsActive(false)
    {}
};

struct serverState_t
{
    string charName;
    bool inventoryLoaded;
    uint16_t incomingChunkSequence;
    int incomingChunkCount;
    int outgoingChunkUsage;

    serverState_t()
        : charName(string(""))
        , inventoryLoaded(true)
        , incomingChunkSequence(0)
        , incomingChunkCount(0)
        , outgoingChunkUsage(0)
    {}
};

struct itemSlotInfo_t
{
    bool isMoved;
    bool isStuck;
    bool isEquip;
    Ashita::FFXI::item_t item;
    IItem* resource;
    int targetCount;
    list<itemTarget_t> targets;

    itemSlotInfo_t()
        : isMoved(false)
        , isStuck(false)
        , isEquip(false)
        , resource(NULL)
        , targetCount(0)
    {
        item       = Ashita::FFXI::item_t();
        item.Count = 0;
    }

    itemSlotInfo_t(Ashita::FFXI::item_t* pItem, IItem* pResource)
        : isMoved(false)
        , resource(pResource)
        , targetCount(0)
    {
        memcpy(&item, pItem, sizeof(Ashita::FFXI::item_t));
        isStuck = ((item.Flags == 5) || (item.Flags == 19) || (item.Flags == 25));
        isEquip = (resource->Flags & 0x800);
    }

    bool isEqual(Ashita::FFXI::item_t* pItem)
    {
        //We block other plugins from moving items, so we can reliably assume that if we haven't touched it that it hasn't moved.
        if (!isMoved)
            return true;

        if (pItem->Id != item.Id)
            return false;

        if (item.Count != pItem->Count)
            return false;

        if ((item.Count == 0) || (item.Id == 0))
            return true;

        //Compare augments last.  It's much more costly than the ID and count checks.
        if (memcmp(item.Extra, pItem->Extra, 28) == 0)
        {
            isMoved = false;
            return true;
        }

        return false;
    }
};

struct gearState_t
{
    bool active;
    bool reparse;
    int moveCount;
    bool invWarning;

    list<itemOrder_t> equipment;
    list<itemOrder_t> other;

    itemSlotInfo_t items[CONTAINER_MAX][81];
    int reservecount[CONTAINER_MAX][81];
    bool hasContainer[CONTAINER_MAX];
    int containermax[CONTAINER_MAX];
    int freecount[CONTAINER_MAX];
    int firstfreespace[CONTAINER_MAX];
};

struct pkMoveItem_t
{
    uint32_t header;
    uint32_t count;
    uint8_t originContainer;
    uint8_t destinationContainer;
    uint8_t originIndex;
    uint8_t destinationIndex;
};

#endif