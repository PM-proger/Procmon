#pragma once

#include <string>
#include <fstream>					// запись в текстовый файл
#include <map>						// JSON-парсер

// определить для шифрования. Иначе шифрование не будет выполняться (заглушка)
#define DoEncrypt


class JsonString_CLASS {
	// Формирует строку JSON для тестового задания
	
	std::string JsonString;	// в этой строке формируется результат JSON

public:

	void JsonStart();
		// Начинает формирование строки.
	void JsonEnd();
		// Завершает формирование строки.

	void JsonAdd(const std::string& JsonName, const std::string& JsonVal, 
					bool Quotes, bool FinalElement = false);
		// Добавляет строку к Json.
		// Quote определяет - будет ли заключено значение _JsonVal_ в кавычки

	void JsonAdd(const std::string& JsonName, const char* JsonVal, bool Quotes, bool FinalElement = false);
		// Добавляет строку к Json

	void JsonAdd(const std::string& JsonName, int JsonVal, bool FinalElement = false);
		// Добавляет целое число к Json (значение - без кавычек).

	std::string JsonGet();
		// возвращает строкку JSON в ее текущем состоянии.

}; // class JsonString_CLASS {


/* ------------- очень простой парсер строки JSON --------------------
Проверяется только:
1) наличие открыающей и закрывающей скобок - {}
2) наличие заранее определенных имен полей (из задания),
3) значения эти полей.

УПРОЩЕНИЕ:
Все, что находится между полями не проверяется.
*/

class JsonParser_CLASS {
	std::map<std::string, std::string> JsonVals;

	bool ParseString(const std::string& s);
		// Разбирает строку. Помещает разобранные значения в map
		// Возвращает: true - разбор успешен, false - ошибка разбора.

	std::string GetJsonVal(const std::string& JsonName);

}; // class JsonParser_CLASS {


// --------------- разное -------------------

bool CapsIsPressed() noexcept;
	// Возвращает статус CAPSLOCK: true - нажата, false - не нажата


std::string EncryptString(const std::string& SourceString) noexcept;
	// Зашифровывает строку.
	// Результат: зашифрованная строка.

std::string EncryptWideString(const std::wstring& SourceWideString) noexcept;
	// Зашифровывает строку в формате "wide chars".
	// Результат: зашифрованная строка (в формате URI).


std::string EncryptBuffer(const char* SourceBufPtr, int SourceBufSize) noexcept;
	// Зашифровывает произвольный буфер SourceBufPtr:SourceBufSize.
	// Размер буфера (SourceBufSize) задается в байтах.
	// Результат: зашифрованная строка.

std::string DecryptString(const std::string& SourceString) noexcept;
	// Расшифровывает строку.
	// Результат: Расшифрованный поток байт.


std::wstring DecryptWideString(const std::string& SourceString) noexcept;
	// Расшифровывает строку и представляет ее в формате wide string.
	// ВАЖНО, чтобы исходная строка была зашифрована ф-цией EncryptWideString !!!
	// Результат: Расшифрованная строка.



// -------- отладочный вывод --------
void DebugOutput(std::wstring s);

void DebugOutput(std::string s);

// -------- запись в текстовый файла (только для отладки !!!) --------

class Filer_CLASS {

	std::ofstream OutFile;

public:
	bool Create(const std::wstring& DiffFileName);
	void WriteString(const std::string& s);
	void Close();
	~Filer_CLASS();
}; // class Filer_CLASS {


int ParseResponse(const std::string& Resp, int& ResponseCode, std::string& Content);
	// Очень-очень простой парсер ответа сервера:
	// Возвращает код ошибки и контент.

int ExtractJsonValue(const std::string& JsonString, const std::string& JsonName,
	std::string& JsonValue);
	// Очень-очень простой парсер - ищет значение  в строке JSon. 
	// JsonString - строка JSON целиком,
	// JsonName - имя поля.
	// JsonValue - значение поля (пустая строка при ошибке или при отсутствии).
	// Имя поля = JsonString - БЕЗ ":" в конце !!!
	// Возвращает: 0 - успешно, иначе - код ошибки.


// ------ ГСЧ ----------
void GenerateRandomBytes(char* BufPtr, int BufSize);

// --- уникальный ID ---
const char* GetUniqueID();

// -------- строки -----------

void AppendStrToWStr(std::wstring& Dst, const std::string& Src);