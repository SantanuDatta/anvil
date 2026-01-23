#include "ui/Theme.h"
#include <QApplication>
#include <QStyleFactory>
#include <QSettings>

#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusInterface>
#endif

namespace Anvil::UI
{
    Theme &Theme::instance()
    {
        static Theme instance;
        return instance;
    }

    Theme::Theme() : m_mode(ThemeMode::Auto), m_isDark(false)
    {
        // Load saved theme preference
        QSettings settings("Anvil", "Anvil");
        int savedMode = settings.value("theme/mode", static_cast<int>(ThemeMode::Auto)).toInt();
        m_mode = static_cast<ThemeMode>(savedMode);

        if (m_mode == ThemeMode::Auto)
        {
            detectSystemTheme();
        }
        else
        {
            m_isDark = (m_mode == ThemeMode::Dark);
        }

        updateColors();
    }

    void Theme::setMode(ThemeMode mode)
    {
        if (m_mode == mode)
            return;

        m_mode = mode;

        if (mode == ThemeMode::Auto)
        {
            detectSystemTheme();
        }
        else
        {
            m_isDark = (mode == ThemeMode::Dark);
        }

        updateColors();
        apply();

        // Save preference
        QSettings settings("Anvil", "Anvil");
        settings.setValue("theme/mode", static_cast<int>(mode));

        emit themeChanged();
    }

