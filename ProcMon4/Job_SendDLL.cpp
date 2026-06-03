// Организация диалога отправки списка DLL на сервер.
// Получение результата и отображения результат на экране.

#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include <winuser.h>        // DialogBox
//#include <commCctrl>        // DialogBox

#include <windowsx.h>       // макросы ListBox_AddString и т.п.
#include <vector>
#include <string>
#include <assert.h>
#include <thread>

#include "resource.h"
#include "UtilsWin_LIB.h"
#include "UtilsProc_LIB.h"
#include "UtilsNet_LIB.h"
#include "UtilsMisc_LIB.h"
#include "Job_SendDLL.h"


// параметры, передаваемые в диалог отправки DLL
struct SendDataDialogParams_STRUCT {
    std::string ServerIP;               // IP-адрес сервера
    std::string ServerPage;             // страница на сервере, начинается со слэша '/'
    ModulesDescription_STRUCT md;
    //WStrings_ARRAY1 DLLNames;
    std::wstring DLLNamesString; // перечень DLL, скленных в 1 строку
};

// id таймера для отслеживания статуса сетевого задания
// ОТСЛЕДИТЬ - чтобы его ID не пересекался с таймерами:
//      - основного окна - IDT_TIMER1 - главная программа
//      - отслеживания получения данных - Job_RecvDLL.cpp
#define IDT_TIMER_JOB_SEND      11
// собственно таймер
static UINT_PTR g_Timer_JobSend = NULL;
NetworkJob_STRUCT g_Job_Send;
std::wstring g_DllsListString = L"";      // для упрощения - сохраним полученный перечень в глою переменной

// поток для выполнения сетевых операций
static std::thread Send_Thread{};

void InitTimer_JobSend(HWND hWnd) {
    g_Timer_JobSend = SetTimer(hWnd, IDT_TIMER_JOB_SEND, 1000, (TIMERPROC)NULL);
    if (g_Timer_JobSend == 0) {
        MessageBox(NULL, L"Ошибка инициализации таймера отслеживания сети.\n"
            L"Статус отправки обновляться не будет.",
            NULL, 0);
    }
}

void SetServerAddr_JobSend(const std::string& ServerIP_Send, const std::string& ServerPage_Send) {
    // Запоминает адрес сервера и страницу для выполнения сетевого задания по чтению.
    g_Job_Send.Resp.ServerIP = ServerIP_Send;
    g_Job_Send.Resp.ServerPage = ServerPage_Send;
}

void CancelTimer_JobSend(HWND hWnd) {
    if (g_Timer_JobSend != NULL) {
        KillTimer(hWnd, g_Timer_JobSend);
        g_Timer_JobSend = NULL;
    }
}

void FillListBox(HWND hListBoxWnd, SendDataDialogParams_STRUCT* sddp_ptr) {
    // Заполняет ListBox перечнем DLL

    const int NListBoxItems = 20;

    if (sddp_ptr == nullptr) {
        int res_add = ListBox_AddString(hListBoxWnd, L"ERROR: no items passed to the dialog");
    }
    else {
        for (int i = 0; i < sddp_ptr->md.size(); i++) {
            int res_add = ListBox_AddString(hListBoxWnd, (*sddp_ptr).md[i].Name.c_str());
        }
    }
}


void DisplayData_Send(HWND hDlg, const std::wstring& TextToDisplay) {
    // Вывод строки или др. информации в текстовое поле формы (EDIT)

    HWND hTextData = GetDlgItem(hDlg, IDC_SendDLLs_TEXT);
    int res = Edit_SetText(hTextData, TextToDisplay.c_str());
    int err = GetLastError();
} // void DisplayData(HWND hDlg, const std::wstring& TextToDisplay)


void DisplayJobResult_Send(HWND hDlg, const HttpResponse_STRUCT& Resp) {
    // Вывод готового ответа сервера в диалоговое окно.
    // Вызывающая программа ДОЛЖНА обеспечить готовность данных!!!

    std::wstring Msg;

    if (g_Job_Send.Resp.Error) {
        Msg = L"Не удалось передать сообщение. \r\nОшибка передвчи = ";
        //AppendStrToWStr(Msg, g_Job_Send.Resp.ErrMsg);
        Msg.append(g_Job_Send.Resp.ErrMsg);
        Msg.append(L"\r\n");
    }
    else {
        Msg = L"Отправка данных завершена. \r\nКод ответа сервера = " +
            std::to_wstring(g_Job_Send.Resp.ResponseCode) + L"\r\n";
    }

    DisplayData_Send(hDlg, Msg);
} // DisplayJobResult_Send



