// Copyright (C) 2004 WoW Daemon
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "Common.h"
#include "GameObject.h"
#include "UpdateMask.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"

GameObject::GameObject() : Object()
{
    m_objectType |= TYPE_GAMEOBJECT;
    m_objectTypeId = TYPEID_GAMEOBJECT;

    m_valuesCount = GAMEOBJECT_END;
    m_RespawnTimer = 0;
    m_gold = 0;
    m_ItemCount = 0;
}


void GameObject::Create( uint32 guidlow, uint32 display_id, uint8 state, uint32 obj_field_entry, float scale, uint16 type,
    uint16 faction, uint32 mapid, float x, float y, float z, float ang )
{
    printf("create game object\n");
    Object::_Create(guidlow, 0xF0007000, mapid, x, y, z, ang);

    SetUInt32Value( OBJECT_FIELD_ENTRY, obj_field_entry ); // no idea yet
    SetFloatValue( OBJECT_FIELD_SCALE_X, scale );
    SetUInt32Value( GAMEOBJECT_DISPLAYID, display_id );
    SetUInt32Value( GAMEOBJECT_STATE, state  );
    SetUInt32Value( GAMEOBJECT_TYPE_ID, type  );
    SetUInt32Value( GAMEOBJECT_TIMESTAMP, (uint32)time(NULL)); // ??
    SetFloatValue( GAMEOBJECT_POS_X, x);
    SetFloatValue( GAMEOBJECT_POS_Y, y );
    SetFloatValue( GAMEOBJECT_POS_Z, z );
    SetFloatValue( GAMEOBJECT_FACING, ang );
    SetFloatValue( GAMEOBJECT_FLAGS, 0 );
}

void GameObject::Update(uint32 p_time)
{
    WorldPacket data;
    // Respawn Timer
    if(m_RespawnTimer > 0){
        if(m_RespawnTimer > p_time)
            m_RespawnTimer -= p_time;
        else{
            if(!this->IsInWorld()){
                PlaceOnMap();
            }

            data.Initialize(SMSG_GAMEOBJECT_SPAWN_ANIM);
            data << GetGUID();
            SendMessageToSet(&data,true);

            SetUInt32Value(GAMEOBJECT_STATE,1);
            m_RespawnTimer = 0;
        }
    }
}

void GameObject::Despawn(uint32 time)
{
    WorldPacket data;
    RemoveFromMap();

    data.Initialize(SMSG_GAMEOBJECT_DESPAWN_ANIM);
    data << GetGUID();
    SendMessageToSet(&data,true);

    m_RespawnTimer = time;
    m_ItemCount = 0;
}

bool GameObject::FillLoot(WorldPacket *data)
{
    FillItemList();

    *data << this->GetGUID();
    *data << uint8(0x01);
    *data << m_gold;  // Loot Money
    *data << m_ItemCount;  // item Count


    for(uint8 i = 0; i<=m_ItemCount ; i++)
    {
        if (m_ItemAmount[i] > 0)
        {
            *data << uint8(i);
            ItemPrototype* tmpLootItem = objmgr.GetItemPrototype(m_ItemList[i]);
            if(!tmpLootItem)
                return false;
            *data << m_ItemList[i];
            *data << uint32(m_ItemAmount[i]);
            *data << uint32(tmpLootItem->DisplayInfoID);
            *data << uint32(0) << uint32(0) << uint8(0);
        }
    }
    return true;
}

void GameObject::FillItemList()
{
    uint32 i = 0;
    for(i=0;i<10;i++){
        m_ItemAmount[i] = 0;
        m_ItemList[i] = 0;
    }
    i = 0;

    if(GetUInt32Value(OBJECT_FIELD_ENTRY) == 1617 || GetUInt32Value(UNIT_FIELD_DISPLAYID) == 270){ // Silverleaf
        m_ItemCount = 1;
        m_ItemAmount[i] = 1;//rand()%3;
        m_ItemList[i] = 765;
    }
}

void GameObject::SaveToDB()
{
    std::stringstream ss;
    ss << "DELETE FROM gameobjects WHERE id=" << GetGUIDLow();
    sDatabase.Execute(ss.str().c_str());

    ss.rdbuf()->str("");
    ss << "INSERT INTO gameobjects (id, mapId, positionX, positionY, positionZ, orientation, data) VALUES ( "
        << GetGUIDLow() << ", "
        << GetMapId() << ", "
        << m_positionX << ", "
        << m_positionY << ", "
        << m_positionZ << ", "
        << m_orientation << ", '";

    for( uint16 index = 0; index < m_valuesCount; index ++ )
        ss << GetUInt32Value(index) << " ";

    ss << "' )";

    sDatabase.Execute( ss.str( ).c_str( ) );
}


void GameObject::LoadFromDB(uint32 guid)
{
    std::stringstream ss;
    ss << "SELECT * FROM gameobjects WHERE id=" << guid;

    QueryResult *result = sDatabase.Query( ss.str().c_str() );
    ASSERT(result);

    Field *fields = result->Fetch();

    Create(fields[0].GetUInt32(),1,1,0,1,1,0,fields[5].GetUInt32(),fields[1].GetFloat(),fields[2].GetFloat(),fields[3].GetFloat(),fields[4].GetFloat());

    m_zoneId = 0;

    LoadValues(fields[6].GetString());

    delete result;
}

void GameObject::DeleteFromDB()
{
    char sql[256];

    sprintf(sql, "DELETE FROM gameobjects WHERE id=%u", GetGUIDLow());
    sDatabase.Execute(sql);
}
