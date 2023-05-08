// Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "triton/core/tritonrepoagent.h"
#include "triton/core/tritonserver.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <openssl/opensslv.h>
#include <openssl/md5.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000
#include <openssl/evp.h>
#endif

//
// Checksum Repository Agent that implements the TRITONREPOAGENT API.
//

namespace triton { namespace repoagent { namespace checksum {

using RelativeFilePath = std::string;
using HashString = std::string;

namespace {
//
// ErrorException
//
// Exception thrown if error occurs while running ChecksumRepoAgent
//
struct ErrorException {
  ErrorException(TRITONSERVER_Error* err) : err_(err) {}
  TRITONSERVER_Error* err_;
};

#define THROW_IF_TRITON_ERROR(X)                                    \
  do {                                                              \
    TRITONSERVER_Error* tie_err__ = (X);                            \
    if (tie_err__ != nullptr) {                                     \
      throw triton::repoagent::checksum::ErrorException(tie_err__); \
    }                                                               \
  } while (false)


#define THROW_TRITON_ERROR(CODE, MSG)                                 \
  do {                                                                \
    TRITONSERVER_Error* tie_err__ = TRITONSERVER_ErrorNew(CODE, MSG); \
    throw triton::repoagent::checksum::ErrorException(tie_err__);     \
  } while (false)


#define RETURN_IF_ERROR(X)               \
  do {                                   \
    TRITONSERVER_Error* rie_err__ = (X); \
    if (rie_err__ != nullptr) {          \
      return rie_err__;                  \
    }                                    \
  } while (false)

class CheckSum {
 public:
  virtual std::string AlgorithmTypeString() = 0;
  virtual std::string GenerateHash(const std::string& message) = 0;
};

class MD5Sum : public CheckSum {
 public:
  std::string AlgorithmTypeString() override { return "MD5"; }

  std::string GenerateHash(const std::string& message) override
  {
#if OPENSSL_VERSION_NUMBER >= 0x30000000
    EVP_MD_CTX *mdctx;
    const EVP_MD *md_type = EVP_get_digestbyname("md5");
    mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex2(mdctx, md_type, nullptr);
    EVP_DigestUpdate(mdctx, message.data(), message.size());
    EVP_DigestFinal_ex(mdctx, result_, nullptr);
    EVP_MD_CTX_free(mdctx);
# else
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, message.data(), message.size());
    MD5_Final(result_, &ctx);
#endif
    unsigned char low_mask = 0b00001111;
    unsigned char high_mask = 0b11110000;
    std::stringstream stream;
    for (size_t idx = 0; idx < MD5_DIGEST_LENGTH; idx++) {
      stream << std::hex << (size_t)((result_[idx] & high_mask) >> 4)
             << std::hex << (size_t)(result_[idx] & low_mask);
    }
    return stream.str();
  }

 private:
  unsigned char result_[MD5_DIGEST_LENGTH];
};

std::pair<std::unique_ptr<CheckSum>, RelativeFilePath>
ParseAlgorithmAndFile(const std::string& algorithm_and_file)
{
  auto pos = algorithm_and_file.find(":");
  if (pos == std::string::npos) {
    THROW_TRITON_ERROR(
        TRITONSERVER_ERROR_INVALID_ARG,
        (std::string("failed to parse key: ") + algorithm_and_file).c_str());
  }

  std::string lower_key = algorithm_and_file.substr(0, pos);
  std::transform(
      lower_key.begin(), lower_key.end(), lower_key.begin(),
      [](unsigned char c) { return std::tolower(c); });

  std::unique_ptr<CheckSum> hash_utility;
  if (lower_key == "md5") {
    hash_utility.reset(new MD5Sum());
  } else {
    THROW_TRITON_ERROR(
        TRITONSERVER_ERROR_UNSUPPORTED,
        (std::string("Unsupported checksum algorithm: ") +
         algorithm_and_file.substr(0, pos))
            .c_str());
  }
  return {std::move(hash_utility), algorithm_and_file.substr(pos + 1)};
}

const std::string
ReadFile(const std::string& model_dir, const std::string& relative_file_path)
{
  // It could be more efficient to generate the checksum by streaming through
  // the file than reading it in and then digesting it, but the latter is
  // sufficient as an example.
  std::ifstream in(
      model_dir + "/" + relative_file_path, std::ios::in | std::ios::binary);
  if (!in) {
    THROW_TRITON_ERROR(
        TRITONSERVER_ERROR_INVALID_ARG,
        (std::string("failed to open text file for read ") +
         relative_file_path + ": " + strerror(errno))
            .c_str());
  }

  std::string res;
  in.seekg(0, std::ios::end);
  res.resize(in.tellg());
  in.seekg(0, std::ios::beg);
  in.read(&res[0], res.size());
  in.close();

  return res;
}

}  // namespace

/////////////

extern "C" {

TRITONSERVER_Error*
TRITONREPOAGENT_ModelAction(
    TRITONREPOAGENT_Agent* agent, TRITONREPOAGENT_AgentModel* model,
    const TRITONREPOAGENT_ActionType action_type)
{
  // Return success (nullptr) if the agent does not handle the action
  if (action_type != TRITONREPOAGENT_ACTION_LOAD) {
    return nullptr;
  }
  const char* location_cstr;
  TRITONREPOAGENT_ArtifactType artifact_type;
  RETURN_IF_ERROR(TRITONREPOAGENT_ModelRepositoryLocation(
      agent, model, &artifact_type, &location_cstr));
  if (artifact_type != TRITONREPOAGENT_ARTIFACT_FILESYSTEM) {
    return TRITONSERVER_ErrorNew(
        TRITONSERVER_ERROR_UNSUPPORTED,
        (std::string("Unsupported filesystem: ") +
         std::to_string(artifact_type))
            .c_str());
  }
  const std::string location(location_cstr);

  // Check the agent parameters for the checksum configuration of the model
  uint32_t parameter_count = 0;
  RETURN_IF_ERROR(
      TRITONREPOAGENT_ModelParameterCount(agent, model, &parameter_count));
  for (uint32_t idx = 0; idx < parameter_count; idx++) {
    const char* key = nullptr;
    const char* value = nullptr;
    RETURN_IF_ERROR(
        TRITONREPOAGENT_ModelParameter(agent, model, idx, &key, &value));

    try {
      auto algorithm_and_file = ParseAlgorithmAndFile(key);
      const auto generated_hash = algorithm_and_file.first->GenerateHash(
          ReadFile(location, algorithm_and_file.second));
      if (generated_hash != value) {
        return TRITONSERVER_ErrorNew(
            TRITONSERVER_ERROR_INVALID_ARG,
            (std::string("Mismatched ") +
             algorithm_and_file.first->AlgorithmTypeString() +
             " hash for file " + algorithm_and_file.second +
             ", expect: " + value + ", got: " + generated_hash)
                .c_str());
      }
    }
    catch (const ErrorException& ee) {
      return ee.err_;
    }
  }

  return nullptr;  // success
}

}  // extern "C"

}}}  // namespace triton::repoagent::checksum