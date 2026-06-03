#pragma once

/*
‘ункции и класс дл€ работы с сетью.

	 ласс Network реализует основные ф-ции работы с сетью:
		- CreateConnection - создает сокет и устанавливает соединение,
		- Receive - получает сетевые данные и сохран€ет во внутреннем буфере,
		- GetData - возвращает данные из буфера, прочитанные посредством Receive,
		- Send - отправл€ет данные на сервер,
		- Close - закрывает сокет.
	Ѕольшинство ф-ций возвращают ошибки путем бросани€ исключений.

	 ласс NetworkErrorException - используетс€ дл€ передачи информации об
	ошибках при работе с сетью (в частности, хранит код ошибки winsock).

	¬ этом же файле определены адрес сервера и параметры запроса.

	јдрес сервера и страницы заданы константами.
*/

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "ws2tcpip.h"	// getnameinfo
#include "UtilsMisc_LIB.h"	// json
#include "UtilsNet_LIB.h"

#include <assert.h>
#include <chrono>			// замер времени ожидани€ ответа

#pragma comment(lib, "ws2_32.lib")


// ≈сли определено, при нажатом CAPSLOCK считаетс€, что пользователь досрочно
// прервал операцию. »спользуетс€ дл€ отладки (защита от зацикливани€ в Network::Send)
#define CAPSLOCK_ABORTS_OPERATION_no

// –азмер буфера чтени€ в объекте Network - предполагаетс€, что нам не потребуетс€
// читать сообщени€, превышающее этот размер
#define TEMP_BUFFER_SIZE	100000

// таймауты в мс
#define SOCKET_READ_TIMEOUT_MS					100
#define SOCKET_WRITE_TIMEOUT_MS					100
// столько мс будем ждать в Network::Send (Sleep), если буфер передатчика полон
#define WAITING_TIME_IF_SEND_BUFFER_FULL_MS		200
// столько мс будем ждать в Network::Receive (Sleep), если данных
// ƒЋя „“≈Ќ»я ќ“—”“—“¬”ё“ (перед очередной попыткой чтени€)
#define WAITING_TIME_IF_DATA_NOT_READY_MS		200


// ----------------------- функции вне объектов ------------------------

int GetSocketStatus(SOCKET s) {

	/*
	¬озвращаемые значени€:
		WSANOTINITIALISED - A successful WSAStartup call must occur before using this function.
		WSAENETDOWN - The network subsystem has failed?
		WSAEFAULT - The name or the namelen parameter is not in a valid part of the user address space, or the namelen parameter is too small.
		WSAEINPROGRESS - A blocking Windows Sockets 1.1 call is in progress, or the service provider is still processing a callback function.
		WSAENOTCONN - The socket is not connected.
		WSAENOTSOCK - The descriptor is not a socket.
	*/

	SOCKADDR sa;
	int sa_len = sizeof(sa);

	int res = getpeername(s, &sa, &sa_len);

	if (res == SOCKET_ERROR)
		res = WSAGetLastError();

	return res;

}

int SetSocketNonBlock(SOCKET sn) {
	// устанавливаем неблокирующий режим сокета
	// *net - ссылка на объект - из него беретс€ им€ объекта
	// AddInfo - доп. информаци€ дл€ строки ошибки
	// ¬ќ«¬–јўј≈ћќ≈ «Ќј„≈Ќ»≈: 0 - нет ошибки, иначе - код ошибки
	unsigned long ul = 1;
	int ErrCode = ioctlsocket(sn, FIONBIO, &ul);


	if (ErrCode == SOCKET_ERROR) {
		//std::cout << o1 << "IOctl:" << AddInfo << " nonblocking mode set ERROR = " << ErrCode << std::endl;
		ErrCode = -1;
	}
	else {
		//std::cout << "IOctl:" << AddInfo << " nonblocking mode set succesfully.\n";
		ErrCode = 0;
	}

	return ErrCode;
}


int InitWinsockDLL() {
	// загрузка DLL winsock2
	WSAData wsData;
	WORD DLLVersion = MAKEWORD(2, 1);
	int ErrCode = WSAStartup(DLLVersion, &wsData);

	return ErrCode;
}

