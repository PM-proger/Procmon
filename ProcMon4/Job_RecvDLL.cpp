// Организация диалога получения списка DLL с сервера,
// получение результата и отображения результат на экране.

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
#include "UtilsNet_LIB.h"
#include "UtilsMisc_LIB.h"
#include "Job_RecvDLL.h"


// параметры, передаваемые в диалог приема DLL
struct RecvDataDialogParams_STRUCT {
    std::string ServerIP;               // IP-адрес сервера
    std::string ServerPage;             // страница на сервере, начинается со слэша '/'
};

// id таймера для отслеживания статуса сетевого задания
// ОТСЛЕДИТЬ - чтобы его ID не пересекался с таймерами:
//      - основного окна - IDT_TIMER1 - главная программа
//      - отслеживания получения данных - Job_SendDLL.cpp
#define IDT_TIMER_JOB_RECV      12
// собственно таймер
static UINT_PTR g_Timer_JobRecv = NULL;
NetworkJob_STRUCT g_Job_Recv;

// поток для выполнения сетевых операций
static std::thread g_Recv_Thread{};

void InitTimer_JobRecv(HWND hWnd) {
    g_Timer_JobRecv = SetTimer(hWnd, IDT_TIMER_JOB_RECV, 1000, (TIMERPROC)NULL);
    if (g_Timer_JobRecv == 0) {
        MessageBox(NULL, L"Ошибка инициализации таймера отслеживания сетевой активности.\n"
            L"Статус получения обновляться не будет.",
            NULL, 0);
    }
}

void SetServerAddr_JobRecv(const std::string& ServerIP_Recv, const std::string& ServerPage_Recv) {
    // Запоминает адрес сервера и страницу для выполнения сетевого задания по чтению.
    g_Job_Recv.Resp.ServerIP = ServerIP_Recv;
    g_Job_Recv.Resp.ServerPage = ServerPage_Recv;
}

void CancelTimer_JobRecv(HWND hWnd) {
    if (g_Timer_JobRecv != NULL) {
        KillTimer(hWnd, g_Timer_JobRecv);
        g_Timer_JobRecv = NULL;
    }
}



void StopRecvThread(bool DoStop) {
    
    g_Job_Recv.n.Stop();

    if (g_Recv_Thread.joinable()) {
        g_Recv_Thread.join();
    }
}

void DisplayData_Recv(HWND hDlg, const std::wstring& TextToDisplay) {
    // Вывод строки или др. информации в текстовое поле формы

    HWND hTextData = GetDlgItem(hDlg, IDC_DecryptedData_TEXT);
    int res = Edit_SetText(hTextData, TextToDisplay.c_str());
    int err = GetLastError();
} // void DisplayData_Recv(HWND hDlg, const std::wstring& TextToDisplay)


void DisplayJobResult_Recv(HWND hDlg, const HttpResponse_STRUCT& Resp) {
    // Вывод готового ответа сервера в диалоговое окно.
    // Вызывающая программа ДОЛЖНА обеспечить готовность данных!!!

    std::wstring Msg;

    if (g_Job_Recv.Resp.Error) {
        Msg = L"Не удалось принять сообщение. \r\nОшибка приема = ";
        //AppendStrToWStr(Msg, g_Job_Recv.Resp.ErrMsg);
        Msg.append(g_Job_Recv.Resp.ErrMsg);
        Msg.append(L"\r\n");
    }
    else {
        Msg = L"Получение данных завершено. \r\nКод ответа сервера = " +
            std::to_wstring(g_Job_Recv.Resp.ResponseCode) + L"\r\n" +
            L"Расшифрованная строка = " + g_Job_Recv.Resp.DecryptedData + L"\r\n";
    }

    DisplayData_Recv(hDlg, Msg);
} // DisplayJobResult_Recv

void UpdateRecvStatus(HWND hDlg) {
    // Обновить строку статуса
    HWND hStatusText = GetDlgItem(hDlg, IDC_RECV_STATUS_TEXT);

    if (hStatusText == NULL)
        return;

    // ------------------- асинхронный вариант --------------------
    if (g_Job_Recv.Active) {
        // сетевое задание запущено
        if (g_Job_Recv.Resp.Ready) {
            // ... и уже выполнено => можем спокойно дождаться выполнения фонового потока
            // и считать, что данные считаны из сети
            StopRecvThread(false);
            g_Job_Recv.Active = false;

            // отобразим полученные данные
            DisplayJobResult_Recv(hDlg, g_Job_Recv.Resp);
        }
    }
    // ----------------------------------------------------------------

    if (g_Job_Recv.Active) {
        Static_SetText(hStatusText, L"Статус: выполняется сетевое задание.");
    }
    else {
        Static_SetText(hStatusText, L"Статус: сетевое задание не выполняется.");
    }
}



