#include "PanBrowserTestCommon.h"
#include "CredentialStorePayload.h"

#include <QAbstractButton>
#include <QDataStream>
#include <QIODevice>
#include <QMessageBox>
#include <QPromise>

#include <algorithm>

namespace {

template<typename Result>
QFuture<Result> readyFuture(Result result)
{
    QPromise<Result> promise;
    QFuture<Result> future = promise.future();
    promise.start();
    promise.addResult(std::move(result));
    promise.finish();
    return future;
}

class FakeCredentialStore final : public CredentialStore {
public:
    explicit FakeCredentialStore(bool available = true)
        : m_available(available)
    {
    }

    [[nodiscard]] bool isAvailable() const override
    {
        return m_available;
    }

    [[nodiscard]] std::optional<StoredCredential> read(
        const CredentialTarget &target,
        CredentialStoreError *error
    ) override
    {
        if (error)
            error->clear();
        ++readCount;
        lastTarget = target;
        const auto found = credentials.constFind(target.identifier());
        if (found == credentials.cend()) {
            if (error)
                error->code = CredentialStoreErrorCode::NotFound;
            return std::nullopt;
        }
        return *found;
    }

    bool write(
        const CredentialTarget &target,
        const StoredCredential &credential,
        CredentialStoreError *error
    ) override
    {
        if (error)
            error->clear();
        ++writeCount;
        lastTarget = target;
        credentials.insert(target.identifier(), credential);
        targets.insert(target.identifier(), target);
        return true;
    }

    bool remove(
        const CredentialTarget &target,
        CredentialStoreError *error
    ) override
    {
        if (error)
            error->clear();
        ++removeCount;
        lastTarget = target;
        credentials.remove(target.identifier());
        targets.remove(target.identifier());
        return true;
    }

    bool removeAll(CredentialStoreError *error) override
    {
        if (error)
            error->clear();
        ++removeAllCount;
        credentials.clear();
        targets.clear();
        hiddenCredentialCount = 0;
        listError.clear();
        return true;
    }

    [[nodiscard]] QList<StoredCredentialSummary> list(
        CredentialStoreError *error
    ) override
    {
        if (error)
            *error = listError;
        QList<StoredCredentialSummary> summaries;
        for (auto iterator = targets.cbegin(); iterator != targets.cend(); ++iterator) {
            const auto credential = credentials.constFind(iterator.key());
            if (credential == credentials.cend())
                continue;
            summaries.append(StoredCredentialSummary{
                iterator.value(),
                credential->username,
                {},
            });
        }
        return summaries;
    }

    [[nodiscard]] QFuture<CredentialStoreListResult> listAsync() override
    {
        CredentialStoreListResult result;
        result.summaries = list(&result.error);
        return readyFuture(std::move(result));
    }

    [[nodiscard]] QFuture<CredentialStoreRemovalResult> removeAsync(
        const QList<CredentialTarget> &targetsToRemove
    ) override
    {
        CredentialStoreRemovalResult result;
        for (const CredentialTarget &target : targetsToRemove) {
            CredentialStoreError error;
            if (!remove(target, &error)) {
                result.failures.append(CredentialRemovalFailure{
                    target,
                    std::move(error),
                });
            }
        }
        return readyFuture(std::move(result));
    }

    [[nodiscard]] QFuture<CredentialStoreOperationResult> removeAllAsync() override
    {
        if (deferRemoveAll) {
            deferredRemoveAll = std::make_shared<
                QPromise<CredentialStoreOperationResult>
            >();
            QFuture<CredentialStoreOperationResult> future =
                deferredRemoveAll->future();
            deferredRemoveAll->start();
            return future;
        }

        CredentialStoreOperationResult result;
        result.succeeded = removeAll(&result.error);
        return readyFuture(std::move(result));
    }

    void completeDeferredRemoveAll()
    {
        if (!deferredRemoveAll)
            return;
        CredentialStoreOperationResult result;
        result.succeeded = removeAll(&result.error);
        deferredRemoveAll->addResult(std::move(result));
        deferredRemoveAll->finish();
        deferredRemoveAll.reset();
    }