int ShutdownWinsockDLL() {
	// прекращение использовани€ DLL winsock2
	int ErrCode = WSACleanup();

	return ErrCode;
}

void GetIPString(const SOCKADDR_IN sock_addr, std::string& IPString) {
	// ¬џ’ќƒЌџ≈ ƒјЌЌџ≈:	строковое представление IP адреса и порта структуры <sock_addr>
	//IPString = ":" + ntohs(sock_addr.sin_port);

	char hostname[NI_MAXHOST];
	char servInfo[NI_MAXSERV];

	DWORD res = getnameinfo((struct sockaddr*)&sock_addr,
		sizeof(sock_addr),
		hostname,
		NI_MAXHOST, servInfo, NI_MAXSERV, NI_NUMERICSERV + NI_NUMERICHOST);

	IPString = hostname;
	IPString = IPString + ":" + servInfo;
}



Network::Network() throw (NetworkErrorException) {

	TempBufPtr = (char*) malloc(TEMP_BUFFER_SIZE);

	if (TempBufPtr == nullptr)
		throw std::runtime_error("Cannot allocate memory in Network::Network");

	TempBufSize = TEMP_BUFFER_SIZE;
	TempBufSizeUsed = 0;
}

Network::~Network() {
	Close();

	if (TempBufPtr != nullptr) {
		free(TempBufPtr);
		TempBufPtr = nullptr;
	}
}

void Network::Log(const std::string& LogMessage, bool AddNewLine) noexcept {
	// Ћоггирование сетевых операций.

	DebugOutput(LogMessage);
	if (AddNewLine)
		DebugOutput("\n");
}


void Network::Reset() noexcept {
	// —брос состо€ни€.
	sn = 0;
	StopOperation = false;
}

void Network::Stop() noexcept {
	// ƒосрочное прекращение всех операций
	StopOperation = true;
}

bool Network::DataReady() noexcept {
	// возвращает true, если в сокете накопились данные, которые можно считать

	bool data_ready = false;

	fd_set readfds;	// перечень сокетов дл€ получени€ статуса
	timeval select_timer;

	// таймаут = 0,5 сек
	select_timer.tv_sec = 0;
	select_timer.tv_usec = 200000; // 200 тыс. микросекунд = 0,5c

	// готовим запрос на чтение		
	FD_ZERO(&readfds);

	// (2) добавл€ем TCP-сокеты в массив отслеживани€
	FD_SET(sn, &readfds);

	// запрашиваем статус сокетов из массива (2)
	int result = select(0, &readfds, NULL, NULL, &select_timer);

	if (result == 0) {
		// таймаут исчерпан
	}
	else if (result == SOCKET_ERROR) {
		// ошибка - игнорируем
		int ErrCode = WSAGetLastError();
	}
	else {
		data_ready = FD_ISSET(sn, &readfds);
	}

	return data_ready;
}

#define ERR(msg) \
		{ \
		ErrCode = WSAGetLastError(); \
		throw NetworkErrorException(msg, ErrCode);	\
		}

