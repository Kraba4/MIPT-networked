#include <enet/enet.h>
#include <iostream>
#include <cstring>
#include <vector>
#include <format>
#include <unordered_map>

class GameServer {
 public:

  GameServer(enet_uint32 host, enet_uint16 port) {
    ENetAddress address;
    address.host = host;
    address.port = port;
    m_server = enet_host_create(&address, 32, 2, 0, 0);
    if (!m_server)
    {
      printf("Cannot create ENet server\n");
      exit(1);
    }
  }

  ~GameServer() {
    enet_host_destroy(m_server);
  }

  void ProcessMessages() {
      ENetEvent event;
      while (enet_host_service(m_server, &event, 10) > 0)
      {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
          ProcessConnectEvent(event);
          break;
        case ENET_EVENT_TYPE_RECEIVE:
          ProcessReceiveEvent(event);
          break;
        default:
          break;
        };
      }
  }

  bool hasConnection() {
    return m_isConnected;
  }

  void SendPingLists() {
    std::string message = "List of players:\n";
    for (size_t i = 0; i < m_server->connectedPeers; ++i) {
        uint32_t playerId = m_peerIdToIdMap[m_server->peers[i].connectID];
        message.append(std::format("{} {}ms\n", m_names[playerId % m_names.size()], m_server->peers[i].roundTripTime));
    }
    ENetPacket *packet = enet_packet_create(message.c_str(), message.size() + 1, ENET_PACKET_FLAG_UNSEQUENCED);
    for (size_t i = 0; i < m_server->connectedPeers; ++i) {
        enet_peer_send(&m_server->peers[i], 1, packet);
    }
  }

 private:

  void ProcessConnectEvent(const ENetEvent& event) {
    printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
    m_isConnected = true;
    CreateNewPlayer(event);
  }

  void ProcessReceiveEvent(const ENetEvent& event) {
    printf("Packet received from %x:%u '%s'\n", event.peer->address.host, event.peer->address.port, event.packet->data);

    const char* message = reinterpret_cast<char*>(event.packet->data);

    enet_packet_destroy(event.packet);
  }

  void CreateNewPlayer(const ENetEvent& event) {
    uint32_t playerId = m_uniquePlayersCount++;
    std::string playerName = m_names[playerId % m_names.size()];
    m_peerIdToIdMap[event.peer->connectID] = playerId;
    SendNewPlayerInfoToNewPlayer    (event.peer, playerId, playerName);
    SendNewPlayerInfoToAllOtherPeers(event.peer, playerId, playerName);
    SendListOfOtherPlayers          (event.peer);
  }

  void SendNewPlayerInfoToNewPlayer(ENetPeer* newPlayerPeer, uint32_t newPlayerId, const std::string& newPlayerName) {
    std::string message = std::format("you {} {}", newPlayerId, newPlayerName);
    ENetPacket *packet = enet_packet_create(message.c_str(), message.size() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(newPlayerPeer, 0, packet);
  }

  void SendNewPlayerInfoToAllOtherPeers(ENetPeer* newPlayerPeer, uint32_t newPlayerId, const std::string& newPlayerName) {
    std::string message = std::format("new {} {}", newPlayerId, newPlayerName);
    ENetPacket *packet = enet_packet_create(message.c_str(), message.size() + 1, ENET_PACKET_FLAG_RELIABLE);
    for (size_t i = 0; i < m_server->connectedPeers; ++i) {
        if(m_server->peers[i].connectID != newPlayerPeer->connectID) {
            enet_peer_send(&m_server->peers[i], 0, packet);
        }
    }
  }

  void SendListOfOtherPlayers(ENetPeer* newPlayerPeer) {
    std::string message = "players";
    for (size_t i = 0; i < m_server->connectedPeers; ++i) {
        if(m_server->peers[i].connectID != newPlayerPeer->connectID) {
            uint32_t playerId = m_peerIdToIdMap[m_server->peers[i].connectID];
            message.append(std::format(" {} {} ", std::to_string(playerId), m_names[playerId % m_names.size()]));
        }
    }
    ENetPacket *packet = enet_packet_create(message.c_str(), message.size() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(newPlayerPeer, 0, packet);
  }

  ENetHost* m_server = nullptr;
  uint32_t m_uniquePlayersCount = 0;
  bool m_isConnected = false;
  std::unordered_map<enet_uint32, uint32_t> m_peerIdToIdMap;
  const std::vector<std::string> m_names = 
        {"Aaren", "Aarika", "Abagael", "Abagail", "Abbe", "Abbey", "Abbi", "Abbie", "Abby", "Abbye", "Abigael", "Abigail",
        "Abigale", "Abra", "Ada", "Adah", "Adaline", "Adan", "Adara", "Adda", "Addi", "Addia", "Addie",
        "Addy", "Adel", "Adela", "Adelaida", "Adelaide", "Adele", "Adelheid", "Adelice", "Adelina"};
};

int main(int argc, const char **argv)
{
  if (enet_initialize() != 0)
  {
    printf("Cannot init ENet");
    return 1;
  }

  constexpr int PORT = 10888;
  GameServer gameServer(ENET_HOST_ANY, PORT);
  uint32_t timeStart = enet_time_get();
  uint32_t lastSendTime = timeStart;

  while (true) {
    gameServer.ProcessMessages();

    if (gameServer.hasConnection())
    {
        uint32_t curTime = enet_time_get();
        if (curTime - lastSendTime > 1000)
        {
            lastSendTime = curTime;
            gameServer.SendPingLists();
        }
    }
  }

  atexit(enet_deinitialize);
  return 0;
}

