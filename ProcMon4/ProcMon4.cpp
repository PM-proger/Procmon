


// IP-адрес сервера и параметр для POST (страница), порт = 80 (UtilsNet_LIB.h)
// Реальный IP не раскрывается (принадлежит работодателю)
#define SERVER_IP               "000.000.000.000"
#define PAGE_FOR_POST_REQUEST   "/p/applicants.php"


// генерировать сигнал таймер для автоматического обновления списка ?
// (список можно обновить и вручную)
#define UseTimer
// автообновление экрана
#define AutoUpdateListView
// доступность сетевых функций
#define UseNetwork

// минимальные размер окна в процентах от экрана в целом
#define MIN_WINDOW_WIDTH    50
#define MIN_WINDOW_HEIGHT   50

// Отступ от верхнего края главного окна до начала списка процессов (ListView)
#define LIST_VIEW_UP_INDENT     50

// ProcList.cpp : Определяет точку входа для приложения.
//
#pragma comment( lib, "comctl32" )

#include "framework.h"



#include "ProcMon4.h"
#include <assert.h>
#include <vector>
#include <string>

#include "UtilsProc_LIB.h"    
#include "UtilsMisc_LIB.h"
#include "UtilsWin_LIB.h"      // трей

#include "Job_SendDLL.h"      // процедуры отправки DLL (включая диалог отправки)
#include "Job_RecvDLL.h"      // процедуры получения с сервера DLL (включая диалог получения)


#ifdef UseNetwork
#include "UtilsNet_LIB.h"
#endif

#define MAX_LOADSTRING 100

// описание кнопок
#define ID_BUTTON_TO_TRAY           1
#define ID_BUTTON_RESTART_AS_ADMIN  2
#define ID_KILL_PROCESS             3
#define ID_SEND_DLL                 4
#define ID_UPDATE_SCREEN            5
#define ID_RECEIVE_FROM_SERVER      6
// id таймера
#define IDT_TIMER1 1

// контекстное меню иконки в трее
#define IDM_START_T                     33000
#define IDM_RESTORE_MAIN_WINDOW_TRAY    IDM_START_T + 1
#define IDM_EXIT_PROGRAM_TRAY           IDM_START_T + 2

// события нотификации из трея
#define WM_TRAY_NOTIFY                  WM_USER + 0

struct ButtonDescription_STRUCT {
    std::wstring Caption;
    int ButtonID;
    std::wstring TooltipText;
};
// массив описателей кнопки
using ButtonDescriptions_ARRAY = std::vector<ButtonDescription_STRUCT>;

// Перечень кнопок (создаются в InitInstance)
ButtonDescriptions_ARRAY ButtonsArray = {
    {L"To Tray", ID_BUTTON_TO_TRAY, L"Свернуть в трей"},
    {L"Admin", ID_BUTTON_RESTART_AS_ADMIN, L"Повысить права до администратора"},
    {L"Kill pr.", ID_KILL_PROCESS, L"Завершить процесс"},
    {L"SendDLL", ID_SEND_DLL, L"Сформировать перечень DLL и направить его на сервер"},
    {L"Upd.Scr.", ID_UPDATE_SCREEN, L"Обновить экран"},
    {L"Recv.Srvr", ID_RECEIVE_FROM_SERVER, L"Получить перечень DLL с сервера"}
};

// Глобальные переменные:

static std::string g_ServerIP = SERVER_IP;      // только IP без http...
static std::string g_ServerPage = PAGE_FOR_POST_REQUEST;    // параметр для POST (начинается со слэша!!!)
// Признак ошибки при инициализации winsock
bool g_WinsockInitError = false;
HINSTANCE g_hInst;                                // текущий экземпляр
// главное окно:
static HWND g_hMainWnd = NULL;
// список (ListView):
static HWND g_hWndListView = NULL;
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна
// ............. для трея ....................
bool g_WindowInTray = false;              // true => в трее
NOTIFYICONDATA g_WindowTray;             // актуально если WindowInTray == true
HICON hTrayIcon = NULL;
std::wstring ProgramFile = L"";         // exe-файл программы
// ...........................................
// именованный мьютекс для предотращения 2-ого запуска:
static LPCWSTR MutexName = L"My_Unique_Mutex_NAME_3623237";
HANDLE hMyMutex = NULL;
bool MutexCreationError = false;    // при создании мьютекса произошла ошибка - повторно не создает его !!!
// специальный аргумент, которые временно отменяет проверку перезапуска
const wchar_t* g_Arg1_NoCheckInstance2 = L"no_check_2";
// --------- таймер для обновения списка процессов и захвата мьютекса ---------
UINT_PTR g_NewTimer = 0;


//static ProcessesList_CLASS d; // данные для  ListView сразу
static ProcessesList_CLASS d; // данные для  ListView сразу

// Отправить объявления функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
// --------------------------
HWND CreateListView(HINSTANCE hInstance, HWND hwndParent);


//BOOL InsertNewListViewItem(const std::vector<std::wstring>& ItemTexts);


