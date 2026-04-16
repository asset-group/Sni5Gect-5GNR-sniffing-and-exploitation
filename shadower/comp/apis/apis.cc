#include "shadower/comp/apis/apis.h"
#include "srsran/support/srsran_assert.h"
#include <string.h>
#include <sys/stat.h>
#include <zmq.h>

bool APIs::init(const char* zmq_address)
{
  if (initialized) {
    return true;
  }
  context  = zmq_ctx_new();
  z_socket = zmq_socket(context, ZMQ_PUB);
  int rc   = zmq_bind(z_socket, zmq_address);
  if (rc != 0) {
    logger.error("Error binding to ZMQ socket");
    return false;
  }
  logger.info("ZMQ socket bound to \"%s\"", zmq_address);
  if (strstr(zmq_address, "ipc://") != NULL) {
    chmod(zmq_address + 6, 0777);
  }
  initialized = true;
  return true;
}

int APIs::send_uplink_api_message(uplink_api_hdr_t& msg, cf_t* buffer, cf_t* last_buffer)
{
  if (srsran_unlikely(!z_socket)) {
    logger.error("ZMQ socket not initialized");
    return -1;
  }
  zmq_send(z_socket, &msg, sizeof(msg), ZMQ_SNDMORE);
  zmq_send(z_socket, last_buffer, msg.sf_len * sizeof(cf_t), ZMQ_SNDMORE);
  zmq_send(z_socket, buffer, msg.sf_len * sizeof(cf_t), 0);
  return 0;
}

APIs::~APIs()
{
  if (z_socket) {
    zmq_close(z_socket);
    z_socket = nullptr;
  }
  if (context) {
    zmq_ctx_destroy(context);
    context = nullptr;
  }
}