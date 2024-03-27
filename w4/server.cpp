#include <enet/enet.h>
#include <iostream>
#include "entity.h"
#include "protocol.h"
#include "player.h"
#include <stdlib.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <cmath>
#include <format>

static std::vector<Entity> entities;
static std::unordered_map<uint16_t, Player> entityIdToPlayer;
static  const std::vector<std::string> random_names = 
        {"Aaren", "Aarika", "Abagael", "Abagail", "Abbe", "Abbey", "Abbi", "Abbie", "Abby", "Abbye", "Abigael", "Abigail",
        "Abigale", "Abra", "Ada", "Adah", "Adaline", "Adan", "Adara", "Adda", "Addi", "Addia", "Addie",
        "Addy", "Adel", "Adela", "Adelaida", "Adelaide", "Adele", "Adelheid", "Adelice", "Adelina"};
static u_int16_t nextAvailableRandomName = 0;
static uint16_t create_random_entity()
{
  uint16_t newEid = entities.size();
  uint32_t color = 0xff000000 +
                   0x00440000 * (1 + rand() % 4) +
                   0x00004400 * (1 + rand() % 4) +
                   0x00000044 * (1 + rand() % 4);
  float x = (rand() % 40 - 20) * 5.f;
  float y = (rand() % 40 - 20) * 5.f;
  float radius = (rand() % 6) + 5.f;
  Entity ent = {color, x, y, radius, newEid, false, 0.f, 0.f};
  entities.push_back(ent);
  return newEid;
}


void sendScoreListToAllPeers(ENetHost *server) {
  std::string scoreListText = "Scores:\n";
  for (const auto& [eid, player] : entityIdToPlayer) {
      scoreListText.append(std::format("{} {}\n", player.name, player.score));
  }
  for (size_t i = 0; i < server->connectedPeers; ++i)
  {
    ENetPeer *peer = &server->peers[i];
    send_score(peer, scoreListText);
  }
}

void on_join(ENetPacket *packet, ENetPeer *peer, ENetHost *host)
{
  // send all entities
  for (const Entity &ent : entities)
    send_new_entity(peer, ent);

  // find max eid
  uint16_t newEid = create_random_entity();
  const Entity& ent = entities[newEid];

  std::string name;
  deserialize_join(packet, name);
  if (name == "-none") {
    name = random_names[nextAvailableRandomName];
    nextAvailableRandomName = (nextAvailableRandomName + 1) % random_names.size();
  }
  float score = 0.f;
  entityIdToPlayer[newEid] = {peer, name, score};
  sendScoreListToAllPeers(host);

  // send info about new entity to everyone
  for (size_t i = 0; i < host->connectedPeers; ++i)
    send_new_entity(&host->peers[i], ent);
  // send info about controlled entity
  send_set_controlled_entity(peer, newEid);
}

void on_state(ENetPacket *packet)
{
  uint16_t eid = invalid_entity;
  float x = 0.f; float y = 0.f;
  deserialize_entity_state(packet, eid, x, y);
  for (Entity &e : entities)
    if (e.eid == eid)
    {
      e.x = x;
      e.y = y;
    }
}

bool isEntitiesCollide(const Entity& e1, const Entity& e2) {
  float distX = e1.x - e2.x;
  float distY = e1.y - e2.y;
  float sumRadii = e1.radius + e2.radius;
  return distX * distX + distY * distY < sumRadii * sumRadii;  
}


void simulateEat(Entity& whoEat, Entity& whoGetsEaten, ENetHost *server) {
  float radiusToAdd = whoGetsEaten.radius / 2.f;
  whoEat.radius += whoEat.radius < 100.f ? radiusToAdd : 100.f - whoEat.radius;
  whoGetsEaten.radius = (rand() % 6) + 5.f;
  whoGetsEaten.x = (rand() % 120 - 60) * 5.f;
  whoGetsEaten.y = (rand() % 120 - 60) * 5.f;
  for (size_t i = 0; i < server->connectedPeers; ++i)
  {
    ENetPeer *peer = &server->peers[i];
    send_change_size(peer, whoEat.eid, whoEat.radius);
    send_change_size(peer, whoGetsEaten.eid, whoGetsEaten.radius);
  }
  if (entityIdToPlayer.contains(whoGetsEaten.eid)) {
    send_teleport(entityIdToPlayer[whoGetsEaten.eid].peer, whoGetsEaten.eid, whoGetsEaten.x, whoGetsEaten.y);
  }
  if (entityIdToPlayer.contains(whoEat.eid)) {
    entityIdToPlayer[whoEat.eid].score += radiusToAdd;
    sendScoreListToAllPeers(server);
  }
}

void simulateTryEat(Entity& e1, Entity& e2, ENetHost *server) {
  if (isEntitiesCollide(e1, e2)) {
    if (e1.radius > e2.radius) {
      simulateEat(e1, e2, server);
    } else if (e1.radius < e2.radius) {
      simulateEat(e2, e1, server);
    }
    // no one will be eaten
  }
  // no one will be eaten
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

  constexpr int numAi = 10;

  for (int i = 0; i < numAi; ++i)
  {
    uint16_t eid = create_random_entity();
    entities[eid].serverControlled = true;
  }

  uint32_t lastTime = enet_time_get();
  while (true)
  {
    uint32_t curTime = enet_time_get();
    float dt = (curTime - lastTime) * 0.001f;
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
            on_join(event.packet, event.peer, server);
            break;
          case E_CLIENT_TO_SERVER_STATE:
            on_state(event.packet);
            break;
        };
        enet_packet_destroy(event.packet);
        break;
      default:
        break;
      };
    }
    for (Entity &e : entities)
    {
      if (e.serverControlled)
      {
        const float diffX = e.targetX - e.x;
        const float diffY = e.targetY - e.y;
        const float dirX = diffX > 0.f ? 1.f : -1.f;
        const float dirY = diffY > 0.f ? 1.f : -1.f;
        constexpr float spd = 50.f;
        e.x += dirX * spd * dt;
        e.y += dirY * spd * dt;
        if (fabsf(diffX) < 10.f && fabsf(diffY) < 10.f)
        {
          e.targetX = (rand() % 40 - 20) * 15.f;
          e.targetY = (rand() % 40 - 20) * 15.f;
        }
      }
    }
    for (size_t i = 0; i < entities.size(); ++i) {
      for (size_t j = i + 1; j < entities.size(); ++j) {
        simulateTryEat(entities[i], entities[j], server);
      }
    }
    for (const Entity &e : entities)
    {
      if (entityIdToPlayer.contains(e.eid)) {
        for (size_t i = 0; i < server->connectedPeers; ++i)
        {
          ENetPeer *peer = &server->peers[i];
          if (entityIdToPlayer[e.eid].peer != peer)
            send_snapshot(peer, e.eid, e.x, e.y);
        }
      } else {
        for (size_t i = 0; i < server->connectedPeers; ++i)
        {
          ENetPeer *peer = &server->peers[i];
          send_snapshot(peer, e.eid, e.x, e.y);
        }
      }
    }
    //usleep(400000);
  }

  enet_host_destroy(server);
  atexit(enet_deinitialize);
  return 0;
}