    void Theme::detectSystemTheme()
    {
#ifdef Q_OS_LINUX
        // Try to detect GNOME/KDE dark mode preference
        QDBusInterface interface(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.Settings");

        if (interface.isValid())
        {
            QDBusMessage reply = interface.call("Read",
                                                "org.freedesktop.appearance",
                                                "color-scheme");

            if (reply.type() == QDBusMessage::ReplyMessage)
            {
                QVariant variant = reply.arguments().first();
                int colorScheme = variant.toInt();
                m_isDark = (colorScheme == 1); // 1 = prefer-dark
                return;
            }
        }
#endif

        // Fallback: Check Qt palette
        QPalette palette = QApplication::palette();
        QColor windowColor = palette.color(QPalette::Window);
        m_isDark = (windowColor.lightness() < 128);
    }

    void Theme::updateColors()
    {
        // Colors are already defined in m_light and m_dark
        // This method is here for future dynamic color adjustments
    }

    void Theme::apply()
    {
        QString style = generateStylesheet();
        qApp->setStyleSheet(style);
    }

    QColor Theme::background() const
    {
        return m_isDark ? m_dark.background : m_light.background;
    }

    QColor Theme::foreground() const
    {
        return m_isDark ? m_dark.foreground : m_light.foreground;
    }

    QColor Theme::card() const
    {
        return m_isDark ? m_dark.card : m_light.card;
    }

    QColor Theme::cardForeground() const
    {
        return m_isDark ? m_dark.cardForeground : m_light.cardForeground;
    }

    QColor Theme::primary() const
    {
        return m_isDark ? m_dark.primary : m_light.primary;
    }

    QColor Theme::primaryForeground() const
    {
        return m_isDark ? m_dark.primaryForeground : m_light.primaryForeground;
    }

    QColor Theme::secondary() const
    {
        return m_isDark ? m_dark.secondary : m_light.secondary;
    }

    QColor Theme::secondaryForeground() const
    {
        return m_isDark ? m_dark.secondaryForeground : m_light.secondaryForeground;
    }

    QColor Theme::muted() const
    {
        return m_isDark ? m_dark.muted : m_light.muted;
    }

    QColor Theme::mutedForeground() const
    {
        return m_isDark ? m_dark.mutedForeground : m_light.mutedForeground;
    }

    QColor Theme::accent() const
    {
        return m_isDark ? m_dark.accent : m_light.accent;
    }

    QColor Theme::accentForeground() const
    {
        return m_isDark ? m_dark.accentForeground : m_light.accentForeground;
    }

    QColor Theme::border() const
    {
        return m_isDark ? m_dark.border : m_light.border;
    }

    QColor Theme::success() const
    {
        return m_isDark ? m_dark.success : m_light.success;
    }

    QColor Theme::warning() const
    {
        return m_isDark ? m_dark.warning : m_light.warning;
    }

    QColor Theme::error() const
    {
        return m_isDark ? m_dark.error : m_light.error;
    }

    QFont Theme::fontRegular() const
    {
        QFont font("Inter", 10); // Using Inter font (modern, like shadcn)
        font.setWeight(QFont::Normal);
        return font;
    }

    QFont Theme::fontMedium() const
    {
        QFont font("Inter", 10);
        font.setWeight(QFont::Medium);
        return font;
    }

    QFont Theme::fontSemiBold() const
    {
        QFont font("Inter", 10);
        font.setWeight(QFont::DemiBold);
        return font;
    }

    QFont Theme::fontMono() const
    {
        QFont font("JetBrains Mono", 9);
        return font;
    }

    int Theme::radius(int size) const
    {
        // Border radius sizes: sm=4, md=6, lg=8, xl=12
        switch (size)
        {
        case 0:
            return 0; // none
        case 1:
            return 4; // sm
        case 2:
            return 6; // md (default)
        case 3:
            return 8; // lg
        case 4:
            return 12; // xl
        default:
            return 6;
        }
    }

    QString Theme::generateStylesheet() const
    {
        QString fg = foreground().name();
        QString bg = background().name();
        QString cardBg = card().name();
        QString cardFg = cardForeground().name();
        QString primaryColor = primary().name();
        QString primaryFg = primaryForeground().name();
        QString secondaryColor = secondary().name();
        QString borderColor = border().name();
        QString mutedColor = muted().name();
        QString mutedFg = mutedForeground().name();
        QString successColor = success().name();
        QString warningColor = warning().name();
        QString errorColor = error().name();
        QString comboIcon = m_isDark ? ":/icons/chevrons-up-down-dark.svg"
                                     : ":/icons/chevrons-up-down-light.svg";

        return QString(R"(
        /* Global Application Style */
        QWidget {
            font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
            font-size: 10pt;
            color: %1;
            background-color: %2;
        }

        /* Main Window */
        QMainWindow {
            background-color: %2;
        }

        /* Sidebar */
        QListWidget#sidebar {
            background-color: %3;
            border: none;
            border-right: 1px solid %7;
            padding: 8px;
            outline: none;
        }

        QListWidget#sidebar::item {
            color: %4;
            padding: 8px 12px;
            border-radius: 6px;
            margin: 2px 0;
        }

        QListWidget#sidebar::item:hover {
            background-color: %8;
        }

