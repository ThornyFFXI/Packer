#include "Packer.h"
#include <fstream>
#include <iostream>

void packer::loadDefaultProfile(bool forceReload)
{
    stopEvent();

    bool exists;
    std::string path = pSettings->GetCharacterSettingsPath(mServer.charName.c_str());

    if (path == "FILE_NOT_FOUND")
    {
        path = pSettings->GetDefaultSettingsPath();
        if (!writeDefaultProfile(path))
        {
            pOutput->error_f("Default profile could not be created. [$H%s$R]", path.c_str());
            return;        
        }
    }

    if ((forceReload) || (path != pSettings->GetLoadedXmlPath()))
    {
        clearProfile();
        xml_document<>* document = pSettings->LoadSettingsXml(path);
        if (document != NULL)
        {
            if (parseProfileXml(document))
            {
                pOutput->message_f("Config loaded. [$H%s$R]", path.c_str());
            }
            else
            {
                clearProfile();
            }
        }
    }
}
void packer::loadSpecificProfile(string fileName)
{
    stopEvent();
    clearProfile();

    std::string path = pSettings->GetInputSettingsPath(fileName.c_str());
    if (path == "FILE_NOT_FOUND")
    {
        pOutput->error_f("Profile could not be loaded.  File not found.  [$H%s$R]", fileName.c_str());
        return;
    }

    xml_document<>* document = pSettings->LoadSettingsXml(path);
    if (document != NULL)
    {
        if (parseProfileXml(document))
        {
            pOutput->message_f("Config loaded. [$H%s$R]", path.c_str());
        }
        else
        {
            clearProfile();
        }
    }
}
bool packer::parseProfileXml(xml_document<>* xmlDocument)
{
    xml_node<>* root = xmlDocument->first_node("packer");
    if (!root)
    {
        pOutput->error("Could not evaluate config XML.  Root packer node not found.");
        return false;
    }
    xml_node<>* node = root->first_node("settings", 0, false);
    if (node)
    {
        for (xml_node<>* child = node->first_node(); child; child = child->next_sibling())
        {
            if (_stricmp(child->name(), "equipbags") == 0)
            {
                mConfig.EquipBags.clear();
                stringstream stream(child->value());
                while (stream.good())
                {
                    string bag;
                    getline(stream, bag, ',');
                    int bagIndex = atoi(bag.c_str());
                    if ((bagIndex < 0) || (bagIndex > 16)) continue;
                    mConfig.EquipBags.push_back(bagIndex);
                }                
            }
            else if (_stricmp(child->name(), "forceenablebags") == 0)
            {
                mConfig.ForceEnableBags.clear();
                stringstream stream(child->value());
                while (stream.good())
                {
                    string bag;
                    getline(stream, bag, ',');
                    if (bag.length() < 1)
                        break;
                    int bagIndex = atoi(bag.c_str());
                    if ((bagIndex < 0) || (bagIndex > 16))
                        continue;
                    mConfig.ForceEnableBags.push_back(bagIndex);
                }
            }
            else if (_stricmp(child->name(), "forcedisablebags") == 0)
            {
                mConfig.ForceDisableBags.clear();
                stringstream stream(child->value());
                while (stream.good())
                {
                    string bag;
                    if (bag.length() < 1)
                        break;
                    getline(stream, bag, ',');
                    int bagIndex = atoi(bag.c_str());
                    if ((bagIndex < 0) || (bagIndex > 12))
                        continue;
                    mConfig.ForceDisableBags.push_back(bagIndex);
                }
            }
            else if (_stricmp(child->name(), "threading") == 0)
            {
                if (_stricmp(child->value(), "false") == 0)
                    mConfig.EnableThreading = false;
            }
            else if (_stricmp(child->name(), "weaponpriority") == 0)
            {
                if (_stricmp(child->value(), "false") == 0)
                    mConfig.EnableWeaponPriority = false;
            }
            else if (_stricmp(child->name(), "dirtypackets") == 0)
            {
                if (_stricmp(child->value(), "true") == 0)
                    mConfig.EnableDirtyPackets = true;
            }
            else if (_stricmp(child->name(), "naked") == 0)
            {
                if (_stricmp(child->value(), "false") == 0)
                    mConfig.EnableNaked = false;
            }
            else if (_stricmp(child->name(), "nomadstorage") == 0)
            {
                if (_stricmp(child->value(), "true") == 0)
                    mConfig.EnableNomadStorage = true;
            }
            else if (_stricmp(child->name(), "defrag") == 0)
            {
                if (_stricmp(child->value(), "false") == 0)
                    mConfig.EnableDefrag = false;
            }
            else if (_stricmp(child->name(), "benchmark") == 0)
            {
                if (_stricmp(child->value(), "false") == 0)
                    mConfig.EnableBenchmark = false;
            }
            else if (_stricmp(child->name(), "debug") == 0)
            {
                if (_stricmp(child->value(), "false") == 0)
                    mConfig.EnableDebug = false;
            }
            else if (_stricmp(child->name(), "validateaftergear") == 0)
            {
                if (_stricmp(child->value(), "false") == 0)
                    mConfig.EnableDebug = false;
            }
            else if (_stricmp(child->name(), "movelimit") == 0)
            {
                int value                                   = atoi(child->value());
                mConfig.MoveLimit = max(min(200, value), 0);
            }
            else
            {
                pOutput->error_f("Unrecognized node found in settings.  It will be ignored.  [$H%s$R]", child->name());
            }
        }      
    }

    node = root->first_node("priority", 0, false);
    if (node)
    {
        for (xml_node<>* child = node->first_node(); child; child = child->next_sibling())
        {
            int containerIndex = getContainerIndex(child->name());
            if (containerIndex == -1)
            {
                pOutput->error_f("Unrecognized node found in priority.  It will be ignored.  [$H%s$R]", child->name());
                continue;
            }

            if (std::find_if(mConfig.containers.begin(), mConfig.containers.end(), [&containerIndex](const containerInfo_t& arg) { return arg.containerIndex == containerIndex; }) != mConfig.containers.end())
            {
                pOutput->error_f("Node found twice in priority.  Packer will only honor the first instance.  [$H%s$R]", child->name());
                continue;
            }

            containerInfo_t targetContainer = containerInfo_t();
            targetContainer.containerIndex  = containerIndex;

            xml_attribute<>* attr = child->first_attribute("compress");
            if ((attr) && (_stricmp(attr->value(), "false") == 0))
                targetContainer.canRetrieve = false;

            attr = child->first_attribute("equip");
            if ((attr) && (_stricmp(attr->value(), "true") == 0))
                targetContainer.canStoreEquipment = true;

            attr = child->first_attribute("other");
            if ((attr) && (_stricmp(attr->value(), "true") == 0))
                targetContainer.canStoreItems = true;

            mConfig.containers.push_back(targetContainer);
        }
    }
    for (xml_node<>* node = root->first_node(); node; node = node->next_sibling())
    {
        if ((_stricmp(node->name(), "settings") == 0) || (_stricmp(node->name(), "priority") == 0))
            continue;

        int containerIndex = getContainerIndex(node->name());
        if (containerIndex == -1)
        {
            pOutput->error_f("Unrecognized base node found.  It will be ignored.  [$H%s$R]", node->name());
            continue;
        }

        std::list<containerInfo_t>::iterator iter = std::find_if(mConfig.containers.begin(), mConfig.containers.end(), [&containerIndex](const containerInfo_t& arg) { return arg.containerIndex == containerIndex; });
        if (iter == mConfig.containers.end())
        {
            pOutput->error_f("A container was listed as base node, but is not in priority($H%s$R).  It will be ignored.", node->name());
            continue;
        }

        for (xml_node<>* child = node->first_node(); child; child = child->next_sibling())
        {
            if ((_stricmp(child->name(), "item") == 0) && (strlen(child->value()) > 0))
            {
                IItem* pResource = m_AshitaCore->GetResourceManager()->GetItemByName(child->value(), 0);
                if (!pResource)
                {
                    int itemId = atoi(child->value());
                    if ((itemId > 0) && (itemId < 65535))
                        pResource = m_AshitaCore->GetResourceManager()->GetItemById(itemId);
                }
                if (pResource)
                    iter->needed.push_back(itemOrder_t(child, pResource));
                else
                {
                    pOutput->error_f("Invalid item node found in $H%s$R.  It will be ignored.  [$H%s$R]", node->name(), child->value());
                    continue;
                }
            }
            else
            {
                pOutput->error_f("Unrecognized node found in $H%s$R.  It will be ignored.  [$H%s$R]", node->name(), child->name());
                continue;
            }
        }
    }

    return true;
}
bool packer::writeDefaultProfile(string path)
{
    pSettings->CreateDirectories(path.c_str());
    ofstream myfile(path.c_str(), ofstream::binary);
    if (myfile.is_open())
    {
        myfile << "<packer>\n\n";
        myfile << "\t<settings>\n";
        myfile << "\t\t<equipbags>8,10,11,12,13,14,15,16,0</equipbags> <!-- List of container indices you can equip items from.  Should only be changed when on a private server that allows players to equip from non-standard containers. -->\n";
        myfile << "\t\t<forceenablebags></forceenablebags> <!-- List of bags that will always be treated as accessible.  Primarily for private server usage.-->\n";
        myfile << "\t\t<forcedisablebags></forcedisablebags> <!-- List of bags that will always be treated as inaccessible.  Primarily for private server usage.-->\n";
        myfile << "\t\t<threading>true</threading> <!--When enabled, initial move lists will be created in a background thread.  Disabling this will cause client stutter when parsing or reparsing inventories.  You may still want to disable it on computers with very few cores or very high single core performance.-->\n";
        myfile << "\t\t<weaponpriority>true</weaponpriority> <!--If this is enabled, all weapons will be stored in your first equipment bag.  This makes it easier to swap weapons from the menu without searching bags.  Since default first bag is 8(wardrobe), that is where they will be unless modified.-->\n";
        myfile << "\t\t<dirtypackets>false</dirtypackets> <!--When enabled, packer will use item move packets to directly combine stacks and bypass inventory.  This is not possible with the legitimate game client and may be a ban risk, but will increase speed.-->\n";
        myfile << "\t\t<defrag>true</defrag> <!--When enabled, packer will combine like stacks after organizing.  Requires a movelimit of 4 or higher, or dirtypackets enabled.-->\n";
        myfile << "\t\t<benchmark>true</benchmark> <!--When enabled, packer will print the amount of time various steps take to ashita log for troubleshooting.-->\n";
        myfile << "\t\t<debug>true</debug> <!--When enabled, packer will print a large amount of information to ashita log for troubleshooting.-->\n";
        myfile << "\t\t<nomadstorage>false</nomadstorage> <!--When enabled, Packer will access storage bag at nomad moogles despite the menu not directly allowing it.  This is not possible with the legitimate game client and may be a ban risk.-->\n";
        myfile << "\t\t<validateaftergear>true</validateaftergear> <!--When enabled, Packer will automatically do a validate after you use /ac gear or /packer gear to show which items are missing.-->\n";
        myfile << "\t\t<movelimit>200</movelimit> <!--Limits the amount of items moved per packet interval.  Packet size makes for a maximum of 166 when using dirty packets, or 83 without, so any value higher than this is effectively unlimited.-->\n";
        myfile << "\t</settings>\n\n";

        myfile << "\t<priority> <!--Set which containers will have items stored in them, and which item types can be stored in them.-->\n";
        myfile << "\t<!--List all your bags, in the order you want them filled.\n";
        myfile << "\tIf a bag is not listed at all, it will be as if it does not exist and items will never be added to or removed from it.\n";
        myfile << "\tPacker will only store equipment in bags with 'equip=\"true\"' and will only store non-equipment in bags with 'other=\"true\"' attributes.\n";
        myfile << "\tIf an item is listed in an item node, it will always go in that bag regardless of attributes, unless a GEAR command requires it and the bag is not equippable.  So, you can list equipment in item nodes even if the bag does not allow equipment, for example.\n";
        myfile << "\tBy default, Packer will move items from later bags to earlier bags to make more space in your later bags.\n";
        myfile << "\tIf you set 'compress=\"false\"', Packer will not remove items from a bag to make space, only if the items are required to be elsewhere.\n";
        myfile << "\tIf you do not enable dirtypackets, it is highly recommended that you set inventory as last bag.\n";
        myfile << "\tOtherwise, your inventory is likely to fill up, and since items have to travel through inventory to reach other bags you will reach a jam.\n";
        myfile << "\tBesides, who wants a full inventory in this game?  Why?  Just don't do it. -->\n";
        
        myfile << "\t\t<" << gContainerNames[1] << " equip=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[2] << " equip=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[4] << " equip=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[9] << " equip=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[8] << " equip=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[10] << " equip=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[11] << " equip=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[12] << " equip=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[13] << " equip=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[14] << " equip=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[15] << " equip=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[16] << " equip=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[5] << " other=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[6] << " other=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[7] << " other=\"true\" />\n";
        myfile << "\t\t<" << gContainerNames[0] << " compress=\"false\" />\n";

        myfile << "\t</priority>\n\n";

        myfile << "\t<!--List bags at the same level as priority/settings if you want to specify items that always belong in those bags.-->\n";

        for (int x = 0; x < CONTAINER_MAX; x++)
        {
            //Skip temporary items.
            if (x == 3)
                x++;
            myfile << "\t<" << gContainerNames[x] << ">\n";
            myfile << "\t</" << gContainerNames[x] << ">\n\n";
        }
        myfile << "</packer>";
        myfile.close();
        return true;
    }
    return false;
}
void packer::clearProfile()
{
    mConfig = packerConfig_t();
}