//void HandleWM_NOTIFY(LPARAM lParam);
//bool HandleWM_COMMAND(WPARAM wParam, LPARAM lParam);
//bool HandleWM_TRAYNOTIFY(LPARAM lParam);
//BOOL InsertColumns(HWND hWndListView, ProcessesList_CLASS& lvd);



BOOL InsertColumns(HWND hWndListView, ProcessesList_CLASS& lvd) {

    LVCOLUMN LvCol;
    memset(&LvCol, 0, sizeof(LvCol));
    LvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    LvCol.cx = 0x28;                                // width between each coloum

    int NHeaders = lvd.GetHeadersCount();
    for (int HdrIndex = 0; HdrIndex < NHeaders; HdrIndex++) {
        const std::wstring& HdrText = lvd.GetHeader(HdrIndex);
        LvCol.pszText = (LPWSTR)HdrText.c_str();                     // First Header
        LRESULT res1 = SendMessage(hWndListView, LVM_INSERTCOLUMN, HdrIndex, (LPARAM)&LvCol); // Insert/Show the coloum

        // здесь правим ширину вручную (более правильно было вынести в lvd... )
        int HeaderWidth = HdrIndex == 0 ? 60 : LVSCW_AUTOSIZE_USEHEADER;
        ListView_SetColumnWidth(hWndListView, HdrIndex, HeaderWidth);
    }

    return TRUE;

} // BOOL InsertColumns(HWND hWndListView, ProcessesList_CLASS& lvd) {

//debug
#define TRACE_UPDATE_ITEMS_no

BOOL UpdateListViewItems(ProcessesList_CLASS& lvd) {
    // Добавляет к ListView все записи (строки, а в них - items и subitems)
    LVITEM lvI = { 0 };

    lvI.mask = LVIF_TEXT | LVIF_STATE | LVIF_PARAM;
    lvI.stateMask = 0;
    lvI.iSubItem = 0;
    lvI.state = 0;

    const int NItemsInLV_AtStart = ListView_GetItemCount(g_hWndListView);   // столько строк в ListView в начале
    int TotalItemsUpdate = lvd.GetItemsCount(); // информацию о таком кол-ве элементов заносим в ListView

    //int NItemsToAdd = lvd.GetItemsCount();   // сколько СТРОК всего добавляем
    //int NItemsTotal = NItemsInLV + NItemsToAdd;
    int NItemsTotal = ListView_GetItemCount(g_hWndListView);

    //DebugOutput(L"  Total on screen = " + std::to_wstring(NItemsTotal) + L"\n");
    //DebugOutput(L"  Total in list = " + std::to_wstring(TotalItemsUpdate) + L"\n");

    int CurListViewItemIndex = -12;
    for (int Cur_LV_ItemIndex = 0; Cur_LV_ItemIndex < TotalItemsUpdate; Cur_LV_ItemIndex++) {
        bool DoUpdate = (Cur_LV_ItemIndex < NItemsInLV_AtStart);
        // DoUpdate == true - обновим существующую запись,
        // DoUpdate == false - добавим новую

        // получам item из таблицы процессов + доп. данные ItemData (PID)
        LPARAM ItemData;
        const std::wstring& ItemText = lvd.GetItem(Cur_LV_ItemIndex, 0, ItemData);

        // ------ общие х-ки добавляемого/обновляемого элемента -------
        lvI.mask = LVIF_TEXT | LVIF_PARAM;
        lvI.lParam = ItemData;
        lvI.pszText = (LPWSTR)ItemText.c_str();
        lvI.iSubItem = 0;
        
        // ------------------------------------------------------------

        CurListViewItemIndex = -11;
        if (DoUpdate) {
            // обновить сущ-ую запись
            lvI.iItem = Cur_LV_ItemIndex;
            lvI.iImage = Cur_LV_ItemIndex;
            BOOL ok_set = CurListViewItemIndex = ListView_SetItem(g_hWndListView, &lvI);
            //assert(ok_set == TRUE);

            CurListViewItemIndex = Cur_LV_ItemIndex;

            //DebugOutput(L"[" + std::to_wstring(Cur_LV_ItemIndex) + L"]: update\n");
        }
        else {
            // добавляем новую запись
            // заведомо большие номера, чтобы ListView сам определил индекс
            lvI.iItem = 999999;
            lvI.iImage = 999999;
            BOOL ok_insert = CurListViewItemIndex = ListView_InsertItem(g_hWndListView, &lvI);
            //assert(ok_insert == TRUE);
            //DebugOutput(L"[" + std::to_wstring(ListViewNewItemIndex) + L"]:      __ADDED__\n");
        }

        if (CurListViewItemIndex < 0) {
            // индекс элемента остался отрицательным - неизвестная ошибка:
            MessageBoxErr(L"Ошибка обновления списка процессов в UpdateListViewItems(...)");
            return FALSE;
        }

        // устанавливаем subItems для добавленного элемента

        LVITEM lvIsub = { 0 };

        //const int NColumns = lvd.GetSubItemsCount();   // сколько столбцов в строке [ItemIndex]
        const int NColumns = lvd.GetHeadersCount();   // сколько столбцов в строке [ItemIndex]
        const int NSubItems = NColumns - 1;
        for (int SubItemIndex = 1; SubItemIndex <= NSubItems; SubItemIndex++) {
            // Initialize LVITEM members that are common to all items.

            lvIsub.mask = LVIF_TEXT;
            lvIsub.stateMask = 0;
            lvIsub.state = 0;

            // получим текст строки [ItemIndex], а в ней Item [SubItemIndex]
            LPARAM SubItemData;
            const std::wstring& SubItemText = lvd.GetItem(Cur_LV_ItemIndex, SubItemIndex, SubItemData);
            lvIsub.pszText = (LPWSTR)SubItemText.c_str();

            lvIsub.iItem = CurListViewItemIndex; // ListViewNewItemIndex;              // заведомо большой номер, чтобы система добавила в конец списка
            lvIsub.iSubItem = SubItemIndex;
            lvIsub.iImage = 0; // ListViewNewItemIndex;
            //lvIsub.lParam = SubItemData;

            #ifdef TRACE_UPDATE_ITEMS
            if (CurListViewItemIndex < 4) {
                std::wstring s;
                s = L"up: [" + std::to_wstring(CurListViewItemIndex) + L"." +
                    std::to_wstring(SubItemIndex) + L"]: " + lvIsub.pszText + L"\n";
                DebugOutput(s);
            }
            #endif

            // Insert subitems into the list.
            BOOL ok_set = ListView_SetItem(g_hWndListView, &lvIsub);
            if (ok_set != TRUE) {
                // индекс элемента остался отрицательным - неизвестная ошибка:
                MessageBoxErr(L"Ошибка ListView_SetItem в UpdateListViewItems(...)");
                return FALSE;
            }
        }

    }

    //#error - что-то здесь не так !!!

    int NItemsInListView = ListView_GetItemCount(g_hWndListView);

    // Ловим ситуацию, когда в новой версии lvd (TotalItemsUpdate) записей меньше, 
    // чем в сущ-щем ListView на экране (NItemsInListView) 
    // - это означает, что лишние записи из ListView надо удалить
    
    NItemsTotal = ListView_GetItemCount(g_hWndListView);
    int NToDelete = NItemsInListView - TotalItemsUpdate;

    if (NToDelete > 0) {
        // удаляем лишние записи с конца списка
        int EndLVIndexToDelete = NItemsTotal - 1;
        int StartLVIndexToDelete = EndLVIndexToDelete - NToDelete + 1;

        // удаляем 
        for (int i = EndLVIndexToDelete; i >= StartLVIndexToDelete; i--) {
            BOOL ok_del = ListView_DeleteItem(g_hWndListView, i);

            if (ok_del != TRUE) {
                // неизвестная ошибка:
                MessageBoxErr(L"Ошибка ListView_DeleteItem в UpdateListViewItems(...)");
                return FALSE;
            }
        }

        int NItemsTotal3 = ListView_GetItemCount(g_hWndListView);
    }


    return TRUE;
}

