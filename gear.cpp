#include "Packer.h"

void packer::gear(const char* filename)
{
    string profile(filename);

    if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(profile.c_str())) //Check for absolute path
    {
        profile = string(m_AshitaCore->GetInstallPath()) + "config\\ashitacast\\" + string(filename);
        if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(profile.c_str())) //Check for filename in packer's config directory
        {
            if (endsWith(profile, ".xml"))
            {
                pOutput->error_f("Gear failed.  File not found.  [$H%s$R]", profile.c_str());
                return;
            }
            profile += ".xml";
            if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(profile.c_str())) //Check if user omitted the .xml
            {
                pOutput->error_f("Gear failed.  File not found.  [$H%s$R]", profile.c_str());
                return;
            }
        }
    }

    char* buffer = NULL;
    xml_document<>* document = pSettings->LoadXml(filename, buffer);
    if (document != NULL)
    {
        gear(document);
        delete[] buffer;
        delete document;
    }
    else
    {
        m_AshitaCore->GetPluginManager()->RaiseEvent("packer_failed", nullptr, 0);
        return;
    }
}

void packer::gear(xml_document<>* document)
{
    if (!parseAshitacastXml(document, &mGear.equipment, &mGear.other))
    {
        m_AshitaCore->GetPluginManager()->RaiseEvent("packer_failed", nullptr, 0);
        return;
    }

    mGear.equipWarning     = false;
    mGear.loadWarning      = false;
    mGear.useequipmentlist = true;
    mGear.reparse     = true;
    mGear.active = true;
    m_AshitaCore->GetPluginManager()->RaiseEvent("packer_started", nullptr, 0);
}
void packer::gear(GearListEvent_t* gearEvent)
{
    mGear.equipment.clear();
    mGear.other.clear();
    for (int x = 0; x < gearEvent->EntryCount; x++)
    {
        IItem* pResource = m_AshitaCore->GetResourceManager()->GetItemByName(gearEvent->Entries[x].Name, 0);
        if (!pResource)
        {
            pOutput->error_f("Invalid item data received from gear event.  It will be ignored.  [Item:$H%s$R]", gearEvent->Entries[x].Name);
            continue;
        }
        else if (pResource->Flags & 0x0800)
        {
            mGear.equipment.push_back(itemOrder_t(gearEvent->Entries[x], pResource));
        }
        else
        {
            mGear.other.push_back(itemOrder_t(gearEvent->Entries[x], pResource));
        }
    }

    mGear.equipWarning     = false;
    mGear.loadWarning      = false;
    mGear.useequipmentlist = true;
    mGear.reparse = true;
    mGear.active = true;
    m_AshitaCore->GetPluginManager()->RaiseEvent("packer_started", nullptr, 0);
}