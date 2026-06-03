#pragma once


// Организация диалога отправки списка DLL на сервер.
// Получение результата и отображения результат на экране.

/* идентификаторы окна отправки DLL
IDD_SendDLLs_Dialog - диалог отправки DLL (окно)
IDC_DLLsList        - список DLL (ListBox)
IDC_SendDLLs_TEXT   - список DLL в виде строки для последующего шифрования (EDIT)
IDC_SendToServer    - кнопка отправить (Button)
IDC_CancelSend		- кнопка отмены отправки
IDC_STATUS_TEXT		- строка статуса (static text)
*/


// параметры окон, передаваемые из головной программы
struct WindowsParams_SEND_STRUCT {
	std::string ServerIP;
	std::string ServerPage;
	HINSTANCE hInst = NULL;
	HWND hMainWnd = NULL;
	HWND hWndListView = NULL;
};

void SendDLLsToServer(const WindowsParams_SEND_STRUCT& w);
	//      - hInst - экземпляр основной программы, 
	//      - hMainWnd - основное окно программы, 
	//      - hWndListView - окно ListView с перечнем процессов