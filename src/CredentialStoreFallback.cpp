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
        CredentialStoreError *error
    ) override
    {
        setUnavailable(error);
        return std::nullopt;
    }

    bool write(
        const CredentialTarget &,
        const StoredCredential &,
        CredentialStoreError *error
    ) override
    {
        setUnavailable(error);
        return false;
    }

    bool remove(const CredentialTarget &, CredentialStoreError *error) override
    {
        setUnavailable(error);
        return false;
    }

    [[nodiscard]] QList<StoredCredentialSummary> list(
        CredentialStoreError *error
    ) override
    {
        setUnavailable(error);
        return {};
    }

private:
    static void setUnavailable(CredentialStoreError *error)
    {
        if (!error)
            return;
        error->code = CredentialStoreErrorCode::Unavailable;
        error->message = QStringLiteral(
            "The operating-system credential store is unavailable"
        );
    }
};

} // namespace

std::unique_ptr<CredentialStore> createSystemCredentialStore()
{
    return std::make_unique<UnavailableCredentialStore>();
}
