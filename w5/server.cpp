#include <enet/enet.h>
#include <iostream>
#include "entity.h"
#include "protocol.h"
#include "mathUtils.h"
#include <stdlib.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <queue>

#include "params.h"
static std::vector<Entity> entities;
static std::vector<Entity> entitiesInputs;
static std::map<uint16_t, ENetPeer*> controlledMap;
static std::unordered_map<uint16_t, size_t> eidToIndexInVectorMap;
static std::vector<std::queue<Entity>> snapshotsHistory;

void on_join(ENetPacket *packet, ENetPeer *peer, ENetHost *host, uint32_t time)
{
  uint32_t tick = static_cast<float>(time) / fixedDt;
  // send all entities
  for (const Entity &ent : entities)
    send_new_entity(peer, ent);

  // find max eid
  uint16_t maxEid = entities.empty() ? invalid_entity : entities[0].eid;
  for (const Entity &e : entities)
    maxEid = std::max(maxEid, e.eid);
  uint16_t newEid = maxEid + 1;
  uint32_t color = 0xff000000 +
                   0x00440000 * (rand() % 5 + 1) +
                  //  0x00440000 * (rand() % 5 + 1) +
                   0x00000044 * (rand() % 5 + 1);
  float x = (rand() % 4) * 5.f;
  float y = (rand() % 4) * 5.f;
  Entity ent = {color, x, y, 0.f, (rand() / RAND_MAX) * 3.141592654f, 0.f, 0.f, tick, newEid};
  eidToIndexInVectorMap[ent.eid] = entities.size();
  entities.push_back(ent);
  entitiesInputs.push_back(ent);
  controlledMap[newEid] = peer;
  std::queue<Entity> newQueue; newQueue.push(ent);
  snapshotsHistory.push_back(newQueue);

  // send info about new entity to everyone
  for (size_t i = 0; i < host->connectedPeers; ++i)
    send_new_entity(&host->peers[i], ent);
  // send info about controlled entity
  send_set_controlled_entity(peer, newEid);
  time = enet_time_get();
  send_set_time(peer, time);
}

void simulate_entity_fixed(Entity &e, uint32_t simulateTime) 
{
  auto &snapshotsQueue = snapshotsHistory[eidToIndexInVectorMap[e.eid]];
  Entity &ei = entitiesInputs[eidToIndexInVectorMap[e.eid]];
  while (e.tick * fixedDt <= simulateTime) 
  {
    ++e.tick;
    while (!snapshotsQueue.empty() && snapshotsQueue.front().tick <= e.tick) {
      ei = snapshotsQueue.front();
      snapshotsQueue.pop();
    }
    if (e.tick >= ei.tick) {
      e.thr = ei.thr; e.steer = ei.steer;
    }
    simulate_entity(e, fixedDt * 0.001f);
  }
}

void on_input(const ENetEvent& event)
{
  ENetPacket *packet = event.packet;
  uint16_t eid = invalid_entity;
  float thr = 0.f; float steer = 0.f;
  uint32_t tick;
  deserialize_entity_input(packet, eid, thr, steer, tick);
  Entity ei;
  uint32_t t = enet_time_get();
  ei.tick = tick; //(t - event.peer->roundTripTime / 2 + FIXED_OFFSET + 1) / fixedDt + 1;
  // if (ei.thr != thr || ei.steer != steer) {
  //   std::cout << (t - event.peer->roundTripTime / 2 + FIXED_OFFSET + 1) << ' ' << tick  << ' ' << ei.tick << ' ' << tick / fixedDt + 1 << std::endl;
  // }
  ei.thr = thr;
  ei.steer = steer;
  snapshotsHistory[eidToIndexInVectorMap[eid]].push(ei);
  simulate_entity_fixed(entities[eidToIndexInVectorMap[eid]], t); // вроде это помогает сделать более точную обработку, может все уже починилось и уже не надо
}


int main(int argc, const char **argv)
{
  if (enet_initialize() != 0)
  {
    printf("Cannot init ENet");
    return 1;
  }
  ENetAddress address;

  address.host = ENET_HOST_ANY;
  address.port = 10131;

  ENetHost *server = enet_host_create(&address, 32, 2, 0, 0);

  if (!server)
  {
    printf("Cannot create ENet server\n");
    return 1;
  }

  enet_time_set(0);
  uint32_t lastTime = enet_time_get();
  uint32_t lastTimeSendSnapshots = enet_time_get();
  while (true)
  {
    uint32_t curTime = enet_time_get();
    lastTime = curTime;
    ENetEvent event;
    while (enet_host_service(server, &event, 1) > 0)
    {
      switch (event.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
        printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        switch (get_packet_type(event.packet))
        {
          case E_CLIENT_TO_SERVER_JOIN:
            on_join(event.packet, event.peer, server, curTime);
            break;
          case E_CLIENT_TO_SERVER_INPUT:
            on_input(event);
            break;
        };
        enet_packet_destroy(event.packet);
        break;
      default:
        break;
      };
    }

    curTime = enet_time_get();
    for (Entity &e : entities)
    {
      simulate_entity_fixed(e, curTime);
    }

    if (curTime - lastTimeSendSnapshots >= SEND_TIMEOUT) {
      for (Entity &e : entities)
      {
        // send
        for (size_t i = 0; i < server->connectedPeers; ++i)
        {
          ENetPeer *peer = &server->peers[i];
          // skip this here in this implementation
          //if (controlledMap[e.eid] != peer)
          send_snapshot(peer, e);
        }
      }
      lastTimeSendSnapshots = curTime;
    }
    // usleep(100000);
  }

  enet_host_destroy(server);

  atexit(enet_deinitialize);
  return 0;
}