    bool m_available = true;
    int readCount = 0;
    int writeCount = 0;
    int removeCount = 0;
    int removeAllCount = 0;
    int hiddenCredentialCount = 0;
    bool deferRemoveAll = false;
    std::shared_ptr<QPromise<CredentialStoreOperationResult>> deferredRemoveAll;
    CredentialStoreError listError;
    CredentialTarget lastTarget;
    QHash<QString, StoredCredential> credentials;
    QHash<QString, CredentialTarget> targets;
};

} // namespace

class CredentialsAndAuthTests final : public QObject {
    Q_OBJECT

private slots:
    void credentialTargetsAreOriginScopedAndDeterministic();
    void credentialPayloadRoundTripsMetadataAndLegacyRecords();
    void httpAuthenticationAcceptsCredentialsAndSanitizesDisplay();
    void httpAuthenticationCancelClearsAuthenticator();
    void httpAuthenticationRetriesAndWarnsForPlainHttp();
    void httpAuthenticationUsesAndRejectsSavedCredentialsWithoutLooping();
    void httpAuthenticationDoesNotPersistNonRealmSchemes();
    void httpAuthenticationDoesNotSaveWithoutOptIn();
    void httpAuthenticationSavesOnlyWithExplicitOptIn();
    void httpAuthenticationPolicyRejectsUnsafePromptContexts();
    void httpAuthenticationRealmDisplayRemovesControlCharacters();
    void proxyAuthenticationUsesSharedCredentialDialog();
    void manualProxyAuthenticationUsesSavedCredentials();
    void credentialsSettingsPageListsAndRemovesSavedCredentials();
    void nativeCredentialStoreRoundTripsWhenEnabled();
};

void CredentialsAndAuthTests::credentialTargetsAreOriginScopedAndDeterministic()
{
    const auto first = CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("https://EXAMPLE.com/private?token=secret")),
        QStringLiteral("members")
    );
    const auto sameOrigin = CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("https://example.com:443/other")),
        QStringLiteral("members")
    );
    const auto otherRealm = CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("https://example.com/")),
        QStringLiteral("administrators")
    );
    const auto otherPort = CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("https://example.com:444/")),
        QStringLiteral("members")
    );
    QVERIFY(first);
    QVERIFY(sameOrigin);
    QVERIFY(otherRealm);
    QVERIFY(otherPort);
    QCOMPARE(first->identifier(), sameOrigin->identifier());
    QVERIFY(first->identifier() != otherRealm->identifier());
    QVERIFY(first->identifier() != otherPort->identifier());
    QVERIFY(!CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("http://example.com/")),
        QStringLiteral("members")
    ));
    QVERIFY(!first->identifier().contains(QStringLiteral("example")));
}

void CredentialsAndAuthTests::credentialPayloadRoundTripsMetadataAndLegacyRecords()
{
    const auto target = CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("https://example.com/private")),
        QStringLiteral("members")
    );
    QVERIFY(target);
    const StoredCredential credential{
        QStringLiteral("alice"),
        QStringLiteral("secret"),
    };
    QByteArray payload = encodeCredentialPayload(*target, credential);
    QVERIFY(!payload.isEmpty());
    const auto decoded = decodeCredentialPayload(payload);
    QVERIFY(decoded);
    QVERIFY(decoded->target);
    QCOMPARE(decoded->target->identifier(), target->identifier());
    QCOMPARE(decoded->credential.username, credential.username);
    QCOMPARE(decoded->credential.password, credential.password);

    for (qsizetype size = 0; size < payload.size(); ++size)
        QVERIFY(!decodeCredentialPayload(payload.first(size)));

    QByteArray impossibleLengthPayload = payload;
    QVERIFY(impossibleLengthPayload.size() > 12);
    impossibleLengthPayload.replace(8, 4, QByteArray::fromHex("7fffffff"));
    QVERIFY(!decodeCredentialPayload(impossibleLengthPayload));

    QByteArray oddStringLengthPayload = payload;
    oddStringLengthPayload.replace(8, 4, QByteArray::fromHex("00000001"));
    QVERIFY(!decodeCredentialPayload(oddStringLengthPayload));

    QByteArray oversizedPayload(128 * 1024 + 1, '\0');
    QVERIFY(!decodeCredentialPayload(oversizedPayload));

    payload.append('x');
    QVERIFY(!decodeCredentialPayload(payload));

    QByteArray legacyPayload;
    QDataStream stream(&legacyPayload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << quint32(0x50424352)
           << quint16(1)
           << QStringLiteral("legacy-user")
           << QStringLiteral("legacy-password");
    const auto legacy = decodeCredentialPayload(legacyPayload);
    QVERIFY(legacy);
    QVERIFY(!legacy->target);
    QCOMPARE(legacy->credential.username, QStringLiteral("legacy-user"));
    QCOMPARE(legacy->credential.password, QStringLiteral("legacy-password"));

    CredentialStoreError notFound;
    notFound.code = CredentialStoreErrorCode::NotFound;
    QVERIFY(!notFound.shouldReport());
    notFound.code = CredentialStoreErrorCode::AccessDenied;
    QVERIFY(notFound.shouldReport());
    notFound.clear();
    QCOMPARE(notFound.code, CredentialStoreErrorCode::None);
}

