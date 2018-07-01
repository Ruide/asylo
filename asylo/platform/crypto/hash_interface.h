/*
 *
 * Copyright 2017 Asylo authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef ASYLO_PLATFORM_CRYPTO_HASH_INTERFACE_H_
#define ASYLO_PLATFORM_CRYPTO_HASH_INTERFACE_H_

#include <cstdint>
#include <cstdlib>
#include <string>

#include "asylo/crypto/algorithms.pb.h"  // IWYU pragma: export

namespace asylo {

// HashInterface defines an interface for hash functions.
//
// Data may be added to an instance of HashInterface via the Update() method at
// any point during the object's lifetime. A user may call the CumulativeHash()
// method to get a hash of all data added to the object since its creation or
// last call to its Init() method.
//
// Implementations of this interface need not be thread-safe.
class HashInterface {
 public:
  HashInterface(const HashInterface &) = delete;
  HashInterface &operator=(const HashInterface &) = delete;
  HashInterface() = default;
  virtual ~HashInterface() = default;

  // Returns the hash algorithm implemented by this object.
  virtual HashAlgorithm Algorithm() const = 0;

  // Returns the size of the message-digest size of this hash algorithm. A
  // return value of zero implies that the object does not implement a
  // fixed-size hash function.
  virtual size_t DigestSize() const = 0;

  // Initializes this hash object to a clean state. Calling this method clears
  // the effects of all previous Update() operations. Note that a newly
  // constructed hash object is always expected to be in a clean state and users
  // are not required to call Init() on such objects.
  virtual void Init() = 0;

  // Updates this hash object by adding |len| bytes from |data|.
  virtual void Update(const void *data, size_t len) = 0;

  // Returns a string containing the current hash. Note that the internal state
  // of the object remains unchanged, and the object can continue to accumulate
  // additional data via Update() operations.
  virtual std::string CumulativeHash() const = 0;
};

}  // namespace asylo

#endif  // ASYLO_PLATFORM_CRYPTO_HASH_INTERFACE_H_
