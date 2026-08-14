#pragma once

#include "CredentialStore.h"

#include <QByteArray>

#include <optional>

struct DecodedCredentialPayload {
    std::optional<CredentialTarget> target;
    StoredCredential credential;
};

[[nodiscard]] QByteArray encodeCredentialPayload(
    const CredentialTarget &target,
    const StoredCredential &credential
);
[[nodiscard]] std::optional<DecodedCredentialPayload> decodeCredentialPayload(
    const QByteArray &payload
);