void CredentialsAndAuthTests::httpAuthenticationAcceptsCredentialsAndSanitizesDisplay()
{
    FakeCredentialStore store(false);
    HttpAuthenticationController controller(nullptr, &store);
    QAuthenticator authenticator;
    authenticator.setUser(QStringLiteral("suggested-user"));
    bool handled = false;
    bool originWasSanitized = false;
    QString suggestedUsername;

    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        handled = true;
        QString visibleText;
        for (const QLabel *label : dialog->findChildren<QLabel *>())
            visibleText += label->text() + QLatin1Char('\n');
        originWasSanitized = visibleText.contains(QStringLiteral("https://example.com"))
            && !visibleText.contains(QStringLiteral("alice"))
            && !visibleText.contains(QStringLiteral("url-secret"))
            && !visibleText.contains(QStringLiteral("private/path"));

        auto *username = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialUsername")
        );
        auto *password = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialPassword")
        );
        if (!username || !password) {
            dialog->reject();
            return;
        }
        suggestedUsername = username->text();
        username->setText(QStringLiteral("alice"));
        password->setText(QStringLiteral("top-secret"));
        dialog->accept();
    });

    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral(
            "https://alice:url-secret@Example.COM/private/path?token=url-secret"
        )),
        &authenticator
    );

    QVERIFY(handled);
    QVERIFY(originWasSanitized);
    QCOMPARE(suggestedUsername, QStringLiteral("suggested-user"));
    QCOMPARE(authenticator.user(), QStringLiteral("alice"));
    QCOMPARE(authenticator.password(), QStringLiteral("top-secret"));
}

void CredentialsAndAuthTests::httpAuthenticationCancelClearsAuthenticator()
{
    FakeCredentialStore store(false);
    HttpAuthenticationController controller(nullptr, &store);
    QAuthenticator authenticator;
    authenticator.setUser(QStringLiteral("stale-user"));
    authenticator.setPassword(QStringLiteral("stale-password"));
    bool handled = false;

    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        handled = true;
        dialog->reject();
    });
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("https://example.com/protected")),
        &authenticator
    );

    QVERIFY(handled);
    QVERIFY(authenticator.isNull());
    QVERIFY(authenticator.user().isEmpty());
    QVERIFY(authenticator.password().isEmpty());
}

void CredentialsAndAuthTests::httpAuthenticationRetriesAndWarnsForPlainHttp()
{
    FakeCredentialStore store(true);
    HttpAuthenticationController controller(nullptr, &store);
    QAuthenticator firstAttempt;
    bool firstHandled = false;
    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        firstHandled = true;
        auto *username = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialUsername")
        );
        auto *password = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialPassword")
        );
        if (!username || !password) {
            dialog->reject();
            return;
        }
        username->setText(QStringLiteral("wrong-user"));
        password->setText(QStringLiteral("wrong-password"));
        dialog->accept();
    });
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("http://example.com/private")),
        &firstAttempt
    );
    QVERIFY(firstHandled);

    QAuthenticator retryAttempt;
    bool retryHandled = false;
    bool retryWasVisible = false;
    bool warningWasVisible = false;
    bool rememberWasHidden = false;
    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        retryHandled = true;
        retryWasVisible = dialog->findChild<QLabel *>(QStringLiteral("errorText"));
        warningWasVisible = dialog->findChild<QLabel *>(
            QStringLiteral("insecureTransportWarning")
        );
        rememberWasHidden = !dialog->findChild<QCheckBox *>(
            QStringLiteral("rememberCredential")
        );
        dialog->reject();
    });
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("http://EXAMPLE.com:80/another-path")),
        &retryAttempt
    );

    QVERIFY(retryHandled);
    QVERIFY(retryWasVisible);
    QVERIFY(warningWasVisible);
    QVERIFY(rememberWasHidden);
    QVERIFY(retryAttempt.isNull());
    QCOMPARE(store.readCount, 0);
    QCOMPARE(store.writeCount, 0);
}

