#ifndef UTILITY_H
#define UTILITY_H

#include <expected>
#include <filesystem>
#include <system_error>

namespace bsm {

enum class error_code_e {
  OK,
  INVALID_ARGS,
  NOT_FOUND,
  PERMISSION_DENIED,
  ALREADY_EXIST,
  INTERNAL
};
struct Error {
  error_code_e code;
  std::string message;
};
using er_e = error_code_e;

inline Error make_error(error_code_e code, std::string message){
    return {code, std::move(message)};
}

std::string_view user_message(error_code_e code){
    switch (code)
    {
    case er_e OK:
        return "Expected thing happened, read \"no error\""

    default:
    }
}


using Ev = std::expected<void, Error>;

bool is_file_exist(const std::string& filename);

std::string c_error_string();

}  // namespace bsm

#endif