BOOL InsertListViewItems(ProcessesList_CLASS& lvd) {
    // Добавляет к ListView все записи (строки, а в них - items и subitems)
    LVITEM lvI = { 0 };

    // Initialize LVITEM members that are common to all items.
    //lvI.pszText ----= LPSTR_TEXTCALLBACK; // Sends an LVN_GETDISPINFO message.
    lvI.mask = LVIF_TEXT | LVIF_STATE | LVIF_PARAM;
    lvI.stateMask = 0;
    lvI.iSubItem = 0;
    lvI.state = 0;

    int NItemsInLV = ListView_GetItemCount(g_hWndListView);   // столько строк уже в ListView

    int NItemsToAdd = lvd.GetItemsCount();   // сколько СТРОК всего добавляем
    int NItemsTotal = NItemsInLV + NItemsToAdd;
    ListView_SetItemCount(g_hWndListView, NItemsTotal);

    for (int ItemIndex = 0; ItemIndex < NItemsToAdd; ItemIndex++) {
        // получам item из таблицы + доп. данные ItemData
        LPARAM ItemData;
        const std::wstring& ItemText = lvd.GetItem(ItemIndex, 0, ItemData);

        //int ListViewItemIndex = ItemIndex + NItemsInLV;
        // заведомо большие номера, чтобы ListView сам определил индекс
        lvI.iItem = 999999;
        lvI.iImage = 999999;
        lvI.lParam = ItemData;
        lvI.pszText = (LPWSTR)ItemText.c_str();

        // Insert item into the list.
        int ListViewNewItemIndex = ListView_InsertItem(g_hWndListView, &lvI);
        if (ListViewNewItemIndex < 0) {
            // неизвестная ошибка:
            MessageBoxErr(L"Ошибка добавления элемента в список процессов в InsertListViewItems(...)");
            return FALSE;
        }

        // устанавливаем subItems

        LVITEM lvIsub = { 0 };

        //const int NColumns = lvd.GetSubItemsCount();   // сколько столбцов в строке [ItemIndex]
        const int NColumns = lvd.GetHeadersCount();   // сколько столбцов в строке [ItemIndex]
        const int NSubItems = NColumns - 1;
        for (int SubItemIndex = 1; SubItemIndex <= NSubItems; SubItemIndex++) {
            // Initialize LVITEM members that are common to all items.

            lvIsub.mask = LVIF_TEXT | LVIF_STATE;
            lvIsub.stateMask = 0;
            lvIsub.state = 0;

            // получим текст строки [ItemIndex], а в ней Item [SubItemIndex]
            LPARAM SubItemData;
            const std::wstring& SubItemText = lvd.GetItem(ItemIndex, SubItemIndex, SubItemData);
            lvIsub.pszText = (LPWSTR)SubItemText.c_str();

            lvIsub.iItem = ListViewNewItemIndex;              // заведомо большой номер, чтобы система добавила в конец списка
            lvIsub.iSubItem = SubItemIndex;
            lvIsub.iImage = ListViewNewItemIndex;
            //lvIsub.lParam = SubItemData;

            // Insert items into the list.
            BOOL ok_set = ListView_SetItem(g_hWndListView, &lvIsub);
            if (ok_set != TRUE) {
                // неизвестная ошибка:
                MessageBoxErr(L"Ошибка добавления элемента в список процессов в InsertListViewItems(...)");
                return FALSE;
            }
        }

    }

    return TRUE;
}


