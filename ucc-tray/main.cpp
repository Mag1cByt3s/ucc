/*
 * Copyright (C) 2026 Uniwill Control Center Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QProcess>
#include <QQuickView>
#include <QQmlContext>
#include <QQuickItem>
#include <QScreen>
#include <QQuickWindow>
#include <QTimer>
#include <QGuiApplication>
#include <QCursor>
#include <memory>

#ifdef UCC_HAVE_LAYER_SHELL_QT
#include <LayerShellQt/Window>
#include <LayerShellQt/Shell>
#endif

#include "TrayBackend.hpp"

// ---------------------------------------------------------------------------
// TrayController — owns the tray icon, context menu and QML popup
// ---------------------------------------------------------------------------

class TrayController : public QObject
{
  Q_OBJECT

public:
  TrayController( QObject *parent = nullptr )
    : QObject( parent )
    , m_backend( std::make_unique< TrayBackend >() )
  {
    createTrayIcon();
    createPopup();
  }

private slots:
  void togglePopup()
  {
    if ( m_popup->isVisible() )
    {
      m_popup->hide();
    }
    else
    {
      positionPopup();
      m_popup->show();
      m_popup->raise();
      m_popup->requestActivate();
    }
  }

  void showMainWindow()
  {
    QProcess::startDetached( "ucc-gui", QStringList() );
  }

  void showAbout()
  {
    QMessageBox::about(
      nullptr,
      tr( "About UCC Tray" ),
      tr( "Uniwill Control Center System Tray\n"
          "Version 0.1.0\n\n"
          "Quick access to system controls." ) );
  }

  void onPopupActiveChanged()
  {
    // Auto-hide popup when it loses focus (click-outside dismissal)
    if ( m_popup && !m_popup->isActive() && m_popup->isVisible() )
    {
      // Small delay to avoid hiding when the user is clicking tray icon to close
      QTimer::singleShot( 150, this, [this]() {
        if ( m_popup && !m_popup->isActive() )
          m_popup->hide();
      } );
    }
  }

private:
  void createTrayIcon()
  {
    m_trayIcon = new QSystemTrayIcon( this );
    m_trayIcon->setIcon( QIcon::fromTheme( "ucc-tray" ) );
    m_trayIcon->setToolTip( tr( "Uniwill Control Center" ) );

    // Context menu (right-click)
    auto *menu = new QMenu();

    auto *openAction = menu->addAction( tr( "Open Control Center" ) );
    connect( openAction, &QAction::triggered, this, &TrayController::showMainWindow );

    menu->addSeparator();

    auto *aboutAction = menu->addAction( tr( "About" ) );
    connect( aboutAction, &QAction::triggered, this, &TrayController::showAbout );

    auto *quitAction = menu->addAction( tr( "Quit" ) );
    connect( quitAction, &QAction::triggered, qApp, &QApplication::quit );

    m_trayIcon->setContextMenu( menu );
    m_trayIcon->show();

    // Left-click toggles the popup
    connect( m_trayIcon, &QSystemTrayIcon::activated,
             [this]( QSystemTrayIcon::ActivationReason reason ) {
      if ( reason == QSystemTrayIcon::Trigger )
      {
        togglePopup();
      }
    } );
  }

  void createPopup()
  {
    m_popup = new QQuickView();

    // Qt::Popup requires a transient parent with input on Wayland, which
    // QSystemTrayIcon cannot provide.  Use Qt::Tool instead — it keeps the
    // window out of the taskbar and does not require a grab.
    const bool isWayland = QGuiApplication::platformName().contains( "wayland" );
    Qt::WindowFlags flags = Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint;
    if ( isWayland )
      flags |= Qt::Tool;       // no grab, no taskbar, works on Wayland
    else
      flags |= Qt::Popup;      // grab-on-show, auto-dismiss on X11
    m_popup->setFlags( flags );

    m_popup->setResizeMode( QQuickView::SizeRootObjectToView );
    m_popup->setMinimumSize( QSize( 520, 370 ) );
    m_popup->resize( 520, 370 );
    m_popup->setColor( Qt::transparent );

    // Expose the backend to QML
    m_popup->rootContext()->setContextProperty( "backend", m_backend.get() );

    m_popup->setSource( QUrl( "qrc:/qml/qml/TrayPopup.qml" ) );

#ifdef UCC_HAVE_LAYER_SHELL_QT
    if ( isWayland )
    {
      auto *lsWin = LayerShellQt::Window::get( m_popup );
      lsWin->setLayer( LayerShellQt::Window::LayerOverlay );
      lsWin->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityOnDemand );
      lsWin->setScope( "ucc-tray-popup" );
      m_useLayerShell = true;
    }
#endif

    // Auto-hide on focus loss
    connect( m_popup, &QQuickView::activeChanged,
             this, &TrayController::onPopupActiveChanged );
  }

  void positionPopup()
  {
    QScreen *screen = QGuiApplication::primaryScreen();

    // Try to find screen from tray icon or cursor
    QRect trayGeom = m_trayIcon->geometry();
    QPoint cursorPos = QCursor::pos();
    QPoint anchor = trayGeom.isValid() ? trayGeom.center() : cursorPos;
    for ( auto *s : QGuiApplication::screens() )
    {
      if ( s->geometry().contains( anchor ) )
      {
        screen = s;
        break;
      }
    }

    QRect fullGeom = screen->geometry();
    QRect availGeom = screen->availableGeometry();
    int popupW = m_popup->width();
    int popupH = m_popup->height();

    // --- Detect panel edge from available vs full geometry ---
    int panelTop    = availGeom.top() - fullGeom.top();
    int panelBottom = fullGeom.bottom() - availGeom.bottom();
    int panelRight  = fullGeom.right() - availGeom.right();
    int panelLeft   = availGeom.left() - fullGeom.left();

#ifdef UCC_HAVE_LAYER_SHELL_QT
    if ( m_useLayerShell )
    {
      auto *lsWin = LayerShellQt::Window::get( m_popup );
      LayerShellQt::Window::Anchors anchors;
      int marginH = 8, marginV = 4;

      if ( panelBottom >= panelTop && panelBottom >= panelLeft && panelBottom >= panelRight )
      {
        anchors = LayerShellQt::Window::Anchors( LayerShellQt::Window::AnchorBottom ) | LayerShellQt::Window::AnchorRight;
        lsWin->setMargins( QMargins( 0, 0, marginH, marginV ) );
      }
      else if ( panelTop >= panelBottom && panelTop >= panelLeft && panelTop >= panelRight )
      {
        anchors = LayerShellQt::Window::Anchors( LayerShellQt::Window::AnchorTop ) | LayerShellQt::Window::AnchorRight;
        lsWin->setMargins( QMargins( 0, marginV, marginH, 0 ) );
      }
      else if ( panelRight >= panelLeft )
      {
        anchors = LayerShellQt::Window::Anchors( LayerShellQt::Window::AnchorRight ) | LayerShellQt::Window::AnchorTop;
        lsWin->setMargins( QMargins( 0, marginV, marginH, 0 ) );
      }
      else
      {
        anchors = LayerShellQt::Window::Anchors( LayerShellQt::Window::AnchorLeft ) | LayerShellQt::Window::AnchorTop;
        lsWin->setMargins( QMargins( marginH, marginV, 0, 0 ) );
      }

      lsWin->setAnchors( anchors );
      return;
    }
#endif

    int x, y;

    if ( trayGeom.isValid() && trayGeom.width() > 0 )
    {
      // Position relative to tray icon (works on X11)
      x = trayGeom.center().x() - popupW / 2;
      y = trayGeom.bottom() + 4;

      if ( y + popupH > availGeom.bottom() )
        y = trayGeom.top() - popupH - 4;
    }
    else
    {
      // Wayland / fallback: position near the detected panel edge
      if ( panelBottom >= panelTop && panelBottom >= panelLeft && panelBottom >= panelRight )
      {
        // Panel at bottom — popup above the panel, right-aligned
        x = availGeom.right() - popupW - 8;
        y = availGeom.bottom() - popupH - 4;
      }
      else if ( panelTop >= panelBottom && panelTop >= panelLeft && panelTop >= panelRight )
      {
        // Panel at top — popup below the panel, right-aligned
        x = availGeom.right() - popupW - 8;
        y = availGeom.top() + 4;
      }
      else if ( panelRight >= panelLeft )
      {
        // Panel at right — popup to the left of the panel, top-aligned
        x = availGeom.right() - popupW - 4;
        y = availGeom.top() + 8;
      }
      else
      {
        // Panel at left — popup to the right of the panel, top-aligned
        x = availGeom.left() + 4;
        y = availGeom.top() + 8;
      }
    }

    // Clamp to available screen edges
    if ( y + popupH > availGeom.bottom() )
      y = availGeom.bottom() - popupH;
    if ( y < availGeom.top() )
      y = availGeom.top();
    if ( x + popupW > availGeom.right() )
      x = availGeom.right() - popupW;
    if ( x < availGeom.left() )
      x = availGeom.left();

    // On Wayland, ensure we set the screen hint and use setGeometry
    // (sets size + position atomically, which some compositors handle better)
    m_popup->setScreen( screen );
    m_popup->setGeometry( x, y, popupW, popupH );
  }

  std::unique_ptr< TrayBackend > m_backend;
  QSystemTrayIcon *m_trayIcon = nullptr;
  QQuickView *m_popup = nullptr;
  bool m_useLayerShell = false;
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main( int argc, char *argv[] )
{
  QApplication app( argc, argv );
  app.setOrganizationName( "UniwillControlCenter" );
  app.setOrganizationDomain( "uniwill.local" );
  app.setApplicationName( "ucc-tray" );
  app.setApplicationVersion( "0.2.0" );
  app.setQuitOnLastWindowClosed( false );

  if ( !QSystemTrayIcon::isSystemTrayAvailable() )
  {
    QMessageBox::critical(
      nullptr,
      QObject::tr( "System Tray Error" ),
      QObject::tr( "No system tray detected on this system." ) );
    return 1;
  }

  TrayController controller;

  return app.exec();
}

#include "main.moc"
