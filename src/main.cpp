/* Copyright (C) 2004-2020 Robert Griebl. All rights reserved.
**
** This file is part of BrickStore.
**
** This file may be distributed and/or modified under the terms of the GNU
** General Public License version 2 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://fsf.org/licensing/licenses/gpl.html for GPL licensing information.
*/
#include <cstdio>
#include <cstring>

#include <QMessageBox>
#include <QThreadPool>

#include "application.h"

// needed for themed common controls (e.g. file open dialogs)
#if defined(Q_CC_MSVC)
#  pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#if defined(STATIC_QT_BUILD) // this should be needed for linking against a static Qt, but it works without
#include <QtPlugin>
Q_IMPORT_PLUGIN(qjpeg)
Q_IMPORT_PLUGIN(qgif)
#endif

int main(int argc, char **argv)
{
    bool rebuild_db = false;
    bool skip_download = false;
    bool show_usage = false;

    if ((argc == 2) && (!strcmp(argv [1], "-h") || !strcmp(argv [1], "--help"))) {
        show_usage = true;
    }
    else if ((argc >= 2) && !strcmp(argv [1], "--rebuild-database")) {
        rebuild_db = true;
        if ((argc == 3) && !strcmp(argv[2], "--skip-download"))
            skip_download = true;
        else if (argc >= 3)
            show_usage = true;
    }

#if defined(SANITIZER_ENABLED)
    QThreadPool::globalInstance()->setExpiryTimeout(0);
#endif

    if (show_usage) {
#if defined(Q_OS_WINDOWS)
        QApplication a(argc, argv);
        QMessageBox::information(nullptr, QLatin1String("BrickStore"), QLatin1String("<b>Usage:</b><br />BrickStore.exe [&lt;files&gt;]<br /><br />BrickStore.exe --rebuild-database [--skip-download]<br />"));
#else
        printf("Usage: %s [<files>]\n", argv [0]);
        printf("       %s --rebuild-database [--skip-download]\n", argv [0]);
#endif
        return 1;
    }
    else {
        Application a(rebuild_db, skip_download, argc, argv);
        auto ret = qApp->exec();

        // we are using QThreadStorage in thread-pool threads, so we have to make sure they are
        // all joined before destroying the static QThreadStorage objects.
        QThreadPool::globalInstance()->clear();
        QThreadPool::globalInstance()->waitForDone();

        return ret;
    }
}

