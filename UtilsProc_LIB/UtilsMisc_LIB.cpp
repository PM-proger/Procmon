
#include "UtilsMisc_LIB.h"
#include <shlwapi.h>					// GetKeyState
#include <random>					// ГСЧ
#include <assert.h>
#include <map>					// json-parser


#define CryptVersion2

// уникальная строка
#ifdef CryptVersion2
const char* MyUniqueId = "6294hal2222";
#define STATIC_CRYPTO_KEY_LEN	8
static char CryptoKeyStatic[STATIC_CRYPTO_KEY_LEN] = {89,11,168,254,17,83,41,200};
#else
const char* MyUniqueId = "787878ZXC_4";
#endif

const char* GetUniqueID() {
	return MyUniqueId;
}

// ----------- class JsonString_CLASS ------------------

void JsonString_CLASS::JsonStart() {
	JsonString.assign("{");
}

void JsonString_CLASS::JsonEnd() {
	JsonString.append("}", 1);
}

void JsonString_CLASS::JsonAdd(const std::string& JsonName, const std::string& JsonVal, 
									bool Quotes, bool FinalElement) {
	JsonAdd(JsonName, JsonVal.c_str(), Quotes, FinalElement);
}


void JsonString_CLASS::JsonAdd(const std::string& JsonName, const char* JsonVal, 
							bool Quotes, bool FinalElement) {
	// Добавляет элемент к строке JSON
	// Quotes - обрамлять ли значение кавычками,
	// FinalElement - добавлять (false) или не добавлять (true) запятую,
	JsonString.append("\"");
	JsonString.append(JsonName);

	if (Quotes)
		JsonString.append("\":\"");
	else
		JsonString.append("\":");

	JsonString.append(JsonVal);

	if (Quotes)
		JsonString.append("\"");

	if (!FinalElement)
		JsonString.append(",");

}

void JsonString_CLASS::JsonAdd(const std::string& JsonName, int JsonVal, bool FinalElement) {
	// Добавляет целое число к Json (значение - без кавычек).
	JsonAdd(JsonName, std::to_string(JsonVal), false, FinalElement);
}


std::string JsonString_CLASS::JsonGet() {
	// Возвращает сформированную строку.
	return JsonString;
}

struct Token_STRUCT {
	std::string Name;
	bool Quotes;
};

Token_STRUCT Tokens[] = {
	{"cmd:", false},
	{"rid:", true},
	{"status:", true},
	{"data:", true}
};

/* ------------------- очень простой парсер строки JSON --------------------------
Проверяется только:
1) наличие открыающей и закрывающей скобок - {}
2) наличие заранее определенных имен полей (из задания),
3) значения эти полей.

УПРОЩЕНИЕ
Все, что находится между полями не проверяется.
*/


bool JsonParser_CLASS::ParseString(const std::string& s) {
	// Разбирает строку. Помещает разобранные значения в map
	// Возвращает: true - разбор успешен, false - ошибка разбора.

	int s_size = s.size();
	if (s_size)
		return false;

	if ((s[1] != '{') || (s[s_size - 1] != '}'))
		return false;

	int CurIndex = 0;
	int NTokens = sizeof(Tokens) / sizeof(Token_STRUCT);
	for (int i = 1; i < NTokens; i++) {
		int NextNameIndex = s.find(Tokens[i].Name, CurIndex);

		if (NextNameIndex != std::string::npos) {
			int TokenLen = Tokens[i].Name.size();
			CurIndex += TokenLen;
			if (CurIndex + 2 > s_size - 1)	// минимум 2 симв. = закр. кавычка + }
				return false;

			if (Tokens[i].Quotes) {
				if (s[CurIndex] != '\"')
					return false;				// ожидалась открыв. кавычка

				// CurIndex = индекс откр. кавычки
					
				// ожидаемый ограничитель -  закр. кавычки
				int ClosingQ = s.find(Tokens[i].Name, CurIndex + 1);
				if (ClosingQ == std::string::npos)
					return false;					// закр. кавычка не найдена

				int SubstrLen = (ClosingQ - 1) - (CurIndex + 1) + 1;
				std::string TokenVal = s.substr(CurIndex + 1, SubstrLen);

				JsonVals.insert({ (Tokens[i].Name).substr(0, TokenLen - 1), TokenVal });	// отсекам ":"

			}
			else {
				// CurIndex = индекс начала значения без кавычки
					
				// значение без кавычки, ближайший ограничитель - , или }
				int ClosingCh = s.find_first_of("},", CurIndex);
				if (ClosingCh == std::string::npos)
					return false;

				int SubstrLen = (ClosingCh - 1) - (CurIndex + 1) + 1;
				std::string TokenVal = s.substr(CurIndex + 1, SubstrLen);

				JsonVals.insert({ (Tokens[i].Name).substr(0, TokenLen - 1), TokenVal });	// отсекам ":"
			}
				
		}
	}

}