void Network::SetSocketTimeouts() throw (NetworkErrorException) {

	DWORD timeout = SOCKET_READ_TIMEOUT_MS;
	int ErrCode = setsockopt(sn, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
	if (ErrCode != 0)
		ERR(L"Can't set timeout for network reading");

	timeout = SOCKET_WRITE_TIMEOUT_MS;
	ErrCode = setsockopt(sn, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
	if (ErrCode != 0)
		ERR(L"Can't set timeout for network writing");
}


#define CONN_PREFIX L"ќшибка установлени€ сетевого соединени€: "
void Network::CreateConnection(const std::string& IPAddressString) throw (NetworkErrorException) {
	// —оздаем сокет и соедин€емс€ с сервером.
	// ќшибка передаетс€ через исключени.

	int ErrCode = 0;
	sn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sn == INVALID_SOCKET)
		ERR(CONN_PREFIX L"ќшибка создани€ сокета.");

	/*
	Log("Setting socket non-blocking mode...");
	// устанавливаем неблокирующий режим сокета
	ErrCode = SetSocketNonBlock(sn);
	if (ErrCode == SOCKET_ERROR)
		ERR(CONN_PREFIX"Error setting non-blocking mode socket");

	NonBlockingMode = true;
	*/

	SetSocketTimeouts();

	// инициализируем адрес сокета TCP
	memset(&SocketAddr, 0, sizeof(SocketAddr));
	SocketAddr.sin_family = AF_INET;
	SocketAddr.sin_port = htons(ServerPortNumber);

	#define UseInetAddr
	#ifdef UseInetAddr
	
	SocketAddr.sin_addr.s_addr = inet_addr(IPAddressString.c_str());

	#else
	INT WSAAPI res = inet_pton(AF_INET, IPAddressString.c_str(), &SocketAddr);
	if (res != 1) {
		int WinsockErr = WSAGetLastError();
		throw NetworkErrorException("Address conversion error", WinsockErr);
	}
	#endif

	Log("Connecting...");
	ErrCode = connect(sn, (SOCKADDR*) &SocketAddr, sizeof(SocketAddr));

	if (ErrCode == SOCKET_ERROR)
		ERR(CONN_PREFIX L"ќшибка установлени€ соединени€");

	Log("Connection created.");

	return;
}


int Network::Receive() throw (NetworkErrorException) {
	// „тение из сокета во внутреннний буфер TempBufPtr:TempBufSize
	// до его полного заполнени€.
	// ¬ј∆Ќќ:
	//		ѕосле вызова Receive об€зателен вызов GetData().
	//		»наче последующий Receive сотрет предыдущие данные.
	// 
	// ѕравильна€ последовательность вызовов чтени€ данных и их возврата:
	//		Receive();
	//		GetData();

	// ¬ќ«¬–јўј≈ћќ≈ «Ќј„≈Ќ»≈: сколько байтов прочитано.

	// Ѕуфер дл€ чтени€:
	char* Buffer = TempBufPtr + TempBufSizeUsed;
	int MaxToReceive = max(0, TempBufSize - TempBufSizeUsed);

	if (MaxToReceive == 0)
		return 0;

	int TotalReceived = 0;

	int ErrCode = 0;	// дл€ макроса ERR

	Log("Receiving data" + std::to_string(MaxToReceive) + "...");

	bool ContinueReading = true;
	while (ContinueReading && !StopOperation) {
		// запрашиваем наличие данных
		bool Ready = DataReady();

		if (Ready) {
			// данные доступны дл€ чтени€ - считываем их
			int ErrCode = recv(sn, Buffer, MaxToReceive, NULL);

			if (ErrCode == SOCKET_ERROR)
				ERR(L"ќшибка чтени€ из сети.");

			int NReceived = ErrCode;
			//assert(NReceived <= MaxToReceive);

			Log("Received = " + std::to_string(NReceived) + ".");

			ErrCode = NReceived;

			// регистрируем поступление данных и сдвигаем указатели/счетчики
			TotalReceived += NReceived;
			TempBufSizeUsed += NReceived;
			Buffer += NReceived;
			MaxToReceive -= NReceived;

			ContinueReading = false;
		}
		else {
			// ждем по€влени€ данных
			Sleep(WAITING_TIME_IF_DATA_NOT_READY_MS);
		}

		#ifdef CAPSLOCK_ABORTS_OPERATION
		// дл€ отладки (если завиcнем в цикле)
		if (CapsIsPressed())
			StopOperation = true;
		#endif

		if (StopOperation)
			throw NetworkErrorException(L"ѕолучение данных из сервера прервано пользователем.", 0);

	}

	return TotalReceived;
}

int Network::GetData(std::string& out_DataString, bool AppendToString) {
	// ¬ыбирает ¬—≈ данные из буфера чтени€ и возвращает из в строке _DataString_.
	// ≈сли AppendToString==true, то данные добавл€еютс€ к строке,
	//		иначе предыдущие данные строки затираютс€.
	// ≈сли нет доступных данных, то возвращаетс€ 0 и DataString _не_мен€етс€_
	// ѕравильна€ последовательность вызовов чтени€ данных и их возврата:
	//		Receive();
	//		GetData();

	if (TempBufSizeUsed == 0)
		return 0;

	if (AppendToString)
		out_DataString.append(TempBufPtr, TempBufSizeUsed);
	else 
		out_DataString.assign(TempBufPtr, TempBufSizeUsed);

	int res = TempBufSizeUsed;
	TempBufSizeUsed = 0;

	return res;
}


void Network::Close() noexcept {
	// «акрывает сокет.
	Log("Closing socket _" + std::to_string (sn) + "...");

	if (sn != 0) {
		closesocket(sn);
		sn = 0;
	}

}

int Network::Send(const std::string& StringToSend) throw (NetworkErrorException) {
	return Send(StringToSend.data(), StringToSend.size());
}


int Network::Send(const char* BytesToSendPtr, int NBytesToSend) throw (NetworkErrorException) {
	// ќтправл€ет <NBytesToSend> байтов в сеть.
	// ¬ќ«¬–јўј≈ћќ≈ «Ќј„≈Ќ»≈ - фактическое количество отправленных байт
	//		или SOCKET_ERROR (в таком случае код ошибки можно получить в GetLastError)

	int TotalSent = 0;			// сколько всего байт удалось отправить

	int WinsockErrorCode = 0;	// код ошибки winsock
	int nsent = 0;				// количество фактически отправленных байт
	bool SendingError = false;	// флаг выхода по ошибке записи

	const char* CurPortionPtr = BytesToSendPtr;	// текуща€ позици€ буфера, с которой отправл€ютс€ данные
	int CurPortionSize = NBytesToSend;	// текуща€ позици€ буфера, с которой отправл€ютс€ данные

	Log("Sending total = " + std::to_string(NBytesToSend) + "...");

	while ((TotalSent < NBytesToSend) && (!StopOperation)) {
		// отправл€ем, пока все байты не будут отправлены, 

		// очередна€ попытка
		nsent = send(sn, CurPortionPtr, CurPortionSize, NULL);

		bool WinsockError = (nsent == SOCKET_ERROR);
		WinsockErrorCode = WinsockError ? WSAGetLastError() : 0;

		if (WinsockError && NonBlockingMode && (WinsockErrorCode == WSAEWOULDBLOCK))
			WinsockError = false;		// не считаем ошибкой - особенность non-blocking mode
		
		if (WinsockError) {
			if (WinsockErrorCode == WSAENOBUFS) {
				// Ѕуфер передатчика зан€т - ждем, пока он освободитс€
				Sleep(WAITING_TIME_IF_SEND_BUFFER_FULL_MS);
			}
			else {			
				// иначе запоминаем ошибку и выходим из процедуры
				throw NetworkErrorException(L"ќшибка отправки данных на сервер.", WinsockErrorCode);
			}

		}
		else {
			Log("    Sent = " + std::to_string(nsent) + ".");

			// корректируем указатель и счетчик буфера данных
			CurPortionPtr = CurPortionPtr + nsent;
			CurPortionSize = CurPortionSize - nsent;

			TotalSent = TotalSent + nsent;
		}

		#ifdef CAPSLOCK_ABORTS_OPERATION
		// дл€ отладки (если завиcнем в цикле)
		if (CapsIsPressed())
			StopOperation = true;
		#endif

		if (StopOperation)
			throw NetworkErrorException(L"ќтправка данных прервана пользователем.", 0);


	};		// while (ContinueToSend && (TotalSent < NBytesToSend) && !TimeoutOccured)

	if (StopOperation)
		throw NetworkErrorException(L"ќтправка данных прервана пользователем.", 0);

	return TotalSent;

} // Sendbytes


#define use_chars_2

char CRLF[] = { 13, 10 };

std::string Form_POST_request(const std::string& HostIP, const std::string& ReqPage, const std::string& Data) {
	// ‘ормирует и возвращает текст запроса POST в виед строки.
	//	- HostIP - строка дл€ заголовка "Host: " в запросе POST,
	//	- ReqPage - адрес страницы (без IP !!!) - указываетс€ в параметре POST
	//	- Data - буфер дл€ передачи данных в формате JSON,

	std::string Post;
	int ContentLength = Data.size();

	Post = "POST " + ReqPage + " HTTP/1.1";
	Post.append(CRLF, 2);
	Post.append("Host: ");
	Post.append(HostIP);
	Post.append(CRLF, 2);
	Post.append("Content-Type: application/json");
	Post.append(CRLF, 2);
	Post.append("Content-Length: " + std::to_string(ContentLength));
	Post.append(CRLF, 2);
	Post.append(CRLF, 2);	// заголовок завершен
	// контент в JSON
	Post.append(Data);

	return Post;
}


// ----- исключени€ при работе с сетью --------


NetworkErrorException::NetworkErrorException(const std::wstring& in_ErrMsg, int in_WinsockErrCode) :
	std::exception() {

	ErrCode = in_WinsockErrCode;
	ErrMsg = in_ErrMsg;

	if (ErrCode != 0) {

		LPTSTR ErrorMsgPtr;
		DWORD res = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, in_WinsockErrCode, 0, (LPTSTR)&ErrorMsgPtr, 0, NULL);

		if (res != 0) {
			if (res > 1000)
				WinsockErrMsg.assign(ErrorMsgPtr, 1000);
			else
				WinsockErrMsg.assign(ErrorMsgPtr);
			ErrMsg.append(L" (");
			ErrMsg.append(ErrorMsgPtr);
			ErrMsg.append(L")");
		}
		else {
			// ќшибка
			WinsockErrMsg = L"ќшибка форматировани€ сообщени€ об ошибке (FormatMessage)";
		}

		if (ErrorMsgPtr != NULL)
			LocalFree(ErrorMsgPtr);
	}

}