void CredentialsAndAuthTests::httpAuthenticationUsesAndRejectsSavedCredentialsWithoutLooping()
{
    FakeCredentialStore store;
    const QUrl url(QStringLiteral("https://example.com/private"));
    const QString realm = QStringLiteral("members");
    const auto target = CredentialTarget::forHttpServer(url, realm);
    QVERIFY(target);
    store.credentials.insert(
        target->identifier(),
        StoredCredential{QStringLiteral("saved-user"), QStringLiteral("saved-password")}
    );
    HttpAuthenticationController controller(nullptr, &store);

    QAuthenticator firstAttempt;
    firstAttempt.setRealm(realm);
    controller.requestAuthentication(nullptr, url, &firstAttempt);
    QCOMPARE(firstAttempt.user(), QStringLiteral("saved-user"));
    QCOMPARE(firstAttempt.password(), QStringLiteral("saved-password"));
    QCOMPARE(store.readCount, 1);

    bool retryHandled = false;
    bool savedCredentialWarningVisible = false;
    bool replacementWasSelected = false;
    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        retryHandled = true;
        const QLabel *error = dialog->findChild<QLabel *>(QStringLiteral("errorText"));
        savedCredentialWarningVisible = error
            && error->text().contains(QStringLiteral("were removed"));
        const auto *remember = dialog->findChild<QCheckBox *>(
            QStringLiteral("rememberCredential")
        );
        replacementWasSelected = remember && remember->isChecked();
        dialog->reject();
    });
    QAuthenticator retryAttempt;
    retryAttempt.setRealm(realm);
    controller.requestAuthentication(nullptr, url, &retryAttempt);
    QVERIFY(retryHandled);
    QVERIFY(savedCredentialWarningVisible);
    QVERIFY(replacementWasSelected);
    QVERIFY(retryAttempt.isNull());
    QCOMPARE(store.readCount, 1);
    QCOMPARE(store.removeCount, 1);
    QVERIFY(!store.credentials.contains(target->identifier()));
}

void CredentialsAndAuthTests::httpAuthenticationDoesNotPersistNonRealmSchemes()
{
    FakeCredentialStore store;
    HttpAuthenticationController controller(nullptr, &store);
    QAuthenticator authenticator;
    bool rememberWasHidden = false;

    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        rememberWasHidden = !dialog->findChild<QCheckBox *>(
            QStringLiteral("rememberCredential")
        );
        dialog->reject();
    });
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("https://example.com/private")),
        &authenticator
    );

    QVERIFY(rememberWasHidden);
    QCOMPARE(store.readCount, 0);
    QCOMPARE(store.writeCount, 0);
}

void CredentialsAndAuthTests::httpAuthenticationDoesNotSaveWithoutOptIn()
{
    FakeCredentialStore store;
    HttpAuthenticationController controller(nullptr, &store);
    QAuthenticator authenticator;
    authenticator.setRealm(QStringLiteral("members"));

    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        auto *username = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialUsername")
        );
        auto *password = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialPassword")
        );
        if (!username || !password) {
            dialog->reject();
            return;
        }
        username->setText(QStringLiteral("session-user"));
        password->setText(QStringLiteral("session-secret"));
        dialog->accept();
    });
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("https://example.com/private")),
        &authenticator
    );

    QCOMPARE(store.writeCount, 0);
    QCOMPARE(authenticator.user(), QStringLiteral("session-user"));
    QCOMPARE(authenticator.password(), QStringLiteral("session-secret"));
}

