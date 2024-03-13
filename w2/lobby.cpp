#include <enet/enet.h>
#include <iostream>
#include <cstring>

class LobbyServer {
 public:

  LobbyServer(enet_uint32 host, enet_uint16 port) : m_isGameServerStarted(false) {
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

  ~LobbyServer() {
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

 private:

  void ProcessConnectEvent(const ENetEvent& event) {
    printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
    if (m_isGameServerStarted) {
      SendGameServerInfoToPeer(event.peer);
    }
  }

  void ProcessReceiveEvent(const ENetEvent& event) {
    printf("Packet received from %x:%u '%s'\n", event.peer->address.host, event.peer->address.port, event.packet->data);

    const char* message = reinterpret_cast<char*>(event.packet->data);
    if (!m_isGameServerStarted && strcmp(message, "start") == 0) {
      m_isGameServerStarted = true;
      SendGameServerInfoToAllPeers();
    }

    enet_packet_destroy(event.packet);
  }

  void SendGameServerInfoToPeer(ENetPeer* peer) {
    std::string message = "go " + m_gameServerAddress;
    ENetPacket *packet = enet_packet_create(message.c_str(), message.size() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
  }

  void SendGameServerInfoToAllPeers() {
    std::string message = "go " + m_gameServerAddress;
    ENetPacket *packet = enet_packet_create(message.c_str(), message.size() + 1, ENET_PACKET_FLAG_RELIABLE);
    for (size_t i = 0; i < m_server->connectedPeers; ++i) {
      enet_peer_send(&m_server->peers[i], 0, packet);
    }
  }

  ENetHost* m_server = nullptr;
  bool m_isGameServerStarted;
  std::string m_gameServerAddress = "localhost 10888";
};

int main(int argc, const char **argv)
{
  if (enet_initialize() != 0)
  {
    printf("Cannot init ENet");
    return 1;
  }
  
  constexpr int PORT = 10887;
  LobbyServer lobby(ENET_HOST_ANY, PORT);
  while (true) {
    lobby.ProcessMessages();
  }

  atexit(enet_deinitialize);
  return 0;
}

