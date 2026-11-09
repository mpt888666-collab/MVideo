//
// Asynchronous release of Redis login state.
//

#ifndef CHATSERVER1_REDISCLEANUP_H
#define CHATSERVER1_REDISCLEANUP_H

#include <string>

namespace redis_cleanup {

bool QueueRelease(std::string token);
void ResetServerState();

}  // namespace redis_cleanup

#endif  // CHATSERVER1_REDISCLEANUP_H
