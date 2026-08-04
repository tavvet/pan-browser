# PanBrowser

PanBrowser is a small Qt WebEngine viewer with domain-scoped TLS trust rules.
Normal sites use Chromium's ordinary trust configuration. If Chromium rejects a
certificate solely because its CA is unknown, PanBrowser can verify that chain
against CA certificates selected by a matching domain rule. It never installs
those certificates in the macOS Keychain.

The current implementation uses Qt 6.10 or newer and Security.framework on
macOS. The certificate validator has an explicit platform boundary so Windows
and Linux implementations can be added next.

## Run

Requirements: macOS, Xcode command-line tools, Homebrew, and Qt 6.10 or newer.

```sh
brew install qtwebengine cmake ninja
./scripts/build-app.sh
open dist/PanBrowser.app
```

For a development build:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The deploy script creates a self-contained, ad-hoc signed application at
`dist/PanBrowser.app`. Qt WebEngine makes the bundle approximately 300 MB.

On first launch PanBrowser creates:

```text
~/Library/Application Support/PanBrowser/
├── rules.json
└── Certificates/
```

Use **PanBrowser → Show Configuration Folder** to reveal it in Finder. After
editing `rules.json`, select **PanBrowser → Reload Trust Rules** or press
<kbd>⇧⌘R</kbd>.

## Trust rules

Place DER (`.cer`, `.der`) or PEM (`.pem`) CA certificates in the
`Certificates` directory. Paths in `anchors` are relative to `rules.json`.

```json
{
  "version": 1,
  "startPage": "https://example.com",
  "rules": [
    {
      "name": "Example service",
      "enabled": true,
      "domains": [
        "example-service.ru",
        "*.example-service.ru"
      ],
      "mode": "custom-only",
      "anchors": [
        "Certificates/russian-trusted-root-ca.cer"
      ]
    }
  ]
}
```

Modes:

- `system-only`: use ordinary Chromium/system trust evaluation.
- `system-plus-custom`: accept chains ending at either a system root or one of
  the configured anchors.
- `custom-only`: accept only chains ending at one of the configured anchors.

In the Qt implementation, custom validation runs when Chromium reports an
unknown-CA error. Consequently, `custom-only` is exclusive for the recovery
path, but it cannot reject a certificate that Chromium already accepted through
its system roots. `system-plus-custom` is therefore the most accurate mode for
this MVP; full exclusive pinning of otherwise valid Chromium connections
requires a lower-level network hook.

`*.example.com` matches subdomains such as `www.example.com`, but deliberately
does not match the base domain `example.com`; list the base separately if it is
needed. A wildcard like `*.com` is rejected.

Rules are matched against the host of every TLS authentication challenge,
including subresources. Add API or CDN hostnames explicitly. If configuration
loading fails, PanBrowser loads no custom anchors and reports the error in its
status bar.

## Security boundary

PanBrowser only considers overriding Chromium's `CertificateAuthorityInvalid`
error. All other errors—including hostname mismatch, expiration, revocation,
certificate transparency, and pinning failures—are rejected. It then verifies
the complete chain and hostname through Security.framework before asking Qt to
continue. A matching hostname is never a blanket “ignore certificate errors”
exception.

Nevertheless, trusting a CA means that CA can issue certificates for every
hostname covered by the rule. Keep rules narrow, obtain certificates from a
verified source, and compare their SHA-256 fingerprints before use.

## Tests

```sh
ctest --test-dir build --output-on-failure
```
