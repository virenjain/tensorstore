// Copyright 2020 The TensorStore Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TENSORSTORE_INTERNAL_OAUTH2_OAUTH2_AUTH_PROVIDER_H_
#define TENSORSTORE_INTERNAL_OAUTH2_OAUTH2_AUTH_PROVIDER_H_

#include <functional>
#include <string>
#include <string_view>

#include "absl/time/time.h"
#include "tensorstore/internal/http/http_response.h"
#include "tensorstore/internal/http/http_transport.h"
#include "tensorstore/internal/oauth2/auth_provider.h"
#include "tensorstore/internal/oauth2/oauth_utils.h"
#include "tensorstore/util/result.h"
#include "tensorstore/util/status.h"

namespace tensorstore {
namespace internal_oauth2 {

class OAuth2AuthProvider : public AuthProvider {
 public:
  using RefreshToken = internal_oauth2::RefreshToken;

  ~OAuth2AuthProvider() override = default;

  OAuth2AuthProvider(const RefreshToken& creds, std::string uri,
                     std::shared_ptr<internal_http::HttpTransport> transport);
  OAuth2AuthProvider(const RefreshToken& creds, std::string uri,
                     std::shared_ptr<internal_http::HttpTransport> transport,
                     std::function<absl::Time()> clock);

  using AuthProvider::BearerTokenWithExpiration;

  /// \brief Returns the short-term authentication bearer token.
  ///
  /// Safe for concurrent use by multiple threads.
  Result<BearerTokenWithExpiration> GetToken() override;

  /// \brief Refresh the OAuth2 token.
  Status Refresh();

  bool IsExpired() { return clock_() > (expiration_ - kExpirationMargin); }

  bool IsValid() { return !access_token_.empty() && !IsExpired(); }

 protected:
  virtual Result<internal_http::HttpResponse> IssueRequest(
      std::string_view method, std::string_view uri, absl::Cord payload);

 private:
  std::string refresh_payload_;
  std::string uri_;

  std::string access_token_;
  absl::Time expiration_;

  std::shared_ptr<internal_http::HttpTransport> transport_;
  std::function<absl::Time()> clock_;  // mock time.
};

}  // namespace internal_oauth2
}  // namespace tensorstore

#endif  // TENSORSTORE_INTERNAL_OAUTH2_OAUTH2_AUTH_PROVIDER_H_