bool packer::parseAshitacastXml(xml_document<>* xmlDocument, std::list<itemOrder_t>* equipment, std::list<itemOrder_t>* items)
{
    equipment->clear();
    items->clear();
    xml_node<>* node = xmlDocument->first_node("ashitacast");
    if (!node)
    {
        pOutput->error("Could not evaluate ashitacast profile.  Root ashitacast node not found.");
        return false;
    }
    node = node->first_node("sets");
    if (!node)
    {
        pOutput->error("Could not evaluate ashitacast profile.  Sets node not found.");
        return false;
    }

    for (node = node->first_node(); node; node = node->next_sibling())
    {
        if (_stricmp(node->name(), "set") == 0)
        {
            string setName = "Unknown";
            xml_attribute<>* name = node->first_attribute("name");
            if (name)
                setName = name->value();
            list<int> foundSlots;

            list<itemOrder_t> tempItems;
            for (xml_node<>* child = node->first_node(); child; child = child->next_sibling())
            {
                //Check that node is a valid equipment slot, and has not already been used in this set.
                int slot = getSlotIndex(child->name());
                if (slot == -1)
                {
                    pOutput->error_f("Unrecognized node found.  It will be ignored.  [Set:$H%s$R] [Node:$H%s$R]", setName.c_str(), child->name());
                    continue;
                }
                if (find(foundSlots.begin(), foundSlots.end(), slot) != foundSlots.end())
                {
                    pOutput->error_f("Same slot was used twice in a set.  Packer will only honor the first instance.  [Set:$H%s$R] [Node:$H%s$R]", setName.c_str(), child->name());
                    continue;
                }
                foundSlots.push_back(slot);

                //Skip blank nodes, keyword nodes, and nodes with a variable in their value.
                if ((strlen(child->value()) == 0)
                    || (_stricmp(child->value(), "ignore") == 0)
                    || (_stricmp(child->value(), "displaced") == 0)
                    || (_stricmp(child->value(), "remove") == 0)
                    || (strstr(child->value(), "$")))
                    continue;

                //Check that item name matches a valid node.
                IItem* pResource = m_AshitaCore->GetResourceManager()->GetItemByName(child->value(), 0);
                if (!pResource)
                {
                    pOutput->error_f("Invalid item node found.  It will be ignored.  [Set:$H%s$R] [Node:$H%s$R] [Item:$H%s$R]", setName.c_str(), child->name(), child->value());
                    continue;
                }
                else if (!(pResource->Flags & 0x0800))
                {
                    pOutput->error_f("Unequippable item node found.  It will be ignored.  [Set:$H%s$R] [Node:$H%s$R] [Item:$H%s$R]", setName.c_str(), child->name(), child->value());
                    continue;
                }

                //Combine adds counts.  Ensures that if the same item is in a set multiple times, it gets counted appropriately.
                combineOrder(&tempItems, itemOrder_t(child, pResource));
            }

            for (list<itemOrder_t>::iterator iter = tempItems.begin(); iter != tempItems.end(); iter++)
            {
                //Integrate keeps highest count, we don't want to add every instance of the item being in any set.
                integrateOrder(equipment, *iter);
            }
        }
        else if (_stricmp(node->name(), "packer") == 0)
        {
            for (xml_node<>* child = node->first_node(); child; child = child->next_sibling())
            {
                if (_stricmp(child->name(), "item") == 0)
                {
                    IItem* pResource = m_AshitaCore->GetResourceManager()->GetItemByName(child->value(), 0);
                    if (!pResource)
                    {
                        int itemId = atoi(child->value());
                        if ((itemId > 0) && (itemId < 65535))
                            pResource = m_AshitaCore->GetResourceManager()->GetItemById(itemId);
                    }
                    if (!pResource)
                    {
                        pOutput->error_f("Invalid item node found.  It will be ignored.  [Packer Section] [Node:$H%s$R] [Item:$H%s$R]", child->name(), child->value());
                        continue;
                    }

                    //Integrate keeps highest count, we don't want to add include to sets in case item is in both.
                    if (pResource->Flags & 0x0800)
                    {
                        integrateOrder(equipment, itemOrder_t(child, pResource));
                    }
                    else
                    {
                        integrateOrder(items, itemOrder_t(child, pResource));
                    }
                }
                else
                {
                    pOutput->error_f("Unrecognized node found.  It will be ignored.  [Packer Section] [Node:$H%s$R]", child->name());
                    continue;
                }
            }
        }
        else
        {
            pOutput->error_f("Unrecognized node found.  It will be ignored.  [Sets Section] [Node:$H%s$R]", node->name());
            continue;
        }
    }

    return true;
}
void packer::combineOrder(list<itemOrder_t>* list, itemOrder_t order)
{
    for (std::list<itemOrder_t>::iterator iOrder = list->begin(); iOrder != list->end(); iOrder++)
    {
        if (*iOrder == order)
        {
            //Items match, so merge.
            if ((iOrder->all) || (order.all))
            {
                iOrder->all = true;
                iOrder->quantityNeeded = 0;
            }
            else
                iOrder->quantityNeeded = iOrder->quantityNeeded + order.quantityNeeded;
            return;
        }
    }
    list->push_back(order);
}
void packer::integrateOrder(list<itemOrder_t>* list, itemOrder_t order)
{
    for (std::list<itemOrder_t>::iterator iOrder = list->begin(); iOrder != list->end(); iOrder++)
    {
        if (*iOrder == order)
        {
            //Items match, so merge.
            if ((iOrder->all) || (order.all))
            {
                iOrder->all      = true;
                iOrder->quantityNeeded = 0;
            }
            else
                iOrder->quantityNeeded = max(iOrder->quantityNeeded, order.quantityNeeded);
            return;
        }
    }
    list->push_back(order);
}