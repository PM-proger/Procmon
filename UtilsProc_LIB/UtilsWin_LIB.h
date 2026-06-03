#pragma once

// Вспомогательные процедуры, основанные на WinAPI (GUI, системные и т.п.)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>			// notify icon
#include <string>

//456
void WindowToTray(HWND hWnd, HICON hIcon, NOTIFYICONDATA& Tray, UINT CallBackMessage);
	// Свернуть окно в трей.

void WindowFromTray(HWND hWnd, NOTIFYICONDATA& Tray);
	// Развернуть окно, убрать из трея.

bool IsUserAdmin(bool& IsAdmin);
	// Возвращает true при наличии прав админа.

bool SetAdminPriv(LPCWSTR lpFile, LPCWSTR lpParameters);

bool Yes(HWND hMainWnd, const std::wstring& Msg);
    // Выдает диалог (да/нет) и возвращает true, если выбран ответ = "да",
    // или false в других случаях.

// ---- работа с ListView ----------
enum class GetSelItemResult_ENUM {
    Error,
    Selected1,
    Selected0,
    SelectedSeveral
};

GetSelItemResult_ENUM GetSelectedItem(HWND hWndListView, int& out_ItemIndex, LPARAM& out_ItemData);


int GetDialogString127(HWND hDlg, int FieldID, std::string& out_String);
    // Возвращает строку ввода из диалога, если коды всех символов >= 32 <= 127.
    // ВЫХОДНЫЕ ПАРАМЕТРЫ:
    //      out_String - строка ввода (если результат ф-ции == 0).
    // ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:
    //      0  - успешно (out_String содержит строку ввода),
    //      -1 - ошибка (вероятно - недостаточно буыера (не раскрывается),
    //      -2 - ошибка (встретились символы с недопустимыми кодами),

// Сообщения об ошибке
void MessageBoxErr(const wchar_t* ErrMsg);
	// Выдает окно с сообщением об ошибке + код ошибки GetLastError, если он ненулевой.