int NetworkErrorException::GetWinsockErrorCode() const {
	return ErrCode;
}

std::wstring NetworkErrorException::GetWinsockErrMsg() const {
	return WinsockErrMsg;
}

std::wstring NetworkErrorException::GetErrMsg() const {
	return ErrMsg;
}


auto GetTimeInMs() {
	// ?юыЇҐш™є ™хъЇ?хх т®хь†, ™ш€: std::chrono::steady_clock::time_point
	auto curTime = std::chrono::steady_clock::now();
	return curTime;
}

long long GetTimeDiff(std::chrono::steady_clock::time_point StartTime,
	std::chrono::steady_clock::time_point EndTime) {
	//auto curTime = std::chrono::steady_clock::now();
	auto elapsedTimeMcs = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);
	long long t = elapsedTimeMcs.count();
	return t;
}



// ------------- ¬ыполнение задачи - отправка на сервер целиком -------------
// —только ждем ответа от сервера (если не получаем recv(..) = 0
#define WAIT_SERVER_IN_MS	(long long) 5000

void SendTestData(Network* n, const std::wstring TestString, 
											HttpResponse_STRUCT* Resp) noexcept {
	// ќтправл€ет строку TestString на сервер и получает ответ.
	// ¬ходные параметры:
	//		- n - сетевой адаптер,
	//		- TestString - строка дл€ отправки (нешифрованна€)
	// ¬озвращает ответ в виде структуры Resp, в которой распарсены типовые значени€.

	// сбрасываем состо€ние
	try {
		Resp->SetNotReady();
		n->Reset();

		const char* s1 = (char*)TestString.data();

		// Ўифруем строку
		std::string CryptedTestString = EncryptWideString(TestString);

		const char* s2 = CryptedTestString.data();

		// ‘ормируем строку JSON в _JsonString_
		JsonString_CLASS js;

		js.JsonStart();
		js.JsonAdd("cmd", 1);
		const char* MyUID = GetUniqueID();
		js.JsonAdd("rid", MyUID, true);
		js.JsonAdd("data", CryptedTestString, true, true);
		js.JsonEnd();

		std::string JsonString = js.JsonGet();

		// ‘ормируем запрос POST
		std::string Post = Form_POST_request(Resp->ServerIP, Resp->ServerPage, JsonString);

		n->CreateConnection(Resp->ServerIP);

		n->Send(Post);

		// --- получить ответ ---
		Resp->Response.clear();		// в этой строке накапливаетс€ ответ сервера
		bool AllReceived = false;
		bool Timeout = false;
		auto StartTime = GetTimeInMs();
		while (!AllReceived && !Timeout) {
			int NBytesRead = n->Receive();
			if (NBytesRead == 0) {
				// соединение закрыто
				AllReceived = true;
			}
			else if (NBytesRead > 0) {
				n->GetData(Resp->Response, true);
				auto CurTime = GetTimeInMs();
				long long TimeElapsed = GetTimeDiff(StartTime, CurTime);
				Timeout = (TimeElapsed > WAIT_SERVER_IN_MS);
			}
		}

		n->Close();

		/*
		std::cout << "------------------ request -----------------" << std::endl;
		std::cout << Post << std::endl;
		std::cout << "------------------ response -----------------" << std::endl;
		std::cout << Response << std::endl;
		std::cout << "------------------ response -----------------" << std::endl;
		*/

		Resp->ResponseCode = 0;

		int RespParseError = ParseResponse(Resp->Response, Resp->ResponseCode, Resp->Content);

		if (RespParseError == 0) {

			/*
			std::cout << "Resp code = " << ResponseCode << std::endl;
			std::cout << "Resp content = " << Content << std::endl;
			*/

			int ExtractError = ExtractJsonValue(Resp->Content, "rid", Resp->Rid);
			//std::cout << ExtractError << "rid = " << JsonValue << std::endl;

			ExtractError = ExtractJsonValue(Resp->Content, "status", Resp->Status);
			//std::cout << ExtractError << "    status = " << JsonValue << std::endl;

			ExtractError = ExtractJsonValue(Resp->Content, "data", Resp->DataString);

		}
		else {

			//std::cout << "--------- error parsing response --------" << std::endl;
		}

		Resp->SetReady();		// нет ошибок
	}
	catch (const NetworkErrorException ne) {
		std::wstring NetworkError = L"ERROR: ";

		NetworkError.append(ne.GetErrMsg());

		int ErrCode = ne.GetWinsockErrorCode();
		if (ErrCode != 0) {
			NetworkError.append(L"  Winsock Err. code  = ");
			NetworkError.append(std::to_wstring(ErrCode));
		}
		Resp->SetError(NetworkError);
	}
	catch (...) {
		Resp->SetError(L"Unknown error while sending data to network.");
	}

	return;	// рез-то не нужен - работаем по исключени€м
} // void SendTestData(