void StopSendThread(bool DoStop) {
    g_Job_Send.n.Stop();

    if (Send_Thread.joinable()) {
        Send_Thread.join();
    }
}


void UpdateSendStatus(HWND hDlg) {
    // Обновляет строку статуса.

    HWND hStatusText = GetDlgItem(hDlg, IDC_SEND_STATUS_TEXT);

    if (hStatusText == NULL)
        return;

    // ------------------- асинхронный вариант --------------------
    if (g_Job_Send.Active) {
        // сетевое задание запущено
        if (g_Job_Send.Resp.Ready) {
            // ... и уже выполнено => можем спокойно дождаться выполнения фонового потока
            // и считать, что данные считаны из сети
            StopSendThread(false);
            g_Job_Send.Active = false;

            // отобразим полученные данные
            DisplayJobResult_Send(hDlg, g_Job_Send.Resp);
        }
    }
    // ----------------------------------------------------------------

    if (g_Job_Send.Active) {
        Static_SetText(hStatusText, L"Статус: выполняется сетевое задание.");
    }
    else {
        Static_SetText(hStatusText, L"Статус: сетевое задание не выполняется.");
    }

}

// обертка для SendTestData для работы в фоновом потоке
void SendTestData_THREAD(Network* n, std::wstring StringToSend, HttpResponse_STRUCT* Resp) {
    // Запуск сетевого чтения в отдельном потоке.

    Send_Thread = std::thread{ SendTestData, n, StringToSend, Resp };

}

void CancelSendDataToServer(HWND hDlg) {
    // Если процесс передачи активен - прекращает его.
    // Освобождает все ресурсы, связанные с передачей (таймер).
    // Обновляет статус.

    StopSendThread(true);

    CancelTimer_JobSend(hDlg);
    UpdateSendStatus(hDlg);

}


bool WORKER_SendDataToServer(HWND hDlg, HWND hListBoxWnd) {

    // асинхронный вариант
    if (Send_Thread.joinable()) {
        // Фоновый поток еще работает - этого не должно быть!!!
        MessageBox(hDlg, L"ОШИБКА - предыдущий фоновый поток сетевой передачи не остановлен.\n"
            L"Остановите его для продолжения !!!", NULL, 0);
        return false;
    }

    std::string NetworkError = "";     // если строка останется пустой - ошибки не было

    try {
        // важен порядок - далее если Active == true И Ready == true, то задание выполнено!!!
        g_Job_Send.Resp.SetNotReady();
        g_Job_Send.Active = true;
        UpdateSendStatus(hDlg);

        std::wstring StrToSend = g_DllsListString; //  L"широкая=DLL1--DLL3";

        // ------------- асинхронный вариант - с потоком -----------------
        SendTestData_THREAD(&g_Job_Send.n, StrToSend, &g_Job_Send.Resp);
        // контроль выполнения - по таймеру -> IDT_TIMER_JOB_SEND
        //----------------------------------------------------------------

        // .................. синхронный вариант ....................
        // SendTestData(&Job_Send.n, &StrToSend, &Job_Send.Resp);
        //Job_Send.Active = false;
        //UpdateSendStatus(hDlg);
        // ...........................................................
    }
    catch (const NetworkErrorException& ne) {
        NetworkError = "ERROR: ";

        NetworkError.append(ne.what());

        int ErrCode = ne.GetWinsockErrorCode();
        if (ErrCode != 0) {
            NetworkError.append("  Winsock Err. code  = ");
            NetworkError.append(std::to_string(ErrCode));
        }
        CancelSendDataToServer(hDlg);

    }
    catch (...) {
        NetworkError = "ERROR: Unknown error.";
        CancelTimer_JobSend(hDlg);
        CancelSendDataToServer(hDlg);
    }

    UpdateSendStatus(hDlg);

    /* -------------- это синхронный вариант ---------------------
    std::wstring Msg;
    if (NetworkError.size() > 0) {
        // ошибка работы с сетью или что-то другое
        Msg = L"Сетевая ошибка\n";
        Msg.append(NetworkError.begin(), NetworkError.end());
    }
    else {
        Msg = L"Отправка завершена. Код ответа сервера = ";
        Msg.append(std::to_wstring(Job_Send.Resp.ResponseCode));
        Msg.append(L"  Статус HTTP = ");
        Msg.append(Job_Send.Resp.Status.begin(), Job_Send.Resp.Status.end());
        MessageBox(hDlg, Msg.c_str(), NULL, 0);

    }
    */

    return true;
}



