#define NBNET_IMPL

#include "rnet.h"

#if PLATFORM==PLATFORM_DESKTOP

#include "../net_drivers/udp.h"

#elif PLATFORM==PLATFORM_WEB

#include "../net_drivers/webrtc.h"

#endif

static NBN_OutgoingMessage *CreateOutgoingMessage(uint8_t *bytes, unsigned int length);
static Message received_message;

/*******************************/
/*          Client API         */
/*******************************/

void StartClient(const char *protocol_name, const char *ip_address, uint16_t port)
{
    NBN_GameClient_Init(protocol_name, ip_address, port);

    if (NBN_GameClient_Start() == NBN_ERROR)
    {
        TraceLog(LOG_ERROR, "Failed to start client");

        RNetAbort();
    }
}

void StopClient(void)
{
    NBN_GameClient_Stop();
    NBN_GameClient_Deinit();
}

void SendUnreliableMessage(uint8_t *bytes, unsigned int length)
{
    if (NBN_GameClient_SendUnreliableMessage(CreateOutgoingMessage(bytes, length)) == NBN_ERROR)
    {
        TraceLog(LOG_ERROR, "Failed to send unreliable message to server");

        RNetAbort();
    }
}

void SendReliableMessage(uint8_t *bytes, unsigned int length)
{
    if (NBN_GameClient_SendReliableMessage(CreateOutgoingMessage(bytes, length)) == NBN_ERROR)
    {
        TraceLog(LOG_ERROR, "Failed to send reliable message to server");

        RNetAbort();
    }
}

void FlushClient(void)
{
    if (NBN_GameClient_SendPackets() == NBN_ERROR)
    {
        TraceLog(LOG_ERROR, "Failed to flush client");

        RNetAbort();
    }
}

ClientEvent PollClient(void)
{
    NBN_GameClient_AddTime(GetTime());

    int ev = NBN_GameClient_Poll();

    if (ev == NBN_ERROR)
    {
        TraceLog(LOG_ERROR, "An error occured while polling client network events");

        RNetAbort();
    }

    if (ev == NBN_NO_EVENT)
        return NO_EVENT;

    if (ev == NBN_CONNECTED)
        return CONNECTED;

    if (ev == NBN_DISCONNECTED)
        return DISCONNECTED;

    if (ev == NBN_MESSAGE_RECEIVED)
    {
        NBN_MessageInfo msg_info = NBN_GameClient_GetMessageInfo();

        RNetAssert(msg_info.type == NBN_BYTE_ARRAY_MESSAGE_TYPE);

        NBN_ByteArrayMessage *msg = msg_info.data;

        received_message = (Message){.sender = NULL, .bytes = msg->bytes, .length = msg->length};

        return MESSAGE_RECEIVED;
    }

    TraceLog(LOG_ERROR, "Unsupported client network event");

    RNetAbort();

    return NBN_ERROR;
}

/*******************************/
/*          Server API         */
/*******************************/

void StartServer(const char *protocol_name, uint16_t port)
{
    NBN_GameServer_Init(protocol_name, port);

    if (NBN_GameServer_Start() == NBN_ERROR)
    {
        TraceLog(LOG_ERROR, "Failed to start server");

        RNetAbort();
    }
}

void StopServer(void)
{
    NBN_GameServer_Stop();
    NBN_GameServer_Deinit();
}

void SendUnreliableMessageTo(uint8_t *bytes, unsigned int length, Connection *client)
{
    if (NBN_GameServer_SendUnreliableMessageTo(client, CreateOutgoingMessage(bytes, length)) == NBN_ERROR)
    {
        TraceLog(LOG_ERROR, "Failed to send unreliable message to server");

        RNetAbort();
    }
}

void SendReliableMessageTo(uint8_t *bytes, unsigned int length, Connection *client)
{
    if (NBN_GameServer_SendReliableMessageTo(client, CreateOutgoingMessage(bytes, length)) == NBN_ERROR)
    {
        TraceLog(LOG_ERROR, "Failed to send unreliable message to server");

        RNetAbort();
    }
}

