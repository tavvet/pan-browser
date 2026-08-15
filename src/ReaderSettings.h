#pragma once

#include <QString>

enum class ReaderTheme {
    System,
    Light,
    Sepia,
    Dark,
};

enum class ReaderTypeface {
    Serif,
    SansSerif,
};

class ReaderSettings {
public:
    static constexpr int minimumTextSize = 16;
    static constexpr int maximumTextSize = 32;
    static constexpr int minimumContentWidth = 520;
    static constexpr int maximumContentWidth = 1040;

    static ReaderSettings load(QString *error = nullptr);

    bool save(QString *error = nullptr) const;
    bool validate(QString *error = nullptr) const;

    [[nodiscard]] ReaderTheme theme() const;
    void setTheme(ReaderTheme theme);

    [[nodiscard]] ReaderTypeface typeface() const;
    void setTypeface(ReaderTypeface typeface);

    [[nodiscard]] int textSize() const;
    void setTextSize(int size);

    [[nodiscard]] int contentWidth() const;
    void setContentWidth(int width);

    [[nodiscard]] QString themeName() const;
    [[nodiscard]] QString typefaceName() const;

private:
    ReaderTheme m_theme = ReaderTheme::System;
    ReaderTypeface m_typeface = ReaderTypeface::Serif;
    int m_textSize = 20;
    int m_contentWidth = 720;
};