void ReceiveTestData(Network* n, HttpResponse_STRUCT* Resp) noexcept {
	// ѕолучает данные с сервера тестовые данные и получает ответ.
	//  орректность указателей ( != nulptr ) Ќ≈ ѕ–ќ¬≈–я≈“—я.
	// ¬ходные параметры:
	//		- n - сетевой адаптер.
	// ¬озвращает ответ в виде структуры Resp, в которой распарсены типовые значени€.

	// сбрасываем состо€ние
	Resp->SetNotReady();

	try {
		n->Reset();

		// ‘ормируем строку JSON в _JsonString_
		JsonString_CLASS js;

		js.JsonStart();
		js.JsonAdd("cmd", 2);
		const char* MyUID = GetUniqueID();
		js.JsonAdd("rid", MyUID, true, true);
		js.JsonEnd();

		std::string JsonString = js.JsonGet();

		// ‘ормируем запрос POST
		std::string Post = Form_POST_request(Resp->ServerIP, Resp->ServerPage, JsonString);

		n->CreateConnection(Resp->ServerIP);

		n->Send(Post);

		// --- получить ответ ---
		// ждем, пока не получим пакет 0-вой длины (соединение закрыто или
		//    в течении WAIT_SERVER_IN_MS мс
		Resp->Response.clear();		// в этой строке накапливаетс€ ответ сервера
		bool AllReceived = false;
		bool Timeout = false;
		auto StartTime = GetTimeInMs();


		DebugOutput("Starting recv data (ReceiveTestData).");
		while (!AllReceived && !Timeout) {
			int NBytesRead = n->Receive();
			if (NBytesRead == 0) {
				// соединение закрыто
				AllReceived = true;
			}
			else if (NBytesRead > 0) {
				n->GetData(Resp->Response, true);
				auto CurTime = GetTimeInMs();
				long long TimeElapsed = GetTimeDiff(StartTime, CurTime);
				Timeout = (TimeElapsed > WAIT_SERVER_IN_MS);
			}
		}
		if (AllReceived)
			DebugOutput("Finished recv data (ok).");
		else
			DebugOutput("Finished recv data (timeout).");

		n->Close();

		/*
		std::cout << "------------------ request -----------------" << std::endl;
		std::cout << Post << std::endl;
		std::cout << "------------------ response -----------------" << std::endl;
		std::cout << Response << std::endl;
		std::cout << "------------------ response -----------------" << std::endl;
		*/

		Resp->ResponseCode = 0;

		int RespParseError = ParseResponse(Resp->Response, Resp->ResponseCode, Resp->Content);

		if (RespParseError == 0) {

			/*
			std::cout << "Resp code = " << ResponseCode << std::endl;
			std::cout << "Resp content = " << Content << std::endl;
			*/

			int ExtractError = ExtractJsonValue(Resp->Content, "rid", Resp->Rid);
			//std::cout << ExtractError << "rid = " << JsonValue << std::endl;

			ExtractError = ExtractJsonValue(Resp->Content, "status", Resp->Status);
			//std::cout << ExtractError << "    status = " << JsonValue << std::endl;

			ExtractError = ExtractJsonValue(Resp->Content, "data", Resp->DataString);

			Resp->DecryptedData = DecryptWideString(Resp->DataString);
		}
		else {

			//std::cout << "--------- error parsing response --------" << std::endl;
		}

		// дл€ контрол€ в отладчике
		const char* d1 = Resp->DataString.data();
		const char* d2 = (const char*)Resp->DecryptedData.data();

		Resp->SetReady();		// нет ошибок
	}
	catch (const NetworkErrorException ne) {
		std::wstring NetworkError = L"ERROR: ";

		NetworkError.append(ne.GetErrMsg());

		int ErrCode = ne.GetWinsockErrorCode();
		if (ErrCode != 0) {
			NetworkError.append(L"  Winsock Err. code  = ");
			NetworkError.append(std::to_wstring(ErrCode));
		}
		Resp->SetError(NetworkError);
	}
	catch (...) {
		Resp->SetError(L"Unknown error while receiving data to network.");
	}

	return;
}