void CancelRecvDataFromServer(HWND hDlg) {
    // Если процесс получения данных с сервера активен - прекращает его.
    // Освобождает все ресурсы, связанные с получением данных (таймер).
    // Обновляет статус.

    StopRecvThread(true);

    CancelTimer_JobRecv(hDlg);
    UpdateRecvStatus(hDlg);

}



// обертка для ReceiveTestData для работы в фоновом потоке
void ReceiveTestData_THREAD(Network* n, HttpResponse_STRUCT* Resp) {
    // Запуск сетевого чтения в отдельном потоке.

    g_Recv_Thread = std::thread{ ReceiveTestData, n, Resp };

}

bool WORKER_ReceiveDataFromServer(HWND hDlg) {
    // Вызывается из RecvDlls_Dialog_Handler.
    // Получает данные с сервера.
    // Отображает полученный перечень DLL на экране.

    // асинхронный вариант
    if (g_Recv_Thread.joinable()) {
        // Фоновый поток еще работает - этого не должно быть!!!
        MessageBox(hDlg, L"ОШИБКА - предыдущий фоновый поток сетевого чтения не остановлен.\n"
                        L"Остановите его для продолжения !!!" , NULL, 0);
        return false;
    }

    //std::wstring Msg = L"Сейчас будет получен ответ от сервера\n"
    //    "Убедитесь, что CAPSLOCK отжат - его включение прервет работу с сетью";
    //MessageBox(hDlg, Msg.c_str(), NULL, 0);

    std::string NetworkError = "";     // если строка останется пустой - ошибки не было
    try {
        // важен порядок - далее если Active == true И Ready == true, то задание выполнено!!!
        g_Job_Recv.Resp.SetNotReady();
        g_Job_Recv.Active = true;
        UpdateRecvStatus(hDlg);

        // ----------- асинхронный вариант - с потоком -----------
        ReceiveTestData_THREAD(&g_Job_Recv.n, &g_Job_Recv.Resp);
        // контроль выполнения - по таймеру -> IDT_TIMER_JOB_RECV
        //---------------------------------------------------------

        // ............ синхронный вариант .............
        //ReceiveTestData(&Job_Recv.n, &Job_Recv.Resp);
        //Job_Recv.Active = false;
        //CancelRecvDataFromServer(hDlg);
        // .............................................

    }
    catch (const NetworkErrorException& ne) {
        NetworkError = "ERROR: ";

        NetworkError.append(ne.what());

        int ErrCode = ne.GetWinsockErrorCode();
        if (ErrCode != 0) {
            NetworkError.append("  Winsock Err. code  = ");
            NetworkError.append(std::to_string(ErrCode));
        }

        // выводим ошибку на месте расшифрованных данных
        g_Job_Recv.Resp.DecryptedData.append(NetworkError.begin(), NetworkError.end());


        CancelRecvDataFromServer(hDlg);
    }
    catch (...) {
        NetworkError = "ERROR: Unknown error.";

        // выводим ошибку на месте расшифрованных данных
        g_Job_Recv.Resp.DecryptedData.append(NetworkError.begin(), NetworkError.end());

        CancelRecvDataFromServer(hDlg);
    }


} // bool WORKER_ReceiveDataFromServer(HWND hDlg) {