        QListWidget#sidebar::item:selected {
            background-color: %5;
            color: %6;
            font-weight: 600;
        }

        /* Cards */
        QFrame.card {
            background-color: %3;
            border: 1px solid %7;
            border-radius: 8px;
            padding: 16px;
        }

        /* Buttons - Primary */
        QPushButton.primary {
            background-color: %5;
            color: %6;
            border: none;
            border-radius: 6px;
            padding: 4px 8px;
            font-weight: 600;
            min-height: 26px;
        }

        QPushButton.primary:hover {
            background-color: %5;
            opacity: 0.9;
        }

        QPushButton.primary:pressed {
            background-color: %5;
            opacity: 0.8;
        }

        /* Buttons - Secondary */
        QPushButton.secondary {
            background-color: %9;
            color: %1;
            border: 1px solid %7;
            border-radius: 6px;
            padding: 4px 8px;
            font-weight: 500;
            min-height: 26px;
        }

        QPushButton.secondary:hover {
            background-color: %8;
        }

        /* Buttons - Destructive */
        QPushButton.destructive {
            background-color: %12;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 4px 8px;
            font-weight: 600;
            min-height: 26px;
        }

        QPushButton.destructive:hover {
            opacity: 0.9;
        }

        /* ComboBox */
        QComboBox {
            background-color: %2;
            border: 1px solid %7;
            border-radius: 6px;
            padding: 4px 8px;
            min-height: 26px;
            color: %1;
        }

        QComboBox:hover {
            border-color: %8;
        }

        QComboBox::drop-down {
            border: none;
            width: 28px;
            subcontrol-origin: padding;
            subcontrol-position: top right;
        }

        QComboBox::down-arrow {
            image: url(%13);
            width: 16px;
            height: 16px;
        }

        QComboBox QAbstractItemView {
            background-color: %3;
            border: 1px solid %7;
            border-radius: 6px;
            selection-background-color: %8;
            color: %1;
            padding: 4px;
            outline: none;
        }

        QComboBox QAbstractItemView::item {
            padding: 6px 8px;
            border-radius: 4px;
        }

        QComboBox QAbstractItemView::item:selected {
            background-color: %8;
            color: %1;
        }

        /* Labels */
        QLabel.heading {
            font-size: 18pt;
            font-weight: 600;
            color: %1;
            margin-bottom: 8px;
        }

        QLabel.subheading {
            font-size: 14pt;
            font-weight: 600;
            color: %1;
        }

        QLabel.muted {
            color: %10;
            font-size: 9pt;
        }

        /* Status Indicators */
        QLabel.status-running {
            color: %11;
            font-weight: 600;
        }

        QLabel.status-stopped {
            color: %10;
        }

        QLabel.status-error {
            color: %12;
            font-weight: 600;
        }

        /* ScrollBar */
        QScrollBar:vertical {
            background-color: transparent;
            width: 10px;
            border-radius: 5px;
        }

        QScrollBar::handle:vertical {
            background-color: %8;
            border-radius: 5px;
            min-height: 30px;
        }

        QScrollBar::handle:vertical:hover {
            background-color: %10;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }

        /* Line Edit / Input */
        QLineEdit {
            background-color: %2;
            border: 1px solid %7;
            border-radius: 6px;
            padding: 8px 12px;
            color: %1;
            min-height: 36px;
        }

        QLineEdit:focus {
            border-color: %5;
        }

        /* Table */
        QTableWidget {
            background-color: %2;
            border: 1px solid %7;
            border-radius: 8px;
            gridline-color: %7;
            color: %1;
        }

        QTableWidget::item {
            padding: 8px;
        }

        QTableWidget::item:selected {
            background-color: %8;
        }

        QHeaderView::section {
            background-color: %9;
            color: %1;
            padding: 8px;
            border: none;
            border-bottom: 1px solid %7;
            font-weight: 600;
        }

        /* Tabs */
        QTabWidget::pane {
            border: none;
            background-color: transparent;
        }

        QTabBar::tab {
            background-color: transparent;
            color: %10;
            padding: 12px 20px;
            border: none;
            border-bottom: 2px solid transparent;
            font-weight: 500;
        }

        QTabBar::tab:hover {
            color: %1;
            background-color: %8;
        }

        QTabBar::tab:selected {
            color: %1;
            border-bottom: 2px solid %5;
            font-weight: 600;
        }

        /* Tooltip */
        QToolTip {
            background-color: %3;
            color: %1;
            border: 1px solid %7;
            border-radius: 6px;
            padding: 8px;
        }
    )")
            .arg(fg)             // %1 - foreground
            .arg(bg)             // %2 - background
            .arg(cardBg)         // %3 - card background
            .arg(cardFg)         // %4 - card foreground
            .arg(primaryColor)   // %5 - primary
            .arg(primaryFg)      // %6 - primary foreground
            .arg(borderColor)    // %7 - border
            .arg(mutedColor)     // %8 - muted (hover states)
            .arg(secondaryColor) // %9 - secondary
            .arg(mutedFg)        // %10 - muted foreground
            .arg(successColor)   // %11 - success
            .arg(errorColor)     // %12 - error
            .arg(comboIcon);     // %13 - combobox icon
    }
}
