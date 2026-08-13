#include "CredentialStore.h"

namespace {

class UnavailableCredentialStore final : public CredentialStore {
public:
    [[nodiscard]] bool isAvailable() const override
    {
        return false;
    }

    [[nodiscard]] std::optional<StoredCredential> read(
        const CredentialTarget &,
        QString *
    ) override
    {
        return std::nullopt;
    }

    bool write(const CredentialTarget &, const StoredCredential &, QString *) override
    {
        return false;
    }

    bool remove(const CredentialTarget &, QString *) override
    {
        return false;
    }
};

} // namespace

std::unique_ptr<CredentialStore> createSystemCredentialStore()
{
    return std::make_unique<UnavailableCredentialStore>();
}
