#include "protocol.h"
#include <cstring> // memcpy
#include "bitstream.h"
#include <iostream>

void send_join(ENetPeer *peer)
{
  BitstreamWriter bs;
  bs.write(E_CLIENT_TO_SERVER_JOIN);
  ENetPacket *packet = enet_packet_create(bs.data(), bs.size(), ENET_PACKET_FLAG_RELIABLE);

  enet_peer_send(peer, 0, packet);
}

void send_new_entity(ENetPeer *peer, const Entity &ent)
{
  BitstreamWriter bs;
  bs.write(E_SERVER_TO_CLIENT_NEW_ENTITY, ent);
  ENetPacket *packet = enet_packet_create(bs.data(), bs.size(), ENET_PACKET_FLAG_RELIABLE);

  enet_peer_send(peer, 0, packet);
}

void send_set_controlled_entity(ENetPeer *peer, uint16_t eid)
{
  BitstreamWriter bs;
  bs.write(E_SERVER_TO_CLIENT_SET_CONTROLLED_ENTITY, eid);
  ENetPacket *packet = enet_packet_create(bs.data(), bs.size(), ENET_PACKET_FLAG_RELIABLE);

  enet_peer_send(peer, 0, packet);
}

void send_entity_state(ENetPeer *peer, uint16_t eid, float x, float y)
{
  BitstreamWriter bs;
  bs.write(E_CLIENT_TO_SERVER_STATE, eid, x, y);
  ENetPacket *packet = enet_packet_create(bs.data(), bs.size(), ENET_PACKET_FLAG_UNSEQUENCED);

  enet_peer_send(peer, 1, packet);
}

void send_snapshot(ENetPeer *peer, uint16_t eid, float x, float y)
{
  BitstreamWriter bs;
  bs.write(E_SERVER_TO_CLIENT_SNAPSHOT, eid, x, y);
  ENetPacket *packet = enet_packet_create(bs.data(), bs.size(), ENET_PACKET_FLAG_UNSEQUENCED);

  enet_peer_send(peer, 1, packet);
}

MessageType get_packet_type(ENetPacket *packet)
{
  return (MessageType)*packet->data;
}

template<typename T>
using Skip = BitstreamReader::Skip<T>;

void deserialize_new_entity(ENetPacket *packet, Entity &ent)
{
  BitstreamReader bs(reinterpret_cast<char*>(packet->data), packet->dataLength);
  // bs.skip(sizeof(uint8_t));
  bs.read(Skip<MessageType>(), ent);
}

void deserialize_set_controlled_entity(ENetPacket *packet, uint16_t &eid)
{
  BitstreamReader bs(reinterpret_cast<char*>(packet->data), packet->dataLength);
  bs.read(Skip<MessageType>(), eid);
}

void deserialize_entity_state(ENetPacket *packet, uint16_t &eid, float &x, float &y)
{
  BitstreamReader bs(reinterpret_cast<char*>(packet->data), packet->dataLength);
  bs.read(Skip<MessageType>(), eid, x, y);
}

void deserialize_snapshot(ENetPacket *packet, uint16_t &eid, float &x, float &y)
{
  BitstreamReader bs(reinterpret_cast<char*>(packet->data), packet->dataLength);
  // bs.read(Skip<MessageType>(), eid, Skip(x), y);
  bs.read(Skip<MessageType>(), eid, x, y);
}