#include "app/ui/dialogs/NativeFolderPicker.h"

#include <QDir>
#include <QPointer>
#include <QScopeGuard>
#include <QWindow>

#include <windows.h>
#include <shobjidl.h>

namespace miacode::ui {

void showNativeFolders(QObject* lifetime, QObject* parentWindow, const QString& title,
                       const QUrl& startFolder, UiRequestService::NativeFolderResult onResolved)
{
    const QPointer<QObject> guard(lifetime);
    QList<QUrl> urls;
    const auto choose = [&]() -> HRESULT {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(initialized)) return initialized;
        const auto uninitialize = qScopeGuard([] { CoUninitialize(); });
        IFileOpenDialog* dialog = nullptr;
        HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(&dialog));
        if (FAILED(result)) return result;
        const auto releaseDialog = qScopeGuard([dialog] { dialog->Release(); });
        FILEOPENDIALOGOPTIONS options;
        if (FAILED(result = dialog->GetOptions(&options))) return result;
        if (FAILED(result = dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_ALLOWMULTISELECT
                                               | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST))) return result;
        dialog->SetTitle(title.toStdWString().c_str());
        if (startFolder.isLocalFile()) {
            IShellItem* folder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(
                    QDir::toNativeSeparators(startFolder.toLocalFile()).toStdWString().c_str(),
                    nullptr, IID_PPV_ARGS(&folder)))) {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }
        const auto cancellation = QObject::connect(lifetime, &QObject::destroyed,
            [dialog] { dialog->Close(HRESULT_FROM_WIN32(ERROR_CANCELLED)); });
        QWindow* window = qobject_cast<QWindow*>(parentWindow);
        result = dialog->Show(window ? reinterpret_cast<HWND>(window->winId()) : nullptr);
        QObject::disconnect(cancellation);
        if (FAILED(result)) return result;
        IShellItemArray* items = nullptr;
        if (FAILED(result = dialog->GetResults(&items))) return result;
        const auto releaseItems = qScopeGuard([items] { items->Release(); });
        DWORD count = 0;
        if (FAILED(result = items->GetCount(&count))) return result;
        for (DWORD i = 0; i < count; ++i) {
            IShellItem* item = nullptr;
            if (FAILED(result = items->GetItemAt(i, &item))) return result;
            const auto releaseItem = qScopeGuard([item] { item->Release(); });
            PWSTR path = nullptr;
            if (FAILED(result = item->GetDisplayName(SIGDN_FILESYSPATH, &path))) return result;
            urls.append(QUrl::fromLocalFile(QString::fromWCharArray(path)));
            CoTaskMemFree(path);
        }
        return S_OK;
    };
    const HRESULT result = choose();
    if (!guard) return;
    if (SUCCEEDED(result)) onResolved(urls, {});
    else if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) onResolved({}, {});
    else onResolved({}, QStringLiteral("IFileOpenDialog: 0x%1").arg(quint32(result), 8, 16, QLatin1Char('0')));
}

} // namespace miacode::ui
