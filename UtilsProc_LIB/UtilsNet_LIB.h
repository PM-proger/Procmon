
/*
Функции и класс для работы с сетью.

	Класс Network реализует основные ф-ции работы с сетью:
		- CreateConnection - создает сокет и устанавливает соединение,
		- Receive - получает сетевые данные и сохраняет во внутреннем буфере,
		- GetData - возвращает данные из буфера, прочитанные посредством Receive,
		- Send - отправляет данные на сервер,
		- Close - закрывает сокет.
	Большинство ф-ций возвращают ошибки путем бросания исключений.

	Класс NetworkErrorException - используется для передачи информации об
	ошибках при работе с сетью (в частности, хранит код ошибки winsock).

	В этом же файле определены адрес сервера и параметры запроса.

	Адрес сервера и страницы заданы константами.
*/

#pragma once

#include <winsock2.h>
#include <string>
#include <stdexcept>
#include <atomic>


static const int ServerPortNumber = 80;


class NetworkErrorException : public std::exception {
	int ErrCode = 0;					// код ошибки winsock (0 - не сетевая ошибка)
	std::wstring ErrMsg;
	std::wstring WinsockErrMsg;
public: 
	NetworkErrorException(const std::wstring& in_ErrMsg, int in_WinsockErrCode/*TODO- это удалить!!!*/);

	int GetWinsockErrorCode() const;

	std::wstring GetWinsockErrMsg() const;
	std::wstring GetErrMsg() const;

}; // class NetworkErrorException


// Итоговая информация об ответе сервера.
// Заполняется в: SendTestData

struct HttpResponse_STRUCT {
	// Адрес и страница сервера - должны быть обязательно заполнены !!!
	std::string ServerIP;
	std::string ServerPage;

	// информация о разобранных строках в ответе сервера
	std::string Response = "";		// весь ответ
	int ResponseCode = -1;
	std::string Rid = "";
	std::string Content = "";
	std::string Status = "";
	std::string DataString = "";
	std::wstring DecryptedData = L""; // актуально только для ответа
	std::string _dont_use_DecryptedDataNarrow = ""; // уже неактуально
	
	// --- флаги готовности --- 
	// Используются для контроля готовности данных - если Ready == true,
	// все остальные данные могут считываться.
	// Мьютексы/крит. секции в данном случае не используем,
	// ТОЛЬКО ДЛЯ УПРОЩЕНИЯ КОДА.

	std::atomic<bool> Ready = false;		// ответ готов, (возможно, с ошибкой)
	std::atomic<bool> Error = false;		// ошибка (актуально, если )
	std::wstring ErrMsg = L"";				// текст ошибки - актуален, если Error = true

	void SetNotReady() {
		// Обработка начата и не завершена
		Ready = false;
		Error = false;
		ErrMsg.clear();
	}

	void SetReady() {
		// Обработка завершена без ошибок
		Ready = true;
		Error = false;
		ErrMsg.clear();
	}

	void SetError(const std::wstring& in_ErrMsg) {
		// Обработка завершена с ошибкой
		Ready = true;
		Error = true;
		ErrMsg = in_ErrMsg;
	}


};

class Network {
	SOCKET sn;
	bool NonBlockingMode = false;	// режим сокета: блокирующий или нет
	// --- внутренний буфер для чтения данных ---
	// Заполняется в this->Receive()
	// Выбирается (только полностью!!!) в this->GetData()
	char* TempBufPtr = nullptr;		// буфер для чтения сетевых данных
	int TempBufSize = 0;			// текущий размер буфера
	int TempBufSizeUsed = 0;		// кол-во невыбранных байт
	// -------------------------------------------
	bool StopOperation = false;		// установить в true для досрочного прекращения 
									// чтения/передачи данных (в т.ч. исп-ся фоновым потоком
									// для досрочной остановки

	sockaddr_in SocketAddr;

public:

	Network() throw (NetworkErrorException);
	~Network();

	void CreateConnection(const std::string& IPAddressString) throw (NetworkErrorException);

	int Receive() throw (NetworkErrorException);
	int GetData(std::string& out_DataString, bool AppendToString);

	int Send(const std::string& StringToSend) throw (NetworkErrorException);
	int Send(const char* BytesToSendPtr, int NBytesToSend) throw (NetworkErrorException);

	bool DataReady() noexcept;

	void Close() noexcept;

	void Stop() noexcept;
	void Reset() noexcept;

private:
	void SetSocketTimeouts() throw (NetworkErrorException);

	void Log(const std::string& LogMessage, bool AddNewLine = true) noexcept;
};


struct NetworkJob_STRUCT {
	bool Active = false;            // сетевое задание выполняется или нет
	Network n;
	std::string StringToSend;
	HttpResponse_STRUCT Resp;		// ответ сервера
};


// -------- формирование текста запроса POST -----------
std::string Form_POST_request(const std::string& HostIP, 
								const std::string& ReqParams, 
								const std::string& Data);
// Формирует и возвращает текст запроса POST в виед строки.
//	- HostIP - строка для заголовка "Host: " в запросе POST,
//	- ReqPage - адрес страницы (без IP !!!) - указывается в параметре POST
//	- Data - буфер для передачи данных в формате JSON,

// -------- инициализация winsock -----------
int InitWinsockDLL();
int ShutdownWinsockDLL();

// ------------- Выполнение задачи - отправка на сервер целиком -------------
void SendTestData(Network* n, const std::wstring TestString,
						HttpResponse_STRUCT* Resp) noexcept;
	// Отправляет тестовые данные на сервер и получает ответ.
	// Входные параметры:
	//		- n - сетевой адаптер,
	//		- TestString - строка для отправки (нешифрованная)
	// Возвращает ответ в виде структуры Resp, в которой распарсены типовые значения.

void ReceiveTestData(Network* n, HttpResponse_STRUCT* Resp) noexcept;
	// Получает данные с сервера тестовые данные и получает ответ.
	// Корректность указателей ( != nulptr ) НЕ ПРОВЕРЯЕТСЯ.
	// Входные параметры:
	//		- n - сетевой адаптер.
	// Возвращает ответ в виде структуры Resp, в которой распарсены типовые значения.
