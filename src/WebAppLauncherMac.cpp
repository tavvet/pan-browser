#include <CoreFoundation/CoreFoundation.h>

#include <unistd.h>

#include <array>
#include <string>

namespace {

std::string bundleString(CFStringRef key)
{
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (!bundle)
        return {};
    CFTypeRef value = CFBundleGetValueForInfoDictionaryKey(bundle, key);
    if (!value || CFGetTypeID(value) != CFStringGetTypeID())
        return {};
    const auto string = static_cast<CFStringRef>(value);
    const CFIndex maximumBytes = CFStringGetMaximumSizeForEncoding(
        CFStringGetLength(string),
        kCFStringEncodingUTF8
    ) + 1;
    if (maximumBytes <= 1)
        return {};
    std::string result(static_cast<std::size_t>(maximumBytes), '\0');
    if (!CFStringGetCString(string, result.data(), maximumBytes, kCFStringEncodingUTF8))
        return {};
    result.resize(std::char_traits<char>::length(result.c_str()));
    return result;
}

bool isValidAppId(const std::string &id)
{
    if (id.size() != 64)
        return false;
    for (const char character : id) {
        if (!((character >= '0' && character <= '9')
              || (character >= 'a' && character <= 'f'))) {
            return false;
        }
    }
    return true;
}

} // namespace

int main()
{
    const std::string appId = bundleString(CFSTR("PanBrowserWebAppId"));
    const std::string hostExecutable = bundleString(CFSTR("PanBrowserHostExecutable"));
    if (!isValidAppId(appId))
        return 2;

    if (!hostExecutable.empty() && access(hostExecutable.c_str(), X_OK) == 0) {
        execl(
            hostExecutable.c_str(),
            hostExecutable.c_str(),
            "--app-id",
            appId.c_str(),
            static_cast<char *>(nullptr)
        );
    }

    execl(
        "/usr/bin/open",
        "open",
        "-n",
        "-b",
        "dev.panbrowser.app",
        "--args",
        "--app-id",
        appId.c_str(),
        static_cast<char *>(nullptr)
    );
    return 3;
}
