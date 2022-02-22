#include "Packer.h"

void packer::organize()
{
    mGear.active = true;
    mGear.useequipmentlist = false;
    mGear.reparse = true;
    m_AshitaCore->GetPluginManager()->RaiseEvent("packer_started", nullptr, 0);
}