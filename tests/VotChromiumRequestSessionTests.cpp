#include "PanBrowserTestCommon.h"
#include "VotChromiumRequestSession.h"

class VotChromiumRequestSessionTests final : public QObject {
    Q_OBJECT

private slots:
    void requestTimesOutBeforeProfileIsReady();
    void abortAndFailureAreTerminal();
};

void VotChromiumRequestSessionTests::requestTimesOutBeforeProfileIsReady()
{
    VotChromiumRequest request;
    request.id = QStringLiteral("pending-timeout");
    request.timeoutMilliseconds = 20;

    VotChromiumRequestSession session(request);
    QSignalSpy responseSpy(
        &session,
        &VotChromiumRequestSession::responseReady
    );
    QTRY_COMPARE_WITH_TIMEOUT(responseSpy.count(), 1, 1000);

    const QJsonObject response = responseSpy.constFirst().constFirst().toJsonObject();
    QCOMPARE(response.value(QStringLiteral("id")).toString(), request.id);
    QCOMPARE(
        response.value(QStringLiteral("type")).toString(),
        QStringLiteral("timeout")
    );

    session.abort();
    session.fail(QStringLiteral("late failure"));
    QCOMPARE(responseSpy.count(), 1);
}

void VotChromiumRequestSessionTests::abortAndFailureAreTerminal()
{
    VotChromiumRequest abortRequest;
    abortRequest.id = QStringLiteral("abort");
    VotChromiumRequestSession aborted(abortRequest);
    QSignalSpy abortSpy(
        &aborted,
        &VotChromiumRequestSession::responseReady
    );
    aborted.abort();
    QCOMPARE(abortSpy.count(), 1);
    QCOMPARE(
        abortSpy.constFirst().constFirst().toJsonObject()
            .value(QStringLiteral("type")).toString(),
        QStringLiteral("abort")
    );
    aborted.abort();
    QCOMPARE(abortSpy.count(), 1);

    VotChromiumRequest failedRequest;
    failedRequest.id = QStringLiteral("failure");
    VotChromiumRequestSession failed(failedRequest);
    QSignalSpy failureSpy(
        &failed,
        &VotChromiumRequestSession::responseReady
    );
    failed.fail(QStringLiteral("expected failure"));
    QCOMPARE(failureSpy.count(), 1);
    const QJsonObject response = failureSpy.constFirst().constFirst().toJsonObject();
    QCOMPARE(
        response.value(QStringLiteral("type")).toString(),
        QStringLiteral("error")
    );
    QCOMPARE(
        response.value(QStringLiteral("error")).toString(),
        QStringLiteral("expected failure")
    );
}

int runVotChromiumRequestSessionTests(int argc, char **argv)
{
    QApplication application(argc, argv);
    application.setAttribute(Qt::AA_Use96Dpi, true);
    VotChromiumRequestSessionTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "VotChromiumRequestSessionTests.moc"
