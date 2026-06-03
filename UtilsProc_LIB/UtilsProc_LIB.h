#pragma once

/*
Библиотека работы с процессами.
Класс ProcessesList_CLASS:
    - запрашивает, хранит и выдает список процессов.

Функции "двойники": сначала отладка велась без применения ListView для вывода.
Отлаженная версия подключалась к ListView - для минимизации корректировок
исходников в ListView было принято решение включить в класс такие же интерфейсные
ф-ции, какие требовалось в ListView.
Конечно, правильным решение было бы сразу продумать единый интерфейс... :(
*/

#include <string>
#include <vector>

// если определено, сначала в массив процессов будут включены процессы
// при доступе к которым не было ошибки OpenProcess, а затем все процессы,
// доступ к которым оказался невозможным.
#define AccessDeniedLast

// добавлять индекс в массиве процессов к имени процесса
#define AppendIndexToProcess

struct ProcessDescription_STRUCT {
    bool Ready = false;
    DWORD Id = 0;
    std::wstring Name;
    int ErrCode = 0;        // если GetLastError возвратил ошибку при попытке получить данные о процессе
};

class ProcessesList_CLASS {
    std::vector<ProcessDescription_STRUCT> ProcessesInfo;
    int NumActiveRecords = 0;   // кол-во актуальных записей в ProcessesInfo

    // буфер для получения массива id от Winapi и его размер
    #define ProccessIdsArraySize_in_bytes 5000*sizeof(DWORD)
    DWORD* TempBufPtr = nullptr;

    // буфер для получения имени модуля и его размер
    #define ModuleNameSize_in_tchars  MAX_PATH
    #define ModuleNameSize_in_bytes  ModuleNameSize_in_tchars*sizeof(TCHAR)
    LPWSTR ModuleNameBufPtr = nullptr;

public:
    
    int GetItemsCount();

    // ============ для работы с ListView =============
    int GetHeadersCount();
        // Кол-во дб. синхронизировано с GetHeader() !!!
    std::wstring GetHeader(int HeaderIndex);
        // Кол-во дб. синхронизировано с GetHeadersCount() !!!
    // =================================================

    bool CollectProcesses();
    
    // Двойники
    bool GetProcessInfo(int Index, ProcessDescription_STRUCT& pd);
        // Возвращает информацию о процессах, ранее накопленную вызовом Collect
    std::wstring GetItem(int ItemIndex, int SubItemIndex, LPARAM& ItemData);
        // Выдача информации о процессе в формате ListView

    ~ProcessesList_CLASS();


private:
    bool AllocBuffers();
    void FreeBufs();
    void AppendStrings(int FromIndex, int ToIndex, std::wstring& Result);

};


int KillProcess(DWORD ProcessId) noexcept;
    // Уничтожает процесс. Возвращает = 0 - нет ошибки, иное - код ошибки

using ModulesDescription_STRUCT = std::vector<ProcessDescription_STRUCT>;

int GetProcessModulesOfProcess(DWORD ProcessID, ModulesDescription_STRUCT& Modules);


void GetModulesInfo(int MaxPart, const ModulesDescription_STRUCT& md,
                            std::wstring& out_Result);
    // Присоединяет к out_Result склеенные имена модулей.
    // Если общее кол-во > (MaxPart*2 + 1), то возвращаются
    // первые MaxPart и последние MaxPart имен, а между ними - многоточие