void CredentialsAndAuthTests::httpAuthenticationSavesOnlyWithExplicitOptIn()
{
    FakeCredentialStore store;
    HttpAuthenticationController controller(nullptr, &store);
    QAuthenticator authenticator;
    authenticator.setRealm(QStringLiteral("members"));
    bool rememberWasOffByDefault = false;

    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        auto *username = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialUsername")
        );
        auto *password = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialPassword")
        );
        auto *remember = dialog->findChild<QCheckBox *>(
            QStringLiteral("rememberCredential")
        );
        if (!username || !password || !remember) {
            dialog->reject();
            return;
        }
        rememberWasOffByDefault = !remember->isChecked();
        username->setText(QStringLiteral("alice"));
        password->setText(QStringLiteral("secret"));
        remember->setChecked(true);
        dialog->accept();
    });
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("https://example.com/private")),
        &authenticator
    );

    QVERIFY(rememberWasOffByDefault);
    QCOMPARE(store.writeCount, 1);
    QCOMPARE(store.lastTarget.host, QStringLiteral("example.com"));
    QCOMPARE(store.lastTarget.port, 443);
    QCOMPARE(store.lastTarget.realm, QStringLiteral("members"));
    QCOMPARE(authenticator.user(), QStringLiteral("alice"));
    QCOMPARE(authenticator.password(), QStringLiteral("secret"));

    bool rejectedSavedCredentialWasReported = false;
    bool replacementWasSelected = false;
    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        const auto *error = dialog->findChild<QLabel *>(QStringLiteral("errorText"));
        rejectedSavedCredentialWasReported = error
            && error->text().contains(QStringLiteral("were removed"));
        const auto *remember = dialog->findChild<QCheckBox *>(
            QStringLiteral("rememberCredential")
        );
        replacementWasSelected = remember && remember->isChecked();
        dialog->reject();
    });
    QAuthenticator retryAttempt;
    retryAttempt.setRealm(QStringLiteral("members"));
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("https://example.com/private")),
        &retryAttempt
    );

    QVERIFY(rejectedSavedCredentialWasReported);
    QVERIFY(replacementWasSelected);
    QCOMPARE(store.removeCount, 1);
    QVERIFY(store.credentials.isEmpty());
}

void CredentialsAndAuthTests::httpAuthenticationPolicyRejectsUnsafePromptContexts()
{
    const QUrl requestUrl(QStringLiteral("https://auth.example.com/private"));
    const QUrl sameOrigin(QStringLiteral("https://AUTH.example.com:443/login"));

    QVERIFY(HttpAuthenticationPolicy::promptAllowed(
        requestUrl,
        sameOrigin,
        true,
        true
    ));
    QVERIFY(!HttpAuthenticationPolicy::promptAllowed(
        requestUrl,
        sameOrigin,
        false,
        true
    ));
    QVERIFY(!HttpAuthenticationPolicy::promptAllowed(
        requestUrl,
        sameOrigin,
        true,
        false
    ));
    QVERIFY(!HttpAuthenticationPolicy::promptAllowed(
        QUrl(QStringLiteral("https://third-party.example/protected.png")),
        sameOrigin,
        true,
        true
    ));
    QVERIFY(!HttpAuthenticationPolicy::promptAllowed(
        QUrl(QStringLiteral("http://auth.example.com/private")),
        sameOrigin,
        true,
        true
    ));
    QVERIFY(!HttpAuthenticationPolicy::promptAllowed(
        QUrl(QStringLiteral("https://auth.example.com:444/private")),
        sameOrigin,
        true,
        true
    ));
    QVERIFY(!HttpAuthenticationPolicy::promptAllowed(
        requestUrl,
        QUrl(QStringLiteral("about:blank")),
        true,
        true
    ));
}