void SelectItemByIndex(int SeletedItemIndex) {
    ListView_SetItemState(g_hWndListView, SeletedItemIndex,
        LVIS_SELECTED, LVIS_SELECTED);
    //LVIS_FOCUSED
}



void UpdateListView(ProcessesList_CLASS& lvd) {
    // Обновить экран при изменении списка процессов

    // запомним - кто сейчас выделен
    int SelectedItemIndex = 0;
    LPARAM ItemData = 0;

    GetSelItemResult_ENUM res_get = GetSelectedItem(g_hWndListView, SelectedItemIndex, ItemData);

    // запрет перерисовки на время обновления
    //SendMessage(g_hWndListView, WM_SETREDRAW, FALSE, 0);

    int TopItemIndex = ListView_GetTopIndex(g_hWndListView);
    int ItemsPerPage = ListView_GetCountPerPage(g_hWndListView);

    //ListView_DeleteAllItems(hWndListView);
    BOOL ok_update = UpdateListViewItems(lvd);
    if (ok_update != TRUE)
        return;         // при ошибке досрочно прекращаем работу

    int n = lvd.GetItemsCount();

    BOOL ok_redraw = ListView_RedrawItems(g_hWndListView, 0, lvd.GetItemsCount());

    // ListView_SetTop
    BOOL ok_ensure1 = ListView_EnsureVisible(g_hWndListView, TopItemIndex, FALSE);
    BOOL ok_ensure2 = ListView_EnsureVisible(g_hWndListView, TopItemIndex + ItemsPerPage - 1, FALSE);

    // восстанавливаем выделенный элемент
    if (res_get == GetSelItemResult_ENUM::Selected0) {
        SelectItemByIndex(SelectedItemIndex);
        GetSelItemResult_ENUM res = GetSelectedItem(g_hWndListView, SelectedItemIndex, ItemData);
    }

    // разрешаем перерисовку
    //SendMessage(g_hWndListView, WM_SETREDRAW, TRUE, 0);

}




BOOL InsertNewListViewItem(const std::vector<std::wstring>& ItemTexts) {
    // Добавляет новый элемент к уже существующим:
    //      - без textcallback,
    //      - без определения кол-ва строк

    LVITEM lvI = { 0 };

    // Initialize LVITEM members that are common to all items.
    lvI.pszText = (LPTSTR)ItemTexts[0].c_str();

    lvI.mask = LVIF_TEXT | LVIF_STATE;
    lvI.stateMask = 0;
    lvI.iSubItem = 0;
    lvI.state = 0;

    lvI.iItem = 999999;              // заведомо большой номер, чтобы система добавила в конец списка
    lvI.iImage = 999999;

    // Insert items into the list.
    int NewItemIndex = ListView_InsertItem(g_hWndListView, &lvI);
    if (NewItemIndex < 0) {
        // неизвестная ошибка:
        MessageBoxErr(L"Ошибка ListView_InsertItem в InsertNewListViewItem(...)");
        return FALSE;
    }

    // устанавливаем subItems

    LVITEM lvIsub = { 0 };

    const int NColumns = ItemTexts.size();
    const int NSubItems = NColumns - 1;
    for (int SubItemIndex = 1; SubItemIndex <= NSubItems; SubItemIndex++) {
        // Initialize LVITEM members that are common to all items.
        lvIsub.pszText = (LPTSTR)ItemTexts[SubItemIndex].c_str();
        lvIsub.mask = LVIF_TEXT | LVIF_STATE;
        lvIsub.stateMask = 0;
        lvIsub.iSubItem = SubItemIndex;
        lvIsub.state = 0;

        lvIsub.iItem = NewItemIndex;              // заведомо большой номер, чтобы система добавила в конец списка
        lvIsub.iImage = NewItemIndex;

        // Insert items into the list.
        BOOL ok_set = ListView_SetItem(g_hWndListView, &lvIsub);
        if (ok_set != TRUE) {
            // неизвестная ошибка:
            MessageBoxErr(L"Ошибка ListView_SetItem в InsertNewListViewItem(...)");
            return FALSE;
        }
    }


    return TRUE;
}



void FreeResources_Quit() {
    #ifdef UseNetwork
    ShutdownWinsockDLL();
    #endif
}

void FreeResources_WMAIN() {

    #ifdef UseTimer
    KillTimer(g_hMainWnd, g_NewTimer);
    #endif


    if (hMyMutex != NULL)
        ReleaseMutex(hMyMutex);
}


// ----- работа с мьютексом для контроля 2-ого запуска --------

#define MY_MUTEX_CREATED_OK        1
#define MY_MUTEX_ALREADY_EXISTS    2
#define MY_MUTEX_ERROR             3

int CreateNamedMutex() {

    // --- проверка существования 2-ой копии путем контроля мьютекса---
    hMyMutex = CreateMutex(NULL, TRUE, MutexName);
    int MutexErrCode = GetLastError();
    if (MutexErrCode == ERROR_ALREADY_EXISTS) {
        // мьютекс уже существует - завершаем работу
        return MY_MUTEX_ALREADY_EXISTS;
    }
    else if (hMyMutex == NULL) {
        // ошибка
        MutexCreationError = true;
        return MY_MUTEX_ERROR;
    }

    return MY_MUTEX_CREATED_OK;
}




//
//  ФУНКЦИЯ: MyRegisterClass()
//
//  ЦЕЛЬ: Регистрирует класс окна.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PROCMON4));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_PROCMON4);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    ATOM res = RegisterClassExW(&wcex);

    return res;
}



bool CreateButtons(HINSTANCE hInstance) {

    bool ok = true;
    int StartX = 0;       // x-координата очередной кнопки
    const int ButtonsGap = 10;  // расстояние между кнопками
    const int ButtonWidth = 70;
    RECT rect;
    for (const ButtonDescription_STRUCT& b : ButtonsArray) {

        HMENU ButtonID = (HMENU)b.ButtonID;
        HWND hButton = CreateWindow(L"BUTTON", b.Caption.c_str(),
            WS_VISIBLE | WS_CHILD,
            StartX, 0, ButtonWidth, 30,
            g_hMainWnd, ButtonID, g_hInst, NULL);

        if (b.ButtonID == ID_BUTTON_RESTART_AS_ADMIN) {
            // гасим кнопку перезапуска, мы уже админ
            bool IsAdmin;
            bool ok = IsUserAdmin(IsAdmin);
            if (ok) {
                if (IsAdmin) {
                    Button_Enable(hButton, FALSE);
                }
            }
            else
                MessageBox(g_hMainWnd, L"Error while checking admin status.", NULL, 0);
        }


        StartX = StartX + ButtonWidth + ButtonsGap;
        //if (GetWindowRect(hButton, &rect)) {
        //    int Width = rect.right - rect.left;
        //}

        if (hButton == NULL) {
            ok = false;
        }
        else {
            // ------ подсказка к кнопке ------

            HWND hwndTool = GetDlgItem(g_hMainWnd, b.ButtonID);

            // Create the tooltip. g_hInst is the global instance handle.
            HWND hwndTip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL,
                WS_POPUP | TTS_ALWAYSTIP,
                CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, CW_USEDEFAULT,
                hButton, NULL,
                hInstance, NULL);

            if (hwndTip == NULL) {
                // ошибка - игнорируем
            }
            else {
                // создаем tooltip к кнопке
                TOOLINFO toolInfo = { 0 };
                toolInfo.cbSize = sizeof(toolInfo);
                toolInfo.hwnd = hButton; // игнорируется если флаг = TTF_IDISHWND
                toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
                toolInfo.uId = (UINT_PTR) hButton; // (UINT_PTR)hwndTool + b.ButtonID + 20;
                toolInfo.lpszText = (LPWSTR) b.TooltipText.c_str();
                toolInfo.hinst = hInstance;
                SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
            }
            
            // --------------------------------

        }
    }

    return ok;

    //ShowWindow(hButton, SW_SHOW);
}



BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {

    g_hInst = hInstance; // Сохранить маркер экземпляра в глобальной переменной

    g_hMainWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (g_hMainWnd == NULL) {
        int ErrCode = GetLastError();
        return FALSE;
    }

    // --- панель с кнопками ---
    CreateButtons(hInstance);

    // созраняем в глоб. переменной - далее работает только с ней
    g_hWndListView = CreateListView(hInstance, g_hMainWnd);
    if (g_hWndListView == NULL)
        return FALSE;

    InsertColumns(g_hWndListView, d);

    BOOL ok = InsertListViewItems(d);
    if (ok != TRUE)
        return FALSE;

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    if (!g_hWndListView) {
        ShowWindow(g_hWndListView, SW_SHOW);
        UpdateWindow(g_hWndListView);
    }

    return TRUE;
}

void MinimizeWindowToTray(HWND hWnd) {
    // Свернуть окно в трей и уведомлять нас о событиях трея с помощью -> WM_TRAY_NOTIFY

    WindowToTray(hWnd, hTrayIcon, g_WindowTray, WM_TRAY_NOTIFY);
}


void RestoreMainWindowFromTray(HWND hWnd) {
    WindowFromTray(hWnd, g_WindowTray);
    g_WindowInTray = false;
}

bool HandleWM_COMMAND(WPARAM wParam, LPARAM lParam) {
    // Обработка WM_COMMAND.
    // ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:
    //      - true - сообщение обработано, вызов DefWindowProc не требуется.
    //      - false - требуется вызов DefWindowProc.

    bool Processed = true;

    int wmId = LOWORD(wParam);
    // Разобрать выбор в меню или нажатия кнопок:
    switch (wmId) {

        case IDM_RESTORE_MAIN_WINDOW_TRAY: {
            
            // команда иконки из трея - развернуть главное окно из трея
            RestoreMainWindowFromTray(g_hMainWnd);
            break;
        }

        case IDM_EXIT_PROGRAM_TRAY: {
            // команда иконки из трея - завершить работу программы
            PostQuitMessage(0);
            break;
        }

        case ID_KILL_PROCESS: {
            // обработать текущую запись списка по заданию.
            int ItemIndex = -2;
            LPARAM ItemData = -2;

            GetSelItemResult_ENUM res_get = GetSelectedItem(g_hWndListView, ItemIndex, ItemData);
            if (res_get == GetSelItemResult_ENUM::Selected1) {

                std::wstring Msg;
                //MessageBox(hMainWnd, Msg.c_str(), NULL, 0);

                int ErrCode = KillProcess(ItemData);
                if (ErrCode == 0) {
                    // обновим список
                    UpdateListView(d);
                }
                else {
                    Msg = L"Can't kill a process. ErrCode = " + std::to_wstring(ErrCode);
                    MessageBox(g_hMainWnd, Msg.c_str(), NULL, 0);
                }

            }
            else if (res_get == GetSelItemResult_ENUM::SelectedSeveral)
                MessageBoxW(g_hMainWnd, L"Необходимо выделить только 1 процесс для завершения.", NULL, 0);
            else if (res_get == GetSelItemResult_ENUM::Selected0)
                MessageBoxW(g_hMainWnd, L"Процесс для завершения не выделен.", NULL, 0);
            else
                MessageBoxW(g_hMainWnd, L"При получении выделенного процесса произошла ошибка.", NULL, 0);

            break;

        } //case ID_PROCESS_A_PROCESS:

        case ID_UPDATE_SCREEN: {
            d.CollectProcesses();
            UpdateListView(d);

            break;
        }

        case ID_RECEIVE_FROM_SERVER: {
            // получить данные с сервера

            if (g_WinsockInitError)
                MessageBoxW(g_hMainWnd, L"Сетевые функции недоступны.", NULL, 0);
            else {
                WindowsParams_RECV_STRUCT wpr;

                wpr.ServerIP = g_ServerIP;
                wpr.ServerPage = g_ServerPage;
                wpr.hInst = g_hInst;
                wpr.hMainWnd = g_hMainWnd;

                ReceiveDataFromServer(wpr);
            }
            break;
        }

        case ID_SEND_DLL: {
            // Сформировать перечень DLL и отправить его на сервер.

            if (g_WinsockInitError)
                MessageBoxW(g_hMainWnd, L"Сетевые функции недоступны.", NULL, 0);
            else {
                // Параметры для процедуры отправки данных
                WindowsParams_SEND_STRUCT wps;
                wps.ServerIP = g_ServerIP;
                wps.ServerPage = g_ServerPage;
                wps.hInst = g_hInst;
                wps.hMainWnd = g_hMainWnd;
                wps.hWndListView = g_hWndListView;

                SendDLLsToServer(wps);
            }
            
            break;
        }

        case ID_BUTTON_TO_TRAY: {
            // свернуться в Tray

            MinimizeWindowToTray(g_hMainWnd);
            g_WindowInTray = true;
            break;
        } // case ID_BUTTON_TO_TRAY:

        case ID_BUTTON_RESTART_AS_ADMIN: {
            // перезапустить программу с правами админа

            bool IsAdmin;
            bool ok = IsUserAdmin(IsAdmin);
            if (ok) {
                if (IsAdmin)
                    MessageBox(g_hMainWnd, L"АДМИН - НЕ ГОТОВО", NULL, 0);
                else {
                    bool ok_restart = SetAdminPriv(ProgramFile.c_str(), g_Arg1_NoCheckInstance2);
                    if (ok_restart) {
                        // перезапускаемся..., а мы завершаем работу
                        PostQuitMessage(0);
                    }
                    else {
                        // что-то пошло не так.
                        MessageBox(g_hMainWnd, L"Ошибка получения прав администратора\n"
                            L"Перезапуск отменен!", NULL, 0);
                    }
                }

            }
            else
                MessageBox(g_hMainWnd, L"Error while checking admin status.", NULL, 0);

            //RestartAsAdmin();
            break;

        } // case ID_BUTTON_RESTART_AS_ADMIN: 

        case IDM_ABOUT: {
            DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), g_hMainWnd, About);
            break;
        }

        case IDM_EXIT: {
            DestroyWindow(g_hMainWnd);
            break;
        }

        default: {
            Processed = false;
        }

    } // switch

    return Processed;
}



void ShowTrayContextMenu() {
    
    HMENU Menu = CreatePopupMenu();
    if (Menu != NULL) {

        InsertMenu(Menu, -1, MF_BYPOSITION, IDM_RESTORE_MAIN_WINDOW_TRAY, L"Восстановить");
        InsertMenu(Menu, -1, MF_SEPARATOR, -1, L"");
        InsertMenu(Menu, -1, MF_BYPOSITION, IDM_EXIT_PROGRAM_TRAY, L"Завершить");

        SetForegroundWindow(g_hMainWnd);

        POINT pt;
        GetCursorPos(&pt);
        TrackPopupMenu(Menu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_hMainWnd, NULL);
        DestroyMenu(Menu);
    }
}

void HandleWM_TRAY_NOTIFY(LPARAM lParam) {

    switch (lParam) {
        case WM_LBUTTONDBLCLK: {
            // левая кнопка - возвращает окно на место

            RestoreMainWindowFromTray(g_hMainWnd);

            break;
        } // case WM_LBUTTONDBLCLK: {

        case WM_RBUTTONDOWN: {
            // правая кнопка - контекстное меню
            ShowTrayContextMenu();
        }
    } // switch
}

void HandleWM_NOTIFY(LPARAM lParam) {

    switch (((LPNMHDR)lParam)->code) {

        case NM_CUSTOMDRAW: {

            break;

        }

        case LVN_GETDISPINFO: {

            DebugOutput(L"GET_DISPINFO without textcallback !!!");

            break;

        }

        default: {
            int r = 1;
        } 
    } // switch

    return;
}





LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {

    case WM_SIZE: {

        UINT NewWidth = LOWORD(lParam);
        UINT NewHeight = HIWORD(lParam);

        // изменяем высоту ListView
        MoveWindow(g_hWndListView, 0, 0 + LIST_VIEW_UP_INDENT, NewWidth, NewHeight - LIST_VIEW_UP_INDENT, TRUE);

        return DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    case WM_QUIT: {

        FreeResources_Quit();

        break;
    }


    case WM_DESTROY: {

        NOTIFYICONDATA g_tnd{ 0 };
        g_tnd.uFlags = 0;
        Shell_NotifyIcon(NIM_DELETE, &g_tnd);
        PostQuitMessage(0);
        break;
    }

    case WM_TRAY_NOTIFY: {
        // сообщения от трея, действительный код сообщения -> lParam

        HandleWM_TRAY_NOTIFY(lParam);

        break;
    } // case WM_TRAY_NOTIFY: {

    case WM_GETMINMAXINFO: {
        // система желает узнать мин/макс размеры
        // lParam -> MINMAXINFO structure
        MINMAXINFO* MinMax = (MINMAXINFO*)lParam;

        if (MinMax != nullptr) {
            // ограничиваем размеры окна (мин. размеры в % = MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT)

            int ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
            int ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
            int MinWidth = ScreenWidth * MIN_WINDOW_WIDTH / 100;
            int MinHeight = ScreenHeight * MIN_WINDOW_HEIGHT / 100;

            MinMax->ptMinTrackSize.x = MinWidth;
            MinMax->ptMinTrackSize.y = MinHeight;

            return 0;
        }

        break;
    } // case WM_GETMINMAXINFO: {

    case WM_TIMER: {

        #ifdef UseTimer

        if (wParam == IDT_TIMER1) {
            // сигнал таймера - обновить список процессов + восстановить мьютекс

            // --------- пробуем восстановить мьютекс ---
            // это необходимо, если перезапускались под админом и временно отключили его создание
            if (!MutexCreationError)
                CreateNamedMutex();

            #ifdef AutoUpdateListView
            d.CollectProcesses();
            UpdateListView(d);
            #endif

            return 0;
        }
        #else
        DebugOutput(L"WM_TIMER handler not defined !!!");
        #endif

        break;
    } // case WM_TIMER: {

    case WM_COMMAND: {

        bool Processed = HandleWM_COMMAND(wParam, lParam);

        if (!Processed)
            return DefWindowProc(hWnd, message, wParam, lParam);

        break;
    } // case WM_COMMAND: {

    /*
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        EndPaint(hWnd, &ps);

        break;
    }
    */


    // -----------------------
    case WM_NOTIFY: {
        HandleWM_NOTIFY(lParam);
        break;

    }

    // -----------------------

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Обработчик сообщений для окна "О программе".
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}



HWND CreateListView(HINSTANCE hInstance, HWND hwndParent) {

    RECT rcClient;
    GetClientRect(hwndParent, &rcClient);

    // Создаем окно ListView в родительском окне
    const int UpIndent = LIST_VIEW_UP_INDENT; // отступ сверху - от верха главного окна
    HMENU hMenu = NULL;
    HWND hLV = CreateWindow(WC_LISTVIEW,
        L"",
        WS_CHILD | LVS_REPORT | LVS_EDITLABELS | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL,
        0, 0 + UpIndent,
        rcClient.right - rcClient.left,
        rcClient.bottom - rcClient.top - UpIndent,
        hwndParent,
        hMenu,
        hInstance,
        NULL);

    if (hLV == NULL) {
        MessageBoxErr(L"Ошибка создания окна со списком процессом (ListView) в CreateListView");
        return NULL;
    }

    SendMessage(hLV, LVM_SETEXTENDEDLISTVIEWSTYLE,
        LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

    return (hLV);
}


#define ASSERT_TRUE(var, err_msg) if (!(var == TRUE)) { MessageBoxErr(err_msg); exit(4); }  
#define ASSERT_GE_0(var, err_msg) if (!(var > 0)) { MessageBoxErr(err_msg); exit(4); }  
#define ASSERT_NE_0(var, err_msg) if (!(var != 0)) { MessageBoxErr(err_msg); exit(4); }  

// --------------------------------------------------------------

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    int NumArgs = 0;
    auto CmdLineArgs = CommandLineToArgvW(GetCommandLine(), &NumArgs);
    ProgramFile = CmdLineArgs[0];
    std::wstring Param1 = NumArgs >= 1 ? CmdLineArgs[0] : L"";
    if (Param1 == g_Arg1_NoCheckInstance2) {
        // временно не проверяем мьютекс
    }
    else {

        int res_mutex = CreateNamedMutex();

        if (res_mutex == MY_MUTEX_ALREADY_EXISTS) {
            MutexCreationError = true;  // уже не требуется
            MessageBox(NULL, L"Program is already running.", NULL, 0);
            exit(0);
        }
        else if (res_mutex == MY_MUTEX_ERROR) {
            MessageBox(NULL, L"Ошибка инициализации мьютекса.\n"
                L"Контроль 2-ого запуска не будет выполняться.",
                NULL, 0);
        }
        else {
            //MessageBox(NULL, L"Mutex OK.", NULL, 0);
        }


        // контроль 2-ого запуска
        // --- проверка существования 2-ой копии ---
        hMyMutex = CreateMutex(NULL, TRUE, MutexName);
        if (hMyMutex == NULL) {
            // ошибка
            int MutexErrCode = GetLastError();
            if (MutexErrCode == ERROR_ALREADY_EXISTS) {
                // мьютекс уже существует - завершаем работу
                MutexCreationError = true;  // уже не требуется
                MessageBox(NULL, L"Program is already running.", NULL, 0);
                exit(0);
            }
            else {
                // какая-то другая ошибка
                MutexCreationError = true;
                MessageBox(NULL, L"Ошибка инициализации мьютекса.\n"
                    L"Контроль 2-ого запуска не будет выполняться.",
                    NULL, 0);
            }
        }

    }


    #ifdef UseNetwork
    int ErrCode = InitWinsockDLL();
    if (ErrCode != 0) {
        MessageBox(NULL, L"Ошибка инициализации сетевых функций Winsock.\nСетевые функции отключены.", NULL, 0);
        g_WinsockInitError = true;
    }
    else
        g_WinsockInitError = false;

    #else
    g_WinsockInitError = true;
    #endif

    // иконка для трея (глобальная переменная)
    hTrayIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PROCMON4));

    // Заполняем тестовые данные ListView
    // ранее - d.InitExample();

    // Собираем данные о процессах для 1-ого вывода окна процессов
    d.CollectProcesses();

    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwICC = ICC_LISTVIEW_CLASSES;
    InitCtrls.dwSize = sizeof(INITCOMMONCONTROLSEX);

    BOOL bRet = InitCommonControlsEx(&InitCtrls);
    ASSERT_TRUE(bRet, L"Ошибка инициализации 'commctrl'.");

    // Инициализация глобальных строк
    int LoadRes = 0;
    LoadRes = LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    ASSERT_GE_0(LoadRes, L"Ошибка загрузки строки APP_TITLE из ресурса.");
    LoadRes = LoadStringW(hInstance, IDC_PROCMON4, szWindowClass, MAX_LOADSTRING);
    ASSERT_GE_0(LoadRes, L"Ошибка загрузки строки IDC_PROCMON4 из ресурса.");

    ATOM res_reg = MyRegisterClass(hInstance);
    ASSERT_NE_0(res_reg, L"Ошибка регистрации класса окна (MyRegisterClass).");

    // Выполнить инициализацию приложения:
    BOOL ok_init = InitInstance(hInstance, nCmdShow);
    ASSERT_TRUE(ok_init, L"Ошибка инициализации приложения (InitInstance).");

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PROCMON4));

    MSG msg;

    #ifdef UseTimer
    g_NewTimer = SetTimer(g_hMainWnd, IDT_TIMER1, 3000, (TIMERPROC)NULL);
    if (g_NewTimer == 0) {
        MessageBox(NULL, L"Ошибка инициализации таймера\n"
            L"Автообновление процессов и захвата мьютекса контроля 2-ого запуска не будет выполняться",
            NULL, 0);
    }
    #endif

    // Цикл основного сообщения:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}
