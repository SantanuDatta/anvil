#ifndef THEME_H
#define THEME_H

#include <QObject>
#include <QString>
#include <QColor>
#include <QFont>
#include <QPalette>

namespace Anvil::UI
{
    enum class ThemeMode
    {
        Light,
        Dark,
        Auto // System-based
    };

    /**
     * @brief Modern theme system inspired by shadcn/ui
     *
     * Provides dynamic theming with light/dark modes and smooth transitions.
     * Colors follow the shadcn design tokens for consistency.
     */
    class Theme : public QObject
    {
        Q_OBJECT

    public:
        static Theme &instance();

        // Theme management
        void setMode(ThemeMode mode);
        ThemeMode mode() const { return m_mode; }
        bool isDark() const { return m_isDark; }

        // Apply theme to application
        void apply();

        // Color tokens (shadcn-inspired)
        QColor background() const;
        QColor foreground() const;
        QColor card() const;
        QColor cardForeground() const;
        QColor popover() const;
        QColor popoverForeground() const;
        QColor primary() const;
        QColor primaryForeground() const;
        QColor secondary() const;
        QColor secondaryForeground() const;
        QColor muted() const;
        QColor mutedForeground() const;
        QColor accent() const;
        QColor accentForeground() const;
        QColor destructive() const;
        QColor destructiveForeground() const;
        QColor border() const;
        QColor input() const;
        QColor ring() const;

        // Status colors
        QColor success() const;
        QColor warning() const;
        QColor error() const;
        QColor info() const;

        // Typography
        QFont fontRegular() const;
        QFont fontMedium() const;
        QFont fontSemiBold() const;
        QFont fontBold() const;
        QFont fontMono() const;

        // Spacing (in pixels)
        int spacing(int scale) const { return scale * 4; } // 4px base
        int radius(int size) const;                        // Border radius

        // Generate complete stylesheet
        QString stylesheet() const;

    signals:
        void themeChanged();

    private:
        Theme();
        void detectSystemTheme();
        void updateColors();
        QString generateStylesheet() const;

        ThemeMode m_mode;
        bool m_isDark;

        // Light theme colors
        struct LightColors
        {
            QColor background{0xFFFFFF};
            QColor foreground{0x020817};
            QColor card{0xFFFFFF};
            QColor cardForeground{0x020817};
            QColor popover{0xFFFFFF};
            QColor popoverForeground{0x020817};
            QColor primary{0x0F172A};
            QColor primaryForeground{0xF8FAFC};
            QColor secondary{0xF1F5F9};
            QColor secondaryForeground{0x0F172A};
            QColor muted{0xF1F5F9};
            QColor mutedForeground{0x64748B};
            QColor accent{0xF1F5F9};
            QColor accentForeground{0x0F172A};
            QColor destructive{0xEF4444};
            QColor destructiveForeground{0xF8FAFC};
            QColor border{0xE2E8F0};
            QColor input{0xE2E8F0};
            QColor ring{0x020817};
            QColor success{0x10B981};
            QColor warning{0xF59E0B};
            QColor error{0xEF4444};
            QColor info{0x3B82F6};
        } m_light;

        // Dark theme colors
        struct DarkColors
        {
            QColor background{0x020817};
            QColor foreground{0xF8FAFC};
            QColor card{0x020817};
            QColor cardForeground{0xF8FAFC};
            QColor popover{0x020817};
            QColor popoverForeground{0xF8FAFC};
            QColor primary{0x2563EB};
            QColor primaryForeground{0xF8FAFC};
            QColor secondary{0x1E293B};
            QColor secondaryForeground{0xF8FAFC};
            QColor muted{0x1E293B};
            QColor mutedForeground{0x94A3B8};
            QColor accent{0x1E293B};
            QColor accentForeground{0xF8FAFC};
            QColor destructive{0x7F1D1D};
            QColor destructiveForeground{0xF8FAFC};
            QColor border{0x1E293B};
            QColor input{0x1E293B};
            QColor ring{0xD1D5DB};
            QColor success{0x059669};
            QColor warning{0xD97706};
            QColor error{0x991B1B};
            QColor info{0x1E40AF};
        } m_dark;
    };
}
#endif
