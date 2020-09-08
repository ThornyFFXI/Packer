#include "Packer.h"
#include <fstream>
#include <iostream>

augmentData_t packer::createAugmentData(Ashita::FFXI::item_t* item)
{
    augmentData_t currAugment = {0};

    if ((item->Extra[0] != 2) && (item->Extra[0] != 3))
        return currAugment;

    //Crafting shield
    if (item->Extra[1] & 0x08)
    {
        //Unlikely this will be used in ashitacast.
    }

    //Delve system
    else if (item->Extra[1] & 0x20)
    {
        currAugment.path = Ashita::BinaryData::UnpackBitsBE(item->Extra, 16, 2) + 1;
        currAugment.rank = Ashita::BinaryData::UnpackBitsBE(item->Extra, 18, 4);

        //Build raw augment vector
        for (int x = 0; x < 4; x++)
        {
            int32_t augmentId    = Ashita::BinaryData::UnpackBitsBE(item->Extra, (16 * x) + 48, 8);
            int32_t augmentValue = Ashita::BinaryData::UnpackBitsBE(item->Extra, (16 * x) + 56, 8);
            if (augmentId == 0)
                continue;

            singleAugment_t augment;
            augment.realId = augmentId;
            augment.table  = 1;

            for (std::vector<augmentResource_t>::iterator iter = mAugmentData[augment.table][augment.realId].begin(); iter != mAugmentData[augment.table][augment.realId].end(); iter++)
            {
                augment.stat    = (*iter).stat;
                augment.value   = (augmentValue + (*iter).offset) * (*iter).multiplier;
                augment.percent = (*iter).percent;

                //Merge like augments
                std::vector<singleAugment_t>::iterator iter2;
                for (iter2 = currAugment.rawAugments.begin(); iter2 != currAugment.rawAugments.end(); iter2++)
                {
                    if (((*iter2).table == augment.table) && (augment.stat == (*iter2).stat))
                    {
                        (*iter2).value += augment.value;
                        break;
                    }
                }

                //Add to table if no like augment
                if (iter2 == currAugment.rawAugments.end())
                    currAugment.rawAugments.push_back(augment);
            }
        }

        //Build string augment vector
        for (std::vector<singleAugment_t>::iterator iter = currAugment.rawAugments.begin(); iter != currAugment.rawAugments.end(); iter++)
        {
            stringstream stringAugment;
            stringAugment << (*iter).stat;
            if ((*iter).value != 0)
            {
                if ((*iter).value > 0)
                    stringAugment << "+";
                stringAugment << (*iter).value;
                if ((*iter).percent)
                    stringAugment << "%";
            }
            currAugment.stringAugments.push_back(stringAugment.str());
        }

        return currAugment;
    }

    //Dynamis system
    else if (item->Extra[1] == 131)
    {
        currAugment.path = Ashita::BinaryData::UnpackBitsBE(item->Extra, 32, 2) + 1;
        currAugment.rank = Ashita::BinaryData::UnpackBitsBE(item->Extra, 50, 5);
        //pOutput->message_f("Path %d Rank %d", currAugment.path, currAugment.rank);
    }

    //Evolith system
    else if (item->Extra[1] & 0x80)
    {
        //Unsure how this will be implemented, if at all.
    }

    //Basic system
    else
    {
        int maxAugments = 5;
        if (item->Extra[1] & 0x40)
        {
            maxAugments             = 4;
            currAugment.trialNumber = Ashita::BinaryData::UnpackBitsBE(item->Extra, 80, 15);
            //currAugment.trialComplete = Ashita::BinaryData::UnpackBitsBE(item->Extra, 95, 1);  Not in use atm.
        }

        //Build raw augment vector
        for (int x = 0; x < maxAugments; x++)
        {
            int32_t augmentId    = Ashita::BinaryData::UnpackBitsBE(item->Extra, (16 * x) + 16, 11);
            int32_t augmentValue = Ashita::BinaryData::UnpackBitsBE(item->Extra, (16 * x) + 27, 5);
            if (augmentId == 0)
                continue;

            singleAugment_t augment;
            augment.realId = augmentId;
            augment.table  = 0;

            for (std::vector<augmentResource_t>::iterator iter = mAugmentData[augment.table][augment.realId].begin(); iter != mAugmentData[augment.table][augment.realId].end(); iter++)
            {
                augment.stat    = (*iter).stat;
                augment.value   = (augmentValue + (*iter).offset) * (*iter).multiplier;
                augment.percent = (*iter).percent;

                //Merge like augments
                std::vector<singleAugment_t>::iterator iter2;
                for (iter2 = currAugment.rawAugments.begin(); iter2 != currAugment.rawAugments.end(); iter2++)
                {
                    if (((*iter2).table == augment.table) && (augment.stat == (*iter2).stat))
                    {
                        (*iter2).value += augment.value;
                        break;
                    }
                }

                //Add to table if no like augment
                if (iter2 == currAugment.rawAugments.end())
                    currAugment.rawAugments.push_back(augment);
            }
        }

        //Build string augment vector
        for (std::vector<singleAugment_t>::iterator iter = currAugment.rawAugments.begin(); iter != currAugment.rawAugments.end(); iter++)
        {
            stringstream stringAugment;
            stringAugment << (*iter).stat;
            if ((*iter).value != 0)
            {
                if ((*iter).value > 0)
                    stringAugment << "+";
                stringAugment << (*iter).value;
                if ((*iter).percent)
                    stringAugment << "%";
            }
            currAugment.stringAugments.push_back(stringAugment.str());
        }
    }
    return currAugment;
}
void packer::initAugmentData()
{
    stringstream file;
    file << m_AshitaCore->GetInstallPath() << "resources\\packer\\augments.xml";
    ifstream fileReader(file.str().c_str(), ios::in | ios::binary | ios::ate);
    if (fileReader.is_open())
    {
        long size    = fileReader.tellg();
        char* buffer = new char[size + 1];
        fileReader.seekg(0, ios::beg);
        fileReader.read(buffer, size);
        fileReader.close();
        buffer[size] = 0x00;

        xml_document<> doc;
        try
        {
            doc.parse<0>(buffer);
        }
        catch (...)
        {
            delete[] buffer;
            return;
        }
        xml_node<>* node = doc.first_node("augmentdata");
        for (node = node->first_node("augments"); node; node = node->next_sibling("augments"))
        {
            int attr = atoi(node->first_attribute("table")->value());
            for (xml_node<>* child = node->first_node("augment"); child; child = child->next_sibling("augment"))
            {
                int id = atoi(child->first_attribute("id")->value());
                for (xml_node<>* subchild = child->first_node("params"); subchild; subchild = subchild->next_sibling("params"))
                {
                    augmentResource_t temp;
                    temp.id                  = id;
                    temp.realid              = id;
                    temp.multiplier          = 1;
                    temp.percent             = false;
                    temp.offset              = 0;
                    temp.stat                = "";
                    xml_attribute<>* subattr = subchild->first_attribute("stat");
                    if (subattr)
                        temp.stat = subattr->value();
                    subattr = subchild->first_attribute("offset");
                    if (subattr)
                        temp.offset = atoi(subattr->value());
                    subattr = subchild->first_attribute("multiplier");
                    if (subattr)
                        temp.multiplier = atoi(subattr->value());
                    subattr = subchild->first_attribute("percent");
                    if (subattr)
                        temp.percent = subattr->value();
                    for (int x = 0; x < id; x++)
                    {
                        for (std::vector<augmentResource_t>::iterator it = mAugmentData[attr][x].begin(); it != mAugmentData[attr][x].end(); it++)
                        {
                            if (((*it).stat == temp.stat) && ((*it).multiplier == temp.multiplier))
                                temp.realid = id;
                        }
                    }
                    mAugmentData[attr][id].push_back(temp);
                }
            }
        }
        delete[] buffer;
    }
}