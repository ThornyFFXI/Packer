#include "Packer.h"

void packer::validate(const char* filename)
{
    string profile(filename);

    if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(profile.c_str())) //Check for absolute path
    {
        profile = string(m_AshitaCore->GetInstallPath()) + "config\\ashitacast\\" + string(filename);
        if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(profile.c_str())) //Check for filename in packer's config directory
        {
            if (endsWith(profile, ".xml"))
            {
                pOutput->error_f("Validate failed.  File not found.  [$H%s$R]", profile.c_str());
                return;
            }
            profile += ".xml";
            if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(profile.c_str())) //Check if user omitted the .xml
            {
                pOutput->error_f("Validate failed.  File not found.  [$H%s$R]", profile.c_str());
                return;
            }
        }
    }

    char* buffer = NULL;
    xml_document<>* document = pSettings->LoadXml(profile, buffer);
    if (document != NULL)
    {
        validate(document);
        delete[] buffer;
        delete document;
    }
}

void packer::validate(xml_document<>* document)
{
    std::list<itemOrder_t> equipment;
    std::list<itemOrder_t> items;

    if (!mServer.inventoryLoaded)
    {
        pOutput->error("Could not perform validate.  Inventory has not finished loading since you last zoned.");
        return;
    }

    if (!parseAshitacastXml(document, &equipment, &items))
        return;

    checkMog();
    validate(&equipment, &items);
}

void packer::validate(GearListEvent_t* validateEvent)
{
    if (!mServer.inventoryLoaded)
    {
        pOutput->error("Could not perform validate.  Inventory has not finished loading since you last zoned.");
        return;
    }

    std::list<itemOrder_t> equipment;
    std::list<itemOrder_t> items;
    for (int x = 0; x < validateEvent->EntryCount; x++)
    {
        IItem* pResource = m_AshitaCore->GetResourceManager()->GetItemByName(validateEvent->Entries[x].Name, 0);
        if (!pResource)
        {
            pOutput->error_f("Invalid item data received from validate event.  It will be ignored.  [Item:$H%s$R]", validateEvent->Entries[x].Name);
            continue;
        }
        else if (pResource->Flags & 0x0800)
        {
            equipment.push_back(itemOrder_t(validateEvent->Entries[x], pResource));
        }
        else
        {
            items.push_back(itemOrder_t(validateEvent->Entries[x], pResource));
        }
    }

    checkMog();
    validate(&equipment, &items);
}


void packer::validate(std::list<itemOrder_t>* equipment, std::list<itemOrder_t>* items)
{
    for (std::list<int>::iterator bagIter = mConfig.EquipBags.begin(); bagIter != mConfig.EquipBags.end(); bagIter++)
    {
        if (!mGear.hasContainer[*bagIter])
            continue;

        for (int x = 1; x <= pInv->GetContainerCountMax(*bagIter); x++)
        {
            Ashita::FFXI::item_t* pItem = pInv->GetContainerItem((*bagIter), x);
            if ((pItem->Id == 0) || (pItem->Count == 0))
                continue;

            for (std::list<itemOrder_t>::iterator iEquip = equipment->begin(); iEquip != equipment->end();)
            {
                bool deleted = false;
                if (itemMatchesOrder(*iEquip, pItem))
                {
                    iEquip->quantityFound += pItem->Count;
                    if ((!iEquip->all) && (iEquip->quantityFound >= iEquip->quantityNeeded))
                    {
                        iEquip  = equipment->erase(iEquip);
                        deleted = true;
                    }
                }
                if (!deleted)
                    iEquip++;
            }

            if (*bagIter == 0)
            {
                for (std::list<itemOrder_t>::iterator iItemList = items->begin(); iItemList != items->end();)
                {
                    bool deleted = false;
                    if (itemMatchesOrder(*iItemList, pItem))
                    {
                        iItemList->quantityFound += pItem->Count;
                        if ((!iItemList->all) && (iItemList->quantityFound >= iItemList->quantityNeeded))
                        {
                            iItemList = items->erase(iItemList);
                            deleted   = true;
                        }
                    }
                    if (!deleted)
                        iItemList++;
                }
            }
        }
    }

    std::vector<string> equipOutput;
    for (std::list<itemOrder_t>::iterator iEquip = equipment->begin(); iEquip != equipment->end(); iEquip++)
    {
        if (iEquip->quantityFound >= iEquip->quantityNeeded)
            continue;

        char buffer[512];
        if (iEquip->all)
            sprintf_s(buffer, 512, "%d/?? %s%s", iEquip->quantityFound, Ashita::Chat::Colors::LawnGreen, getValidateString(*iEquip, (iEquip->quantityNeeded > 1)).c_str());
        else
            sprintf_s(buffer, 512, "%d/%d %s%s", iEquip->quantityFound, iEquip->quantityNeeded, Ashita::Chat::Colors::LawnGreen, getValidateString(*iEquip, (iEquip->quantityNeeded > 1)).c_str());
        equipOutput.push_back(buffer);
    }

    std::vector<string> itemOutput;
    for (std::list<itemOrder_t>::iterator iItemList = items->begin(); iItemList != items->end(); iItemList++)
    {
        if (iItemList->quantityFound >= iItemList->quantityNeeded)
            continue;

        char buffer[512];
        if (iItemList->all)
            sprintf_s(buffer, 512, "%d/?? %s%s", iItemList->quantityFound, Ashita::Chat::Colors::LawnGreen, getValidateString(*iItemList, (iItemList->quantityNeeded > 1)).c_str());
        else
            sprintf_s(buffer, 512, "%d/%d %s%s", iItemList->quantityFound, iItemList->quantityNeeded, Ashita::Chat::Colors::LawnGreen, getValidateString(*iItemList, iItemList->quantityNeeded > 1).c_str());
        itemOutput.push_back(buffer);
    }

    if (equipOutput.size() > 0)
    {
        pOutput->message("Listing missing equipment..");
        for (std::vector<string>::iterator iter = equipOutput.begin(); iter != equipOutput.end(); iter++)
        {
            pOutput->message(*iter);
        }
    }
    else
    {
        pOutput->message("All required equipment is present in equippable bags.");
    }

    if (itemOutput.size() > 0)
    {
        pOutput->message("Listing missing non-equipment..");
        for (std::vector<string>::iterator iter = itemOutput.begin(); iter != itemOutput.end(); iter++)
        {
            pOutput->message(*iter);
        }
    }
    else
    {
        pOutput->message("All required non-equipment is present in inventory.");
    }
}