INT_PTR CALLBACK RecvDlls_Dialog_Handler(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    // Обработчик событий диалога получения DLL с сервера.
    // Отображает перечень DLL, полученных с сервера.
    // Позволяет получить перечень DLL, отменить получение.

    // Принимает LPARAM -> RecvDataDialogParams_STRUCT:
    //          - адрес сервера и страница на сервере

    UNREFERENCED_PARAMETER(lParam);
    switch (message) {

        case WM_INITDIALOG: {
            
            // Запоминаем параметры
            RecvDataDialogParams_STRUCT* rddp_ptr = (RecvDataDialogParams_STRUCT*)lParam;

            // ------ В контролах EDIT запоминаем адрес сервера и страницу ------
            HWND hServerIP = GetDlgItem(hDlg, IDC_SERVER_IP_RECV);
            int res_addr = SetWindowTextA(hServerIP, rddp_ptr->ServerIP.c_str());
            if (res_addr == 0)
                MessageBoxErr(L"Ошибка установки текста (адрес сервера) в SetWindowTextA");

            HWND hServerPage = GetDlgItem(hDlg, IDC_SERVER_PAGE_RECV);
            int res_page = SetWindowTextA(hServerPage, rddp_ptr->ServerPage.c_str());
            if (res_page == 0)
                MessageBoxErr(L"Ошибка установки текста (страница сервера) в SetWindowTextA");
            // -------------------------------------------------------------------

            // В контроле EDIT отображаем расшифрованные данные с сервера.
            // Пока этих данных нет.

            DisplayData_Recv(hDlg, L"Данные с сервера еще не получены.");

            return (INT_PTR)TRUE;
        }

        case WM_CLOSE: {
            if (g_Job_Recv.Active) {
                // завершить сетевую активность
                bool CancelRecv = Yes(hDlg, L"Сетевое задание (получение от сервера) не завершено\n"
                    L"Прервать задание принудительно? (может потребовать некоторое время)");
                if (CancelRecv) {
                    CancelRecvDataFromServer(hDlg);
                }

            }

            break;
        }

        case WM_QUIT: {
            // завершить сетевую активность
            CancelRecvDataFromServer(hDlg);
            break;
        }

        case WM_TIMER: {
            if (wParam == IDT_TIMER_JOB_RECV) {
                // обновить статус,
                // для асинхронного варианта - если задание по получению из сети
                // выполнено, отобразить соотв. статус.
                UpdateRecvStatus(hDlg);
            }
        }

        case WM_COMMAND: {

            WORD ButtonId = LOWORD(wParam);

            switch (ButtonId) {
            case IDC_RecvFromServer: {
                // Получить данные с сервера

                if (g_Job_Recv.Active) {
                    MessageBox(hDlg, L"Сетевое задание выполняется.\n"
                        L"Необходимо дождаться его завершения для начала нового задания.", NULL, 0);
                }
                else {
                    // Получим IP и страницу из диалога
                    std::string ServerIP_Dlg;
                    std::string ServerPage_Dlg;
                    int res_ip = GetDialogString127(hDlg, IDC_SERVER_IP_RECV, ServerIP_Dlg);
                    int res_page = GetDialogString127(hDlg, IDC_SERVER_PAGE_RECV, ServerPage_Dlg);

                    if (res_ip != 0)
                        MessageBoxErr(L"Некорректный формат IP-адреса (допустимы только символы ASCII (с кодами 32..127)) или ошибка получения данных из диалога");
                    else if (res_page != 0)
                        MessageBoxErr(L"Некорректный формат адреса страницы (допустимы только символы ASCII (с кодами 32..127)) или ошибка получения данных из диалога");
                    else {
                        // ввод принят - начинаем сетевую процедуру
                        SetServerAddr_JobRecv(ServerIP_Dlg, ServerPage_Dlg);
                        InitTimer_JobRecv(hDlg);
                        DisplayData_Recv(hDlg, L"Ожидаем ответ от сервера...");

                        bool ok = WORKER_ReceiveDataFromServer(hDlg);
                    }

    
                }

                break;

            }
            case IDC_CancelRecv: {
                // Прервать получение данных с сервера
                CancelRecvDataFromServer(hDlg);
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
    } // switch

    return (INT_PTR)FALSE;
}



void ReceiveDataFromServer(const WindowsParams_RECV_STRUCT& w) {
    // Вызывается из главной программы.
    // Получает перечень DLL с сервера.
    // Отображает полученный перечень DLL на экране.

    // ВХОДНЫЕ ПАРАМЕТРЫ:
    //      - hInst - экземпляр основной программы, 
    //      - hMainWnd - основное окно программы, 


    RecvDataDialogParams_STRUCT RecvDlgParams;
    RecvDlgParams.ServerIP = w.ServerIP;
    RecvDlgParams.ServerPage = w.ServerPage;

    // Cоздаем диалог для получения перечня DLL
    // с параметрами - RecvDlgParams
    DialogBoxParam(w.hInst, MAKEINTRESOURCE(IDD_RecvDLLs_Dialog), w.hMainWnd, 
                    RecvDlls_Dialog_Handler, (LPARAM)&RecvDlgParams);

    // получение данных на сервер будет осуществлять: RecvDlls_Dialog_Handler
    // посредством вызова -> WORKER_ReceiveDataFromServer

}

