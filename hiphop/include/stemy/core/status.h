#pragma once

#include <string>
#include <utility>

namespace stemy {

/// Lightweight status type for fail-safe error reporting (no exceptions required).
class Status {
public:
  static Status Ok() { return Status(true, {}); }

  static Status Error(std::string message) {
    return Status(false, std::move(message));
  }

  [[nodiscard]] bool ok() const { return ok_; }
  [[nodiscard]] explicit operator bool() const { return ok_; }
  [[nodiscard]] const std::string& message() const { return message_; }

private:
  Status(bool ok, std::string message) : ok_(ok), message_(std::move(message)) {}

  bool ok_;
  std::string message_;
};

}  // namespace stemy
