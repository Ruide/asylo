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

#ifndef ASYLO_UTIL_STATUS_H_
#define ASYLO_UTIL_STATUS_H_

#include <ostream>
#include <string>
#include <type_traits>

#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "asylo/util/logging.h"
#include "asylo/util/error_space.h"  // IWYU: pragma export
#include "asylo/util/status.pb.h"
#include "asylo/util/status_error_space.h"
#include "asylo/util/status_internal.h"

namespace asylo {

/// Status contains information about an error. Status contains an error code
/// from some error space and a message string suitable for logging or
/// debugging.
class Status {
 public:
  /// Builds an OK Status in the canonical error space.
  Status();

  /// Constructs a Status object containing an error code and message.
  ///
  /// \param space The ErrorSpace this code belongs to.
  /// \param code An integer error code.
  /// \param message The associated error message.
  Status(const error::ErrorSpace *space, int code, absl::string_view message);

  /// Constructs a Status object containing an error code and a message. The
  /// error space is deduced from `code`.
  ///
  /// \param code A symbolic error code.
  /// \param message The associated error message.
  template <typename Enum>
  Status(Enum code, absl::string_view message) {
    Set(code, message);
  }

  Status(const Status &other) = default;

  // Non-default move constructor since the moved status should be set to
  // indicate an invalid state, which changes the code and error_space.
  Status(Status &&other);

  /// Constructs a Status object from `StatusT`. `StatusT` must be a status-type
  /// object. I.e.,
  ///
  ///   * It must have a two-parameter constructor that takes an enum as its
  ///     first parameter and a string as its second parameter.
  ///   * It must have non-static error_code(), error_message(), and ok()
  ///     methods.
  ///
  /// This constructor is provided for the convenience of Asylo-SDK consumers
  /// utilizing other status types such as `::grpc::Status`.
  ///
  /// \param other A status-like object to copy.
  template <typename StatusT,
            typename E = typename absl::enable_if_t<
                status_internal::status_type_traits<StatusT>::is_status>>
  explicit Status(const StatusT &other) {
    Set(status_internal::status_type_traits<StatusT>::CanonicalCode(other),
        other.error_message());
  }

  Status &operator=(const Status &other) = default;

  // Non-default move assignment operator since the moved status should be set
  // to indicate an invalid state, which changes the code and error_space.
  Status &operator=(Status &&other);

  /// Constructs an OK status object.
  ///
  /// \return A Status indicating no error occurred.
  static Status OkStatus();

  /// Copy this object to a status type `StatusT`. The method first converts the
  /// ::asylo::Status object to its canonical form, and then constructs a
  /// `StatusT` from the error code and message fields of the converted object.
  /// `StatusT` must be a status-type object. I.e.,
  ///
  ///   * It must have a two-parameter constructor that takes an enum as its
  ///     first parameter and a string as its second parameter.
  ///   * It must have non-static error_code(), error_message(), and ok()
  ///     methods.
  ///
  /// This operator is provided for the convenience of the Asylo SDK users
  /// that utilize other status types, such as `::grpc::Status`.
  ///
  /// \return A status-like object copied from this object.
  template <typename StatusT,
            typename E = typename absl::enable_if_t<
                status_internal::status_type_traits<StatusT>::is_status>>
  StatusT ToOtherStatus() {
    Status status = ToCanonical();
    return StatusT(status_internal::ErrorCodeHolder(status.error_code_),
                   status.message_);
  }

  /// Gets the integer error code for this object.
  ///
  /// \return The associated integer error code.
  int error_code() const;

  /// Gets the string error message for this object.
  ///
  /// \return The associated error message.
  absl::string_view error_message() const;

  /// Gets the string error message for this object.
  ///
  /// \return The associated error space.
  const error::ErrorSpace *error_space() const;

  /// Indicates whether this object is OK (indicates no error).
  ///
  /// \return True if this object indicates no error.
  bool ok() const;

  /// Gets a string representation of this object.
  ///
  /// Gets a string containing the error space name, error code name, and error
  /// message if this object is a non-OK Status, or just a string containing the
  /// error code name if this object is an OK Status.
  ///
  /// \return A string representation of this object.
  std::string ToString() const;

  /// Gets a copy of this object in the canonical error space.
  ///
  /// This operation has no effect if the Status object is already in the
  /// canonical error space. Otherwise, this translation involves the following:
  ///
  ///   * Error code is converted to the equivalent error code in the canonical
  ///     error space.
  ///   * The new error message is set to the `ToString()` representation of the
  ///     old Status object in order to preserve the previous error code
  ///     information.
  Status ToCanonical() const;

  /// Gets the canonical error code for this object's error code.
  ///
  /// \return A canonical `error::GoogleError` code.
  error::GoogleError CanonicalCode() const;

  /// Exports the contents of this object into `status_proto`. This method sets
  /// all fields in `status_proto`.
  ///
  /// \param[out] status_proto A protobuf object to populate.
  void SaveTo(StatusProto *status_proto) const;

  /// Populates this object using the contents of the given `status_proto`.
  ///
  /// If the error space given by `status_proto.space()` is unrecognized, sets
  /// the error space to the canonical error space and sets the error code using
  /// the value given by `status_proto.canonical_code()`. If there is no
  /// canonical code, sets the error code to `error::GoogleError::UNKNOWN`. Note
  /// that the error message is only set if `status_proto` represents a non-ok
  /// Status.
  ///
  /// If the given `status_proto` is invalid, sets the error code of this
  /// object to `error::StatusError::INVALID_STATUS_PROTO`. A StatusProto is
  /// valid if and only if all the following conditions hold:
  ///
  ///   * If `code()` is 0, then `canonical_code()` is set to 0.
  ///   * If `canonical_code()` is 0, then `code()` is set to 0.
  ///   * If the error space is recognized, then `canonical_code()` is equal to
  ///     the equivalent canonical code given by the error space.
  ///
  /// \param status_proto A protobuf object to set this object from.
  void RestoreFrom(const StatusProto &status_proto);

  /// Indicates whether this object is the same as `code`.
  ///
  /// This object is considered to be the same as `code if `code` matches both
  /// the error code and error space of this object.
  ///
  /// \return True if this object matches `code`.
  template <typename Enum>
  bool Is(Enum code) const {
    return (static_cast<int>(code) == error_code_) &&
           (error::error_enum_traits<Enum>::get_error_space() == error_space_);
  }

 private:
  // Sets this object to hold an error code |code| and error message |message|.
  template <typename Enum>
  void Set(Enum code, absl::string_view message) {
    error_space_ = error::error_enum_traits<Enum>::get_error_space();
    error_code_ = static_cast<int>(code);
    if (error_code_ != 0) {
      message_ = std::string(message);
    } else {
      message_.clear();
    }
  }

  // Returns true if the error code for this object is in the canonical error
  // space.
  bool IsCanonical() const;

  const error::ErrorSpace *error_space_;
  int error_code_;

  // An optional error-message if error_code_ is non-zero. If error_code_ is
  // zero, then message_ is empty.
  std::string message_;
};

bool operator==(const Status &lhs, const Status &rhs);

bool operator!=(const Status &lhs, const Status &rhs);

std::ostream &operator<<(std::ostream &os, const Status &status);

/// Checks that the `Status` object in `val` is OK.
#define ASYLO_CHECK_OK(val) CHECK_EQ(::asylo::Status::OkStatus(), (val))

}  // namespace asylo

#endif  // ASYLO_UTIL_STATUS_H_
