/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "fileprovidersettingscontroller.h"

#include <QQmlApplicationEngine>

#include "gui/systray.h"

// Objective-C settings implementation

#import <Foundation/Foundation.h>

@interface FileProviderSettings : NSObject

@property (readonly) NSUserDefaults *userDefaults;

@end

@implementation FileProviderSettings

- (instancetype)init
{
    self = [super init];
    if (self) {
        _userDefaults = NSUserDefaults.standardUserDefaults;
    }
    return self;
}

@end

// End of Objective-C settings implementation

namespace {
constexpr auto fpSettingsQmlPath = "qrc:/qml/src/gui/macOS/ui/FileProviderSettings.qml";

// FileProviderSettingsPage properties -- make sure they match up in QML file!
constexpr auto fpSettingsControllerProp = "FileProviderSettingsController";
} // namespace

namespace OCC {

namespace Mac {

Q_LOGGING_CATEGORY(lcFileProviderSettingsController,
                   "nextcloud.gui.mac.fileprovider.settingscontroller")

FileProviderSettingsController::FileProviderSettingsController(QObject *parent)
    : QObject{parent}
{
    _settingsViewWidget = std::make_unique<QQuickWidget>(Systray::instance()->trayEngine(), nullptr);
    _settingsViewWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    _settingsViewWidget->setSource(QUrl(fpSettingsQmlPath));
    _settingsViewWidget->rootContext()->setContextProperty(fpSettingsControllerProp, this);
}

QQuickWidget *FileProviderSettingsController::settingsViewWidget()
{
    return _settingsViewWidget.get();
}

} // namespace Mac

} // namespace OCC
