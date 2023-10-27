#ifndef DARC_RPC_MSG_SERIALIZER_HPP__
#define DARC_RPC_MSG_SERIALIZER_HPP__

#include <cstddef>
#include <cstdint>

namespace dc {

class msg_serializer {
 public:
  virtual void serialize(uint8_t* buffer, size_t* buffer_size) = 0;
  virtual bool deserialize(uint8_t* buffer, const size_t buffer_size) = 0;
};

};  // namespace dc

#endif  // DARC_RPC_MSG_SERIALIZER_HPP__