void CredentialsAndAuthTests::httpAuthenticationRealmDisplayRemovesControlCharacters()
{
    const QString maliciousRealm = QStringLiteral("  Bank")
        + QChar(0x202e) + QStringLiteral("evil") + QChar(0x202c)
        + QLatin1Char('\n') + QStringLiteral("Admin")
        + QChar(0x2066) + QStringLiteral("test") + QChar(0x2069)
        + QStringLiteral("  ");
    const QString displayed = HttpAuthenticationPolicy::realmForDisplay(maliciousRealm);
    QCOMPARE(displayed, QStringLiteral("Bank evil Admin test"));
    for (const QChar character : displayed) {
        QVERIFY(character.category() != QChar::Other_Control);
        QVERIFY(character.category() != QChar::Other_Format);
        QVERIFY(character.category() != QChar::Separator_Line);
        QVERIFY(character.category() != QChar::Separator_Paragraph);
    }

    const QString truncated = HttpAuthenticationPolicy::realmForDisplay(
        QString(400, QLatin1Char('x'))
    );
    QCOMPARE(truncated.size(), 300);
    QVERIFY(truncated.endsWith(QChar(0x2026)));
}

void CredentialsAndAuthTests::proxyAuthenticationUsesSharedCredentialDialog()
{
    ProxySettings settings = ProxySettings::defaults();
    settings.setMode(ProxyMode::Manual);
    settings.setManualType(ManualProxyType::Http);
    settings.setHost(QStringLiteral("proxy.example.com"));
    settings.setPort(3128);
    settings.setUsername(QStringLiteral("configured-user"));
    FakeCredentialStore store(false);
    ProxyAuthenticationController controller(settings, nullptr, &store);
    QAuthenticator authenticator;
    bool handled = false;
    QString suggestedUsername;

    QTimer::singleShot(0, &controller, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        handled = dialog->objectName() == QStringLiteral("proxyAuthenticationDialog");
        auto *username = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialUsername")
        );
        auto *password = dialog->findChild<QLineEdit *>(
            QStringLiteral("credentialPassword")
        );
        if (!username || !password) {
            dialog->reject();
            return;
        }
        suggestedUsername = username->text();
        password->setText(QStringLiteral("proxy-password"));
        dialog->accept();
    });
    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("https://example.com/private")),
        &authenticator,
        QStringLiteral("proxy.example.com")
    );

    QVERIFY(handled);
    QCOMPARE(suggestedUsername, QStringLiteral("configured-user"));
    QCOMPARE(authenticator.user(), QStringLiteral("configured-user"));
    QCOMPARE(authenticator.password(), QStringLiteral("proxy-password"));
}

void CredentialsAndAuthTests::manualProxyAuthenticationUsesSavedCredentials()
{
    ProxySettings settings = ProxySettings::defaults();
    settings.setMode(ProxyMode::Manual);
    settings.setManualType(ManualProxyType::Http);
    settings.setHost(QStringLiteral("proxy.example.com"));
    settings.setPort(3128);
    FakeCredentialStore store;
    const auto target = CredentialTarget::forHttpProxy(
        QStringLiteral("proxy.example.com"),
        3128,
        QStringLiteral("proxy-realm")
    );
    QVERIFY(target);
    store.credentials.insert(
        target->identifier(),
        StoredCredential{QStringLiteral("proxy-user"), QStringLiteral("proxy-secret")}
    );
    ProxyAuthenticationController controller(settings, nullptr, &store);
    QAuthenticator authenticator;
    authenticator.setRealm(QStringLiteral("proxy-realm"));

    controller.requestAuthentication(
        nullptr,
        QUrl(QStringLiteral("https://example.com/private")),
        &authenticator,
        QStringLiteral("proxy.example.com")
    );

    QCOMPARE(store.readCount, 1);
    QCOMPARE(authenticator.user(), QStringLiteral("proxy-user"));
    QCOMPARE(authenticator.password(), QStringLiteral("proxy-secret"));
}

