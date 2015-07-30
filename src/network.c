#include "obbg_funcs.h"
#include <math.h>

#include <SDL_net.h>

#if 0
typedef struct
{
   uint8 seq;
} packet_header;

typedef struct
{
   uint8 type;
   uint8 count;
} record_header;
#endif

#define SERVER_PORT 4127

static UDPsocket receive_socket;
static UDPpacket *receive_packet;
static UDPpacket *send_packet;

#define MAX_SAFE_PACKET_SIZE  512

typedef struct
{
   address addr;
   objid pid;
} connection_t;

Bool net_send(void *buffer, size_t buffer_size, address *addr)
{
   int res;

   assert(buffer_size <= (size_t) send_packet->maxlen);
   if (buffer_size > (size_t) send_packet->maxlen)
      return False;

   SDLNet_Write32(addr->host, &send_packet->address.host);
   SDLNet_Write16(addr->port, &send_packet->address.port);

   memcpy(send_packet->data, buffer, buffer_size);
   send_packet->len = buffer_size;

   res = SDLNet_UDP_Send(receive_socket, -1, send_packet);

   return True;
}

int net_receive(void *buffer, size_t buffer_size, address *addr)
{
   int result = SDLNet_UDP_Recv(receive_socket, receive_packet);
   if (result == 0) {
      return -1;
   }
   if (result == -1) {
      // @TODO ??? don't know what the error case is
      return -1;
   }
   if (result == 1) {
      addr->host = SDLNet_Read32(&receive_packet->address.host);
      addr->port = SDLNet_Read16(&receive_packet->address.port);
      memcpy(buffer, receive_packet->data, stb_min(buffer_size, (size_t) receive_packet->len));
      return receive_packet->len;
   }
   return -1;
}

address server_address;

Bool net_init(int port)
{
   if (SDLNet_Init() != 0)
      return False;

   receive_socket = SDLNet_UDP_Open(is_server ? SERVER_PORT : port);
   receive_packet = SDLNet_AllocPacket(MAX_SAFE_PACKET_SIZE);
   send_packet    = SDLNet_AllocPacket(MAX_SAFE_PACKET_SIZE);

   server_address.host = (127<<24) + 1;
   server_address.port = SERVER_PORT;
   return True;
}

#define MAX_CONNECTIONS 32

connection_t connection[MAX_CONNECTIONS];
int connection_for_pid[PLAYER_OBJECT_MAX];

int create_connection(address *addr)
{
   int i;
   for (i=0; i < MAX_CONNECTIONS; ++i)
      if (connection[i].addr.port == 0) {
         objid pid;
         connection[i].addr = *addr;
         connection[i].pid = pid = allocate_player();
         connection_for_pid[connection[i].pid] = i;
         obj[pid].position.x = (float) (rand() & 31);
         obj[pid].position.y = (float) (rand() & 31);
         obj[pid].position.z = 100;
         return i;
      }
   return -1;
}

int find_connection(address *addr)
{
   int i;
   for (i=0; i < MAX_CONNECTIONS; ++i)
      if (connection[i].addr.host == addr->host && connection[i].addr.port == addr->port)
         return i;
   return -1;
}

float angle_from_network(uint16 iang)
{
   return iang / 65536.0f * 360;
}

uint16 angle_to_network(float ang)
{
   int iang = (int) (ang/360 * 65536);
   if (iang >= 65536) iang = 65535;
   return iang;
}

void server_net_tick(void)
{
   int i;
   net_client_input input;

   address addr;
   while (net_receive(&input, sizeof(input), &addr) >= 0) {
      int n = find_connection(&addr);
      if (n < 0) {
         n = create_connection(&addr);
         if (n < 0)
            continue;
      }
      p_input[connection[n].pid].buttons = input.last_inputs[0].buttons;
      obj[connection[n].pid].ang.x = angle_from_network(input.last_inputs[0].view_x) - 90;
      obj[connection[n].pid].ang.z = angle_from_network(input.last_inputs[0].view_z);
   }

   for (i=1; i < max_player_id; ++i) {
      if (obj[i].valid) {
         int conn = connection_for_pid[i];
         obj[0].valid = i;
         net_send(obj, sizeof(obj[0])*8, &connection[conn].addr);
      }
   }
}

static net_client_input input;
void client_net_tick(void)
{
   int i;
   vec ang;
   address receive_addr;

   ang = obj[player_id].ang;

   ang.x += 90;
   ang.x = (float) fmod(ang.x, 360);
   if (ang.x < 0) ang.x += 360;
   ang.z = (float) fmod(ang.z, 360);
   if (ang.z < 0) ang.z += 360;

   input.sequence += 1;
   input.type = 0;
   memmove(&input.last_inputs[1], &input.last_inputs[0], sizeof(input.last_inputs) - sizeof(input.last_inputs[0]));

   input.last_inputs[0].buttons = client_player_input.buttons;
   input.last_inputs[0].view_x = angle_to_network(ang.x);
   input.last_inputs[0].view_z = angle_to_network(ang.z);

   ang.x -= 90;   
   obj[player_id].ang = ang;

   net_send(&input, sizeof(input), &server_address);

   while (net_receive(obj, sizeof(obj[0])*8, &receive_addr) >= 0) {
      // @TODO: veryify receive_addr == server_address
      obj[player_id].ang = ang;
      player_id = obj[0].valid;
   }
   for (i=1; i < 8; ++i)
      if (obj[i].valid && i >= max_player_id)
         max_player_id = i+1;
}