void BroadcastUnreliableMessage(uint8_t *bytes, unsigned int length)
{
    NBN_OutgoingMessage *outgoing_msg = CreateOutgoingMessage(bytes, length);

    if (NBN_GameServer_BroadcastUnreliableMessage(outgoing_msg) == NBN_ERROR)
    {
        TraceLog(LOG_ERROR, "Failed to broadcast unreliable message to server");

        RNetAbort();
    }
}

void BroadcastReliableMessage(uint8_t *bytes, unsigned int length)
{
    NBN_OutgoingMessage *outgoing_msg = CreateOutgoingMessage(bytes, length);

    if (NBN_GameServer_BroadcastReliableMessage(outgoing_msg) == NBN_ERROR)
    {
        TraceLog(LOG_ERROR, "Failed to broadcast unreliable message to server");

        RNetAbort();
    }
}

void FlushServer(void)
{
    if (NBN_GameServer_SendPackets() == NBN_ERROR)
    {
        TraceLog(LOG_ERROR, "Failed to flush server");

        RNetAbort();
    }
}

ServerEvent PollServer(void)
{
    NBN_GameServer_AddTime(GetTime());

    int ev = NBN_GameServer_Poll();

    if (ev == NBN_ERROR)
    {
        TraceLog(LOG_ERROR, "An error occured while polling server network events");

        RNetAbort();
    }

    if (ev == NBN_NO_EVENT)
        return NO_EVENT;

    if (ev == NBN_NEW_CONNECTION)
        return CLIENT_CONNECTION_REQUEST;

    if (ev == NBN_CLIENT_DISCONNECTED)
        return CLIENT_DISCONNECTED;

    if (ev == NBN_CLIENT_MESSAGE_RECEIVED)
    {
        NBN_MessageInfo msg_info = NBN_GameServer_GetMessageInfo();

        RNetAssert(msg_info.type == NBN_BYTE_ARRAY_MESSAGE_TYPE);

        NBN_ByteArrayMessage *msg = msg_info.data;

        received_message = (Message){.sender = msg_info.sender, .bytes = msg->bytes, .length = msg->length};

        return CLIENT_MESSAGE_RECEIVED;
    }

    TraceLog(LOG_ERROR, "Unsupported server network event");

    RNetAbort();

    return NBN_ERROR;
}

Connection *AcceptClient(void)
{
    if (NBN_GameServer_AcceptIncomingConnection(NULL) == NBN_ERROR)
    {
        TraceLog(LOG_ERROR, "Failed to accept client");

        RNetAbort();
    }

    return NBN_GameServer_GetIncomingConnection();
}

void RejectClient(void)
{
    if (NBN_GameServer_RejectIncomingConnection() == NBN_ERROR)
    {
        TraceLog(LOG_ERROR, "Failed to reject client");

        RNetAbort();
    }
}

Connection *GetDisconnectedClient(void)
{
    return NBN_GameServer_GetDisconnectedClient();
}

/*******************************/
/*          Common API         */
/*******************************/

Message *GetReceivedMessage(void)
{
    return &received_message;
}

/*******************************/
/*          Private API        */
/*******************************/

static NBN_OutgoingMessage *CreateOutgoingMessage(uint8_t *bytes, unsigned int length)
{
    if (length > NBN_BYTE_ARRAY_MAX_SIZE)
    {
        TraceLog(LOG_ERROR,
                 "Cannot create a message bigger than %d bytes"
                 " (increase it by setting NBN_BYTE_ARRAY_MAX_SIZE)",
                 NBN_BYTE_ARRAY_MAX_SIZE);

        RNetAbort();
    }

    NBN_OutgoingMessage *outgoing_msg = NBN_GameClient_CreateByteArrayMessage(bytes, length);

    if (!outgoing_msg)
    {
        TraceLog(LOG_ERROR, "Failed to create unreliable message");

        RNetAbort();
    }

    return outgoing_msg;
}