void CredentialsAndAuthTests::credentialsSettingsPageListsAndRemovesSavedCredentials()
{
    FakeCredentialStore store;
    const auto websiteTarget = CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("https://accounts.example.com/")),
        QStringLiteral("Members\narea")
    );
    const auto proxyTarget = CredentialTarget::forHttpProxy(
        QStringLiteral("proxy.example.com"),
        3128,
        QStringLiteral("Office proxy")
    );
    QVERIFY(websiteTarget);
    QVERIFY(proxyTarget);

    CredentialStoreError error;
    QVERIFY(store.write(
        *websiteTarget,
        StoredCredential{
            QStringLiteral("alice\nuser"),
            QStringLiteral("website-secret-value"),
        },
        &error
    ));
    QVERIFY(store.write(
        *proxyTarget,
        StoredCredential{
            QStringLiteral("proxy-user"),
            QStringLiteral("proxy-secret-value"),
        },
        &error
    ));

    CredentialsSettingsPage page(&store);
    page.show();
    QApplication::processEvents();

    auto *list = page.findChild<QListWidget *>(
        QStringLiteral("credentialsList")
    );
    auto *removeSelected = page.findChild<QPushButton *>(
        QStringLiteral("removeSelectedCredentials")
    );
    auto *removeAll = page.findChild<QPushButton *>(
        QStringLiteral("removeAllCredentials")
    );
    auto *refresh = page.findChild<QPushButton *>(
        QStringLiteral("refreshCredentials")
    );
    QVERIFY(list);
    QVERIFY(removeSelected);
    QVERIFY(removeAll);
    QVERIFY(refresh);
    QTRY_COMPARE(list->count(), 2);

    QString visibleText;
    int websiteRow = -1;
    for (int row = 0; row < list->count(); ++row) {
        const QListWidgetItem *item = list->item(row);
        visibleText += item->text() + QLatin1Char('\n');
        if (item->data(Qt::UserRole).toString() == websiteTarget->identifier())
            websiteRow = row;
    }
    QVERIFY(visibleText.contains(QStringLiteral("accounts.example.com")));
    QVERIFY(visibleText.contains(QStringLiteral("alice user")));
    QVERIFY(visibleText.contains(QStringLiteral("Members area")));
    QVERIFY(visibleText.contains(QStringLiteral("proxy.example.com:3128")));
    QVERIFY(!visibleText.contains(QStringLiteral("website-secret-value")));
    QVERIFY(!visibleText.contains(QStringLiteral("proxy-secret-value")));
    QVERIFY(websiteRow >= 0);

    list->setCurrentRow(websiteRow);
    QVERIFY(removeSelected->isEnabled());
    bool confirmedSelectedRemoval = false;
    QTimer::singleShot(0, &page, [&] {
        auto *messageBox = qobject_cast<QMessageBox *>(
            QApplication::activeModalWidget()
        );
        if (!messageBox)
            return;
        confirmedSelectedRemoval = true;
        messageBox->button(QMessageBox::Yes)->click();
    });
    removeSelected->click();
    QVERIFY(confirmedSelectedRemoval);
    QTRY_COMPARE(store.removeCount, 1);
    QVERIFY(!store.targets.contains(websiteTarget->identifier()));
    QTRY_COMPARE(list->count(), 1);

    store.hiddenCredentialCount = 1;
    store.listError.code = CredentialStoreErrorCode::CorruptData;
    store.listError.message = QStringLiteral("A hidden credential is corrupt");
    refresh->click();
    const auto hasPartialStatus = [&page] {
        const QList<QLabel *> labels = page.findChildren<QLabel *>();
        return std::any_of(
            labels.cbegin(),
            labels.cend(),
            [](const QLabel *label) {
                return label->text().contains(
                    QStringLiteral("Some entries could not be read")
                );
            }
        );
    };
    QTRY_VERIFY(hasPartialStatus());

    store.credentials.clear();
    store.targets.clear();
    store.listError.code = CredentialStoreErrorCode::AccessDenied;
    store.listError.message = QStringLiteral("The password manager is locked");
    refresh->click();
    QTRY_VERIFY(
        list->count() == 1
            && list->item(0)->text() == QStringLiteral("No readable credentials")
    );
    QVERIFY(removeAll->isEnabled());

    bool confirmedAllRemoval = false;
    store.deferRemoveAll = true;
    QSignalSpy destructiveOperationSpy(
        &page,
        &CredentialsSettingsPage::destructiveOperationActiveChanged
    );
    QTimer::singleShot(0, &page, [&] {
        auto *messageBox = qobject_cast<QMessageBox *>(
            QApplication::activeModalWidget()
        );
        if (!messageBox)
            return;
        confirmedAllRemoval = true;
        messageBox->button(QMessageBox::Yes)->click();
    });
    removeAll->click();
    QVERIFY(confirmedAllRemoval);
    QCOMPARE(destructiveOperationSpy.count(), 1);
    QVERIFY(destructiveOperationSpy.constFirst().constFirst().toBool());
    QVERIFY(!refresh->isEnabled());
    QCOMPARE(store.removeAllCount, 0);

    store.completeDeferredRemoveAll();
    QTRY_COMPARE(store.removeAllCount, 1);
    QTRY_COMPARE(destructiveOperationSpy.count(), 2);
    QVERIFY(!destructiveOperationSpy.constLast().constFirst().toBool());
    QCOMPARE(store.removeCount, 1);
    QCOMPARE(store.hiddenCredentialCount, 0);
    QVERIFY(store.targets.isEmpty());
    QTRY_VERIFY(
        list->count() == 1
            && !(list->item(0)->flags() & Qt::ItemIsEnabled)
    );
}

void CredentialsAndAuthTests::nativeCredentialStoreRoundTripsWhenEnabled()
{
#if !defined(Q_OS_MACOS) && !defined(Q_OS_WIN) && !defined(Q_OS_LINUX)
    QSKIP("The native credential-store integration test is unavailable on this platform");
#else
    if (!qEnvironmentVariableIsSet("PANBROWSER_RUN_CREDENTIAL_STORE_TESTS")
        && !qEnvironmentVariableIsSet("PANBROWSER_RUN_KEYCHAIN_TESTS")) {
        QSKIP("Set PANBROWSER_RUN_CREDENTIAL_STORE_TESTS=1 to exercise the native credential store");
    }

    std::shared_ptr<CredentialStore> store = createSystemCredentialStore();
    QVERIFY(store);
    QVERIFY(store->isAvailable());
    const QString uniqueHost = QStringLiteral("credential-store-test-%1.invalid").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
    );
    const auto target = CredentialTarget::forHttpServer(
        QUrl(QStringLiteral("https://%1/").arg(uniqueHost)),
        QStringLiteral("PanBrowser automated credential-store test")
    );
    QVERIFY(target);

    struct Cleanup final {
        CredentialStore *store = nullptr;
        CredentialTarget target;
        bool active = true;
        ~Cleanup()
        {
            if (!active || !store)
                return;
            CredentialStoreError ignoredError;
            store->remove(target, &ignoredError);
        }
    } cleanup{store.get(), *target};

    CredentialStoreError error;
    QVERIFY2(store->write(
        *target,
        StoredCredential{QStringLiteral("test-user"), QStringLiteral("test-password")},
        &error
    ), qPrintable(error.message));
    auto loaded = store->read(*target, &error);
    QVERIFY2(loaded.has_value(), qPrintable(error.message));
    QCOMPARE(loaded->username, QStringLiteral("test-user"));
    QCOMPARE(loaded->password, QStringLiteral("test-password"));

    const QList<StoredCredentialSummary> initialSummaries = store->list(&error);
    QVERIFY2(!error.shouldReport(), qPrintable(error.message));
    const auto initialSummary = std::find_if(
        initialSummaries.cbegin(),
        initialSummaries.cend(),
        [&](const StoredCredentialSummary &summary) {
            return summary.target.identifier() == target->identifier();
        }
    );
    QVERIFY(initialSummary != initialSummaries.cend());
    QCOMPARE(initialSummary->username, QStringLiteral("test-user"));
    QVERIFY(initialSummary->lastModified.isValid());

    QVERIFY2(store->write(
        *target,
        StoredCredential{QStringLiteral("updated-user"), QStringLiteral("updated-password")},
        &error
    ), qPrintable(error.message));
    loaded = store->read(*target, &error);
    QVERIFY2(loaded.has_value(), qPrintable(error.message));
    QCOMPARE(loaded->username, QStringLiteral("updated-user"));
    QCOMPARE(loaded->password, QStringLiteral("updated-password"));
    QVERIFY2(store->remove(*target, &error), qPrintable(error.message));
    loaded = store->read(*target, &error);
    QVERIFY2(!loaded.has_value(), qPrintable(error.message));
    QCOMPARE(error.code, CredentialStoreErrorCode::NotFound);
    cleanup.active = false;
#endif
}



int runCredentialsAndAuthTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    CredentialsAndAuthTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "CredentialsAndAuthTests.moc"