INT_PTR CALLBACK SendDlls_Dialog_Handler(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    // Обработчик событий диалога отправки DLL на сервер.
    // Отображает перечень DLL (ListBox).
    // Позволяет отправить перечень на сервер или отменить отправку.

    // Принимает LPARAM -> SendDataDialogParams_STRUCT:
    //          - перечень DLL в виде массива и единой строке,
    //          - адрес сервера и страница на сервере

    UNREFERENCED_PARAMETER(lParam);
    switch (message) {

        case WM_INITDIALOG: {
            // Заполнить ListBox -> IDC_DLLsList
            HWND hListBoxWnd = GetDlgItem(hDlg, IDC_DLLsList);

            SendDataDialogParams_STRUCT* sddp_ptr = (SendDataDialogParams_STRUCT*) lParam;

            FillListBox(hListBoxWnd, sddp_ptr);

            // ES_WANTRETURN - Enter разрывает строку,
            // \r\n - перевод строки

            // ------ В контролах EDIT запоминаем адрес сервера и страницу ------
            HWND hServerIP = GetDlgItem(hDlg, IDC_SERVER_IP_SEND);
            int res_addr = SetWindowTextA(hServerIP, sddp_ptr->ServerIP.c_str());
            if (res_addr == 0)
                MessageBoxErr(L"Ошибка установки текста (адрес сервера) в SetWindowTextA");

            HWND hServerPage = GetDlgItem(hDlg, IDC_SERVER_PAGE_SEND);
            int res_page = SetWindowTextA(hServerPage, sddp_ptr->ServerPage.c_str());
            if (res_page == 0)
                MessageBoxErr(L"Ошибка установки текста (страница сервера) в SetWindowTextA");
            // -------------------------------------------------------------------

            // в контроле EDIT отображаем склеенную строку, ее можно редактировать,
            // но на сервер будет отправлена ИСХОДНАЯ строка
            HWND hTextData = GetDlgItem(hDlg, IDC_SendDLLs_TEXT);
            int res = Edit_SetText(hTextData, sddp_ptr->DLLNamesString.c_str());

            // сохраняем для сетевой отправки далее (глоб. перем. для упрощ. доступа)
            g_DllsListString = sddp_ptr->DLLNamesString;

            int err = GetLastError();
            return (INT_PTR)TRUE;
        }

        case WM_QUIT: {
            // завершить сетевую активность
            CancelSendDataToServer(hDlg);
            break;
        }
              
        case WM_TIMER: {
            if (wParam == IDT_TIMER_JOB_SEND) {
                // обновить статус
                // для асинхронного варианта - если задание по получению из сети
                // выполнено, отобразить соотв. статус.
                UpdateSendStatus(hDlg);
            }

            break;
        }

        case WM_CLOSE: {
            if (g_Job_Send.Active) {
                // завершить сетевую активность
                bool CancelSend = Yes(hDlg, L"Сетевое задание (отправка на сервер) не завершено.\n"
                    L"Прервать задание принудительно? (может потребовать некоторое время)");
                if (CancelSend) {
                    CancelSendDataToServer(hDlg);
                }
            }

            break;
        }

        case WM_COMMAND:

            WORD ButtonId = LOWORD(wParam);

            switch (ButtonId) {
            case IDC_SendToServer: {
                // Отправить на сервер

                if (g_Job_Send.Active) {
                    MessageBox(hDlg, L"Сетевое задание выполняется.\n"
                            L"Необходимо дождаться его завершения для начала нового задания.", NULL, 0);
                }
                else {
                    std::string ServerIP_Dlg;
                    std::string ServerPage_Dlg;
                    int res_ip = GetDialogString127(hDlg, IDC_SERVER_IP_SEND, ServerIP_Dlg);
                    int res_page = GetDialogString127(hDlg, IDC_SERVER_PAGE_SEND, ServerPage_Dlg);

                    if (res_ip != 0)
                        MessageBoxErr(L"Некорректный формат IP-адреса (допустимы только символы ASCII (с кодами 32..127)) или ошибка получения данных из диалога");
                    else if (res_page != 0)
                        MessageBoxErr(L"Некорректный формат адреса страницы (допустимы только символы ASCII (с кодами 32..127)) или ошибка получения данных из диалога");
                    else {
                        HWND hListBoxWnd = GetDlgItem(hDlg, IDC_DLLsList);
                        InitTimer_JobSend(hDlg);
                        SetServerAddr_JobSend(ServerIP_Dlg, ServerPage_Dlg);
                        bool ok = WORKER_SendDataToServer(hDlg, hListBoxWnd);
                    }

                }

                return (INT_PTR)TRUE;
                break;
            }
            case IDC_CancelSend: {
                // Прервать отправку данных на сервер
                CancelSendDataToServer(hDlg);
                return (INT_PTR)TRUE;
                break;
            }
            case IDOK: {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            case IDCANCEL: {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            default: {
                break;

            }
        } // switch (ButtonId) {

    } // case WM_COMMAND:
    return (INT_PTR)FALSE;
}


void SendDLLsToServer(const WindowsParams_SEND_STRUCT& w) {
    // Вызывается из главной программы.
    // Получает перечень DLL текущего выделенного процесса.
    // Отображает перечень DLL на экране.
    // Отправляет перечень на сервер (предварительно запрашивается разрешение
    //      пользователя.
    // ВХОДНЫЕ ПАРАМЕТРЫ:
    //      - hInst - экземпляр основной программы, 
    //      - hMainWnd - основное окно программы, 
    //      - hWndListView - окно ListView с перечнем процессов

    int ItemIndex = -2;
    LPARAM PId = -2;
    std::wstring Msg;

    GetSelItemResult_ENUM res = GetSelectedItem(w.hWndListView, ItemIndex, PId);
    if (res == GetSelItemResult_ENUM::Selected1) {
        // получаем DLL-ки процесса с PID = ItemData

        // параметры для отображения диалога: массив имен DLL + то же, скленное в 1 строку
        SendDataDialogParams_STRUCT SendDlgParams;          
        SendDlgParams.ServerIP = w.ServerIP;
        SendDlgParams.ServerPage = w.ServerPage;

        //ModulesDescription_STRUCT md;
        int ErrCode = GetProcessModulesOfProcess(PId, SendDlgParams.md);

        if (ErrCode == 0) {
            // успешно - вывод на экран
            Msg = L"DLLs list: \n";
            GetModulesInfo(9999, SendDlgParams.md, SendDlgParams.DLLNamesString);

            // создаем диалог для отправки перечня DLL
            // с параметрами - SendDlgParams
            DialogBoxParam(w.hInst, MAKEINTRESOURCE(IDD_SendDLLs_Dialog), w.hMainWnd, 
                                SendDlls_Dialog_Handler, (LPARAM) &SendDlgParams);

            // отправку данных на сервер будет осуществлять: SendDlls_Dialog_Handler
            // посредством вызова -> SendDataToServer
        }
        else {
            // Ошибка
            Msg = L"Can't get DLLs list of process = " + std::to_wstring(PId) +
                L". ErrCode = " + std::to_wstring(ErrCode);
            MessageBox(w.hMainWnd, Msg.c_str(), NULL, 0);
        }
    }
    else if (res == GetSelItemResult_ENUM::SelectedSeveral)
        MessageBox(w.hMainWnd, L"Выделены несколько строк - не поддерживается.", NULL, 0);
    else if (res == GetSelItemResult_ENUM::Selected0) {
        // ни одна строка не выделена
        MessageBox(w.hMainWnd, L"Сначала необходимо выделить строку с процессом.", NULL, 0);
    }
    else
        MessageBox(w.hMainWnd, L"Ошибка получения текущей строки списка процессов.", NULL, 0);

}