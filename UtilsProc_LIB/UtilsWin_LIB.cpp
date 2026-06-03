

#include "UtilsWin_LIB.h"

#include <commctrl.h>
#include <tchar.h>          // _T

void WindowToTray(HWND hWnd, HICON hIcon, NOTIFYICONDATA& Tray, UINT CallBackMessage) {
    // Свернуть окно в трей.
    //      - CallBackMessage - каким сообщением трей информирует главное окно,
        
    ShowWindow(hWnd, 0);

    memset(&Tray, 0, sizeof(Tray));

    Tray.cbSize = sizeof(Tray);
    Tray.hIcon = hIcon;
    Tray.hWnd = hWnd;
    wcscpy_s(Tray.szTip, _countof(Tray.szTip), L"ProcMon4");

    Tray.uCallbackMessage = CallBackMessage;
    Tray.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    Tray.uID = 1;

    // Добавляем иконку в трей
    Shell_NotifyIcon(NIM_ADD, &Tray);
}

void WindowFromTray(HWND hWnd, NOTIFYICONDATA& Tray) {
    // Развернуть окно, убрать из трея.

    // Удаляем иконку из трея
    Shell_NotifyIcon(NIM_DELETE, &Tray);
    ShowWindow(hWnd, 1);
}

bool Yes(HWND hMainWnd, const std::wstring& Msg) {
    // Выдает диалог (да/нет) и возвращает true, если выбран ответ = "да",
    // или false в других случаях.

    int Result = MessageBox(hMainWnd, Msg.c_str(), NULL,
        MB_YESNO | MB_DEFBUTTON1 | MB_APPLMODAL);

    return (Result == IDYES);
}

bool IsUserAdmin(bool& IsAdmin) {
    // Возвращаемое значение: true/false - успешно или ошибка,
    // Если успешно, то IsAdmin содержит резулоьтат проверки
 
    bool res = false;

    BOOL b;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;
    b = AllocateAndInitializeSid(
        &NtAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &AdministratorsGroup);

    if (b) {

        BOOL IsMember;
        b = CheckTokenMembership(NULL, AdministratorsGroup, &IsMember);
        if (b != 0) {
            IsAdmin = (IsMember == TRUE);
            res = true;
        }
        FreeSid(AdministratorsGroup);
    }

    return res;
}


bool SetAdminPriv(LPCWSTR lpFile, LPCWSTR lpParameters) {
    // Повысить привилегии.
    // спец. аргумент, чтобы не проверялась 2-ая копию при перезапуске
    bool ok = false;
    HINSTANCE Res = ShellExecute(nullptr, _T("runas"),
        lpFile, lpParameters,
        nullptr, SW_NORMAL);
    if ((int)Res <= 32) {
        // Произошла ошибка
        ok = false;
    }
    else {
        ok = true;
    }

    return ok;
}


GetSelItemResult_ENUM GetSelectedItem(HWND hWndListView, int& out_ItemIndex, LPARAM& out_ItemData) {
    // Поиск текущей выделенной строки ListView и возврат индекса и данных (PID).
    GetSelItemResult_ENUM res = GetSelItemResult_ENUM::Error;

    out_ItemIndex = -1;
    out_ItemData = -1;
    int iPos = ListView_GetNextItem(hWndListView, -1, LVNI_SELECTED);

    if (iPos < 0) {
        res = GetSelItemResult_ENUM::Selected0;
    }
    else {
        int iPosNext = ListView_GetNextItem(hWndListView, iPos, LVNI_SELECTED);
        if (iPosNext == -1) {
            // Выделена 1 строка - обработаем ее
            LVITEM ItemToSearch{ 0 };

            ItemToSearch.mask = LVIF_PARAM; // здесь прячется id процесса
            ItemToSearch.iItem = iPos;
            ItemToSearch.iSubItem = 0;

            BOOL res_get = ListView_GetItem(hWndListView, &ItemToSearch);
            if (res_get == TRUE) {
                // обработка: ItemData = ItemToSearch.lParam == id процесса !!!
                out_ItemIndex = ItemToSearch.iItem;
                out_ItemData = ItemToSearch.lParam;

                res = GetSelItemResult_ENUM::Selected1;
            }
            else
                res = GetSelItemResult_ENUM::Error;

        }
        else {
            // выделены несколько строк - не поддерживается
            out_ItemIndex = -99;
            res = GetSelItemResult_ENUM::SelectedSeveral;
        }

    } //  if (iPos == -1) {

    return res;
}



// Максимальная длина строки
#define MAX_ADDRESS_STRING_LENGTH_IN_GDS127 80

int GetDialogString127(HWND hDlg, int FieldID, std::string& out_String) {
    // Возвращает строку ввода из диалога, если коды всех символов >= 32 <= 127.
    // ВЫХОДНЫЕ ПАРАМЕТРЫ:
    //      out_String - строка ввода (если результат ф-ции == 0).
    // ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:
    //      0  - успешно (out_String содержит строку ввода),
    //      -1 - ошибка (вероятно - недостаточно буыера (не раскрывается),
    //      -2 - ошибка (встретились символы с недопустимыми кодами),

    char Buf[MAX_ADDRESS_STRING_LENGTH_IN_GDS127];

    static_assert(sizeof(Buf[0]) == 1, "Must be char's array!!!");
    UINT CharsCount = GetDlgItemTextA(hDlg, FieldID, (LPSTR)&Buf, sizeof(Buf) / sizeof(Buf[0]));
    if (CharsCount == 0)
        return -1;

    for (int i = 0; i < CharsCount; i++) {
        char ch = Buf[i];
        if ((ch < 32) || (ch > 127))
            return -2;
    }

    out_String.assign((const char*)&Buf, CharsCount);
    return 0;
}

// Сообщения об ошибках
void MessageBoxErr(const wchar_t* ErrMsg) {
    // Выдает окно с сообщением об ошибке + код ошибки GetLastError, если он ненулевой.

    int ErrCode = GetLastError();

    if (ErrCode == 0)
        MessageBoxW(NULL, ErrMsg, L"ОШИБКА", 0);
    else {
        std::wstring ErrWithCode = ErrMsg;
        ErrWithCode.append(L"\n");
        ErrWithCode.append(L"Код ошибки = " + std::to_wstring(ErrCode));

        MessageBoxW(NULL, ErrWithCode.c_str(), L"ОШИБКА", 0);
    }
}