/*
* ----------------------- ѕ–ќ“ќ ќЋ ќЅћ≈Ќј — —≈–¬≈–ќћ -------------------------------
ќтправка данных
Send request POST Data:
{УcmdФ: 1, УridФ: У%”Ќ» јЋ№Ќџ…_»ƒ≈Ќ“»‘» ј“ќ–%Ф, УdataФ: У%Ў»‘–ќ¬јЌЌјя_—“–ќ ј%Ф}
√де
cmd Ц команда, где 1 Ц отправить данные, 2 Ц получить данные
rid Ц уникальный идентификатор запроса
data -	 шифрованна€ строка

Response:
{УridФ: У%”Ќ» јЋ№Ќџ…_»ƒ≈Ќ“»‘» ј“ќ–%Ф, УstatusФ: УtrueФ}
√де
status Ц ответ сервера об успешном получении данных
rid Ц идентификатор запроса, используемый при отправке данных


ѕолучение данных
Send request POST Data:
{УcmdФ: 2, УridФ: У%”Ќ» јЋ№Ќџ…_»ƒ≈Ќ“»‘» ј“ќ–%Ф}
√де
cmd Ц команда, где 1 Ц отправить данные, 2 Ц получить данные
rid Ц идентификатор запроса, используемый при отправке данных

Response:
{УridФ: У%”Ќ» јЋ№Ќџ…_»ƒ≈Ќ“»‘» ј“ќ–%Ф, УdataФ: У%Ў»‘–ќ¬јЌЌјя_—“–ќ ј%Ф}
√де
rid Ц идентификатор запроса, используемый при отправке данных
data Ц шифрованна€ строка

*/