std::string JsonParser_CLASS::GetJsonVal(const std::string& JsonName) {
	return JsonVals[JsonName]; //.find(JsonName);
}


/*
		CurIndex = 0;
		int NTokens = sizeof(Tokens) / sizeof(Token_STRUCT);
		for (int i = 1; i < NTokens; i++) {
			int NextName = s.find(Token[i].Name, CurIndex);

			if (Found) {
				TokenLen = strlen(Token[i].Name);
				CurPos += TokenLen;
				if (CurIndex + 2 > s_size - 1)	// минимум 2 симв. = закр. кавычка + }
					return false;

				if (Token[i].Quote) {
					if (s[CurPos] != '\"')
						return false;				// ожидалась открыв. кавычка
					// ожидаемый ограничитель -  закр. кавычки
					int ClosingQ = s.find(Token[i].Name from CurPos);
					if (ClosingQ == std::string::npos)
						return;					// закр. кавычка не найдена

					взять от CurPos до ClosingQ
					продвинуть CurPos за кавычку
				}
				else {
					// значение без кавычки, ближайший ограничитель - , или }
					int ClosingCh = s.find(Token[i].Name from CurPos из мн-ва символов);
					if (not found)
						return false;
					взять от CurPos до ClosingQ
					продвинуть CurPos за кавычку
					if (огр = })
						продолжить поискж
				}

			}
		}

	}
};
*/

// --------------- разное ------------------

bool CapsIsPressed() noexcept {
	// Возвращает статус CAPSLOCK: true - нажата, false - не нажата
	SHORT res = GetKeyState(VK_CAPITAL);	// 16-bit
	bool Pressed = res > 0;// (res & 0x8000) != 0;
	return Pressed;
};

// преобразованной зашифрованной строки в URI


char HexLetter(char byte_val, bool LowNibble) {
	// Возвращает шестнадцатиричное представление старшего или младшего ниббла
	unsigned char letter, v;
	if (LowNibble)
		v = (byte_val) & 0xF;
	else
		v = (byte_val >> 4) & 0xF;
	letter = (v <= 9) ? (v + '0') : (v - 10 + 'A');
	return letter;
}




bool ConvertFromUriToBytes(const std::string& in, std::string& out) {
	// Вариант для исходной строки std::string.
	// Преобразует поток символов в URI в поток символов без кодировки URI.
	// Входной параметр:
	//      - in - строка std::string в кодировке URI,
	//      - out - _in_, в которой каждая последовательность %xx заменена одним символом.
	// ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:
	//		- true - конвертация успешна,
	//		- false - при конвертации произошла ошибка.

	out.clear();
	int CurIndex = 0;
	int MaxIndex = in.size() - 1;

	char CurCh, Hex1, Hex2;
	char CurByte;
	wchar_t CurChW;

	while (CurIndex <= MaxIndex) {
		char CurCh = in[CurIndex];
		if (CurCh == '%') {
			// сканируем hex-код
			if (CurIndex + 2 > MaxIndex)
				return false; // нет двух символов далее

			Hex1 = in[++CurIndex];
			Hex2 = in[++CurIndex];

			if ((Hex2 >= '0') && (Hex2 <= '9'))
				CurByte = Hex2 - '0';
			else if ((Hex2 >= 'A') && (Hex2 <= 'F'))
				CurByte = 10 + Hex2 - 'A';
			else if ((Hex2 >= 'a') && (Hex2 <= 'f'))
				CurByte = 10 + Hex2 - 'a';
			else
				return false;

			if ((Hex1 >= '0') && (Hex1 <= '9'))
				CurByte |= (Hex1 - '0') << 4;
			else if ((Hex1 >= 'A') && (Hex1 <= 'F'))
				CurByte |= (10 + Hex1 - 'A') << 4;
			else if ((Hex1 >= 'a') && (Hex1 <= 'f'))
				CurByte |= (10 + Hex1 - 'a') << 4;
			else
				return false;

			CurIndex++;

		}
		else {
			// просто копируем
			CurByte = CurCh;
			CurIndex++;
		}

		// Здесь CurByte - распарсенный байт, CurIndex -- следующий за ним символ

		out.append(&CurByte, 1);
	}

	return true;
}


bool ConvertFromBytesToUri(const std::string& in, std::string& out) {
	// Преобразует поток символов _in_ в строку URI.
	// При этом _каждый_ символ кодируется в формате (%hh)
	// Входной параметр:
	//      - in - поток байт,
	//      - out - выходная строка в формате URI.
	// ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:
	//		- true - конвертация успешна,
	//		- false - ошибка (недостаточно памяти).

	out.clear();

	int InSize = in.size();
	int RequiredSize = InSize * 3;
	bool ok_reserve;
	try {
		out.reserve(RequiredSize);
		ok_reserve = true;
	}
	catch (...) {
		ok_reserve = false;
	}

	if (!ok_reserve)
		return false;

	char CurCh;
	char HexByte[4] = { '%', 0, 0, 0 };

	for (int CurIndex = 0; CurIndex < InSize; CurIndex++) {
		CurCh = in[CurIndex];

		if ((CurCh >= 'a' && CurCh <= 'z') ||
			(CurCh >= 'A' && CurCh <= 'Z') ||
			(CurCh >= '0' && CurCh <= '9') ||
			(CurCh == '-') || (CurCh == '.') || (CurCh == '_') || (CurCh == '~')
			) {
			// этот символ мы не кодируем в процентах
			out.append(1, CurCh);
		}
		else {
			// заворачиваем в проценты
			HexByte[1] = HexLetter(CurCh, false);
			HexByte[2] = HexLetter(CurCh, true);

			out.append(HexByte, 3);
		}
	} // for

	return true;
}


// --------------- шифрование строки -----------------
/*
Способ шифрования:
новый символ = исходный xor случаный_ключ xor позиция символа
Т.е. одна и та же строка будет кодироваться каждый раз по-новому.
*/
const char KeyChar = 0;

// размер ключа
#define CRYPTO_KEY_LEN (int) 2

std::string EncryptString(const std::string& SourceString) noexcept {
	// Зашифровывает строку.
	// Результат: зашифрованная строка.
	return EncryptBuffer(SourceString.data(), SourceString.size());
}


#ifdef CryptVersion2
void XorString(std::string& StringToXor, const char* KeyToXor, int KeyToXorSize) {
	// XOR-им строку с буфером KeyToXor:KeyToXorSize
	int KeyIndex = 0;
	for (int i = 0; i < StringToXor.size(); i++) {
		char KeyChar = KeyToXor[KeyIndex];
		StringToXor[i] = StringToXor[i] ^ KeyChar;
		KeyIndex++;
		if (KeyIndex > KeyToXorSize - 1)
			KeyIndex = 0;
	}
}
#endif

std::string EncryptBuffer(const char* SourceBufPtr, int SourceBufSize) noexcept {
	// Зашифровывает произвольный буфер SourceBufPtr:SourceBufSize.
	// Размер буфера (SourceBufSize) задается в байтах.
	// Результат: зашифрованная строка.

	#ifdef DoEncrypt
	char CryptoKey[CRYPTO_KEY_LEN];

	// Генерируем случайный ключ:
	GenerateRandomBytes(CryptoKey, sizeof(CryptoKey));

	// -------------------------------------------------------

	std::string res(SourceBufPtr, SourceBufSize);
	//= SourceString;

	int StrSize = res.size();
	int KeyIndex = 0;
	for (int i = 0; i < res.size(); i++) {
		char KeyChar = CryptoKey[KeyIndex];
		res[i] = res[i] ^ KeyChar ^ ((char) i & 255);
		KeyIndex++;
		if (KeyIndex > CRYPTO_KEY_LEN - 1)
			KeyIndex = 0;
	}

	// дописываем ключ в конец строки
	res.append(CryptoKey, CRYPTO_KEY_LEN);

	#ifdef CryptVersion2
	// дополнительно применяем статичный ключ
	XorString(res, CryptoKeyStatic, STATIC_CRYPTO_KEY_LEN);
	#endif

	// строка res зашифрована - превращаем в uri
	std::string res2;
	ConvertFromBytesToUri(res, res2);

	return res2;
	#else

	// возвращаем исходную строку без шифрования
	std::string Result{""};
	if ((SourceBufPtr != nullptr)) && (SourceBufSize > 0))
		Result.assign(SourceBufPtr, SourceBufSize);

	return Result;

	#endif

}



std::string DecryptString(const std::string& CryptedInURI) noexcept {

	// Расшифровывает строку.
	// Результат: Расшифрованный поток байт.

	#ifdef DoEncrypt
	// переводим из URI
	std::string Crypted;
	bool ok = ConvertFromUriToBytes(CryptedInURI, Crypted);

	#ifdef CryptVersion2
	// сначала применяем статичный ключ
	XorString(Crypted, CryptoKeyStatic, STATIC_CRYPTO_KEY_LEN);
	#endif

	char CryptoKey[CRYPTO_KEY_LEN];
	// получаем ключ - он в самом конце строки
	char* KeyPtr = (char*) Crypted.data() + Crypted.size() - sizeof(CryptoKey);
	memcpy(CryptoKey, KeyPtr, sizeof(CryptoKey));
	
	// длина строки без шифрации
	int UncryptedLen = Crypted.size() - sizeof(CryptoKey);

	std::string Decrypted = "";
	int KeyIndex = 0;
	for (int i = 0; i < UncryptedLen; i++) {
		char KeyChar = CryptoKey[KeyIndex];

		char DecryptedChar = Crypted[i] ^ KeyChar ^ ((char)i & 255);
		Decrypted.append(1, DecryptedChar);
		KeyIndex++;
		if (KeyIndex > CRYPTO_KEY_LEN - 1)
			KeyIndex = 0;
	} 

	// откусываем ключ шифрования
	return Decrypted.substr(0, UncryptedLen);

	#else

	return CryptedInURI;

	#endif
}



std::string EncryptWideString(const std::wstring& SourceWideString) noexcept {
	// Зашифровывает строку в формате "wide chars".
	// Результат: зашифрованная строка (в формате URI).
	std::string Result = EncryptBuffer((const char*)SourceWideString.data(),
		SourceWideString.size() * (sizeof(wchar_t)));

	return Result;
}

std::wstring DecryptWideString(const std::string& SourceString) noexcept {
	// Расшифровывает строку и представляет ее в формате wide string.
	// ВАЖНО, чтобы исходная строка была зашифрована ф-цией EncryptWideString !!!
	// Результат: Расшифрованная строка.

	std::string RawBytes = DecryptString(SourceString);
	std::wstring ResultWide;

	int NWChars = RawBytes.size() / sizeof(wchar_t);
	ResultWide.assign((wchar_t*)RawBytes.data(), NWChars);

	return ResultWide;

}

// ----------------- отдладочный вывод ------------
void DebugOutput(std::wstring s) {
	LPCWSTR p = s.c_str();
	OutputDebugStringW(p);
};

void DebugOutput(std::string s) {
	LPCSTR p = s.c_str();
	OutputDebugStringA(p);
};

// ------ генерация случайных чисел ------

static std::random_device rd;   // non-deterministic generator
static std::mt19937 gen(rd());  // to seed mersenne twister.
static std::uniform_int_distribution<> dist(0, 255);

void GenerateRandomBytes(char* BufPtr, int BufSize) {
	char r = dist(gen);
	for (int i = 0; i < BufSize; i++) {
		r = dist(gen);
		BufPtr[i] = r;
	}
}

// ---------- сохранение строки в файл (только для отладки запросов) -----------
// class Files_CLASS


bool Filer_CLASS::Create(const std::wstring& DiffFileName) {

	OutFile.open(DiffFileName);
    bool Opened = OutFile.is_open();

	return Opened;
} // bool Create(

void Filer_CLASS::WriteString(const std::string& s) {
    // Запись строки в файл: 
	OutFile.write(s.c_str(), s.size());
}

void Filer_CLASS::Close() {
	// Запись строки в файл: 
	OutFile.close();
}

Filer_CLASS::~Filer_CLASS() {
	if (OutFile.is_open())
		OutFile.close();
}


#include <sstream>			// istringstream

// Очень-Очень прострой парсер контента (ответа сервера).
// Минимум проверок!
// Ищет двойной CRLF и все после него считается контентом.
// Первые символы до пробела считаются кодом ответа сервера.
// Никакие другие параметры не проверяются.

char CRLF2[] = { 13, 10, 13, 10 };
int ParseResponse(const std::string& Resp, int& ResponseCode, std::string& Content) {
	// Возвращает 0 - успех, иначе код ошибки:
	//		1 - не распознан код ответа,
	//		2 - не найден двойной crlf
	
	int Delim1 = Resp.find(" ", 0);
	if (Delim1 == std::string::npos)
		return 1;

	int Delim2 = Resp.find(" ", Delim1 + 1);

	if (Delim2 == std::string::npos)
		return 1;

	std::string RespCode = Resp.substr(Delim1 + 1, (Delim2 - 1) - (Delim1 + 1) + 1);

	ResponseCode = atoi(RespCode.c_str());

	std::string Templ;
	Templ.assign(CRLF2, 4);
	const char* p = Resp.data();
	int Delim = Resp.find(Templ, 0);

	if (Delim == std::string::npos)
		return 2;

	Content = Resp.substr(Delim + sizeof(CRLF2));
	// здесь Content опознан и сохранен

	return 0;

}


int ExtractJsonValue(const std::string& JsonString, const std::string& JsonName, 
			std::string& JsonValue) {
	// Очень-очень простой парсер - ищет значение  в строке JSon. 
	// JsonString - строка JSON целиком,
	// JsonName - имя поля.
	// JsonValue - значение поля (пустая строка при ошибке или при отсутствии).
	// Имя поля = JsonString - БЕЗ ":" в конце !!!
	// Возвращает: 0 - успешно, иначе - код ошибки.
	
	int res = 0;

	JsonValue.clear();


	const char* p2 = JsonString.data();
	// в контенте находим строку data:
	std::string Templ = "\"" + JsonName + "\":\"";
	int Delim1 = JsonString.find(Templ, 0);
	int Delim2 = std::string::npos;

	if (Delim1 != std::string::npos) {
		// пробросим Delim1 -> первый символ значения! Не кавычка!
		Delim1 += Templ.size();
		// завершающая кавычка
		Delim2 = JsonString.find("\"", Delim1 + 1);
	}
	else {
		Delim2 = std::string::npos;
		res = 1;
	}


	if ((Delim1 != std::string::npos) && (Delim2 != std::string::npos)) {
		// сечас Delim2 - заверш. кавычка, делаем шаг назад - на последний символ значения
		Delim2--;
		// строка между Delim1 и Delim2 - это и есть значение
		int StartIndex = Delim1 ;
		int EndIndex = Delim2;
		JsonValue = JsonString.substr(StartIndex, EndIndex - StartIndex + 1);

		res = 0;
	}
	else if (res == 0)
		res = 2;

	return res;

}

void AppendStrToWStr(std::wstring& Dst, const std::string& Src) {
	Dst.append(Src.begin(), Src.end());
}
