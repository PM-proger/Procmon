
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <psapi.h>
#include <assert.h>

#include "UtilsProc_LIB.h"
//#include "pch.h"
//#include "framework.h"

#include <memory>               // unique_ptr


// для работы с ListView
int ProcessesList_CLASS::GetHeadersCount() {
    // Кол-во дб. синхронизировано с GetHeader() !!!
    return 2;
}

// для работы с ListView
std::wstring ProcessesList_CLASS::GetHeader(int HeaderIndex) {
    // Кол-во дб. синхронизировано с GetHeadersCount() !!!
    if (HeaderIndex == 0)
        return L"PId";
    else if (HeaderIndex == 1)
        return L"Process name";

    // сюда никогла не должны приходить
    return L"Unknown header!";
} // std::wstring GetHeader(int HeaderIndex)


int ProcessesList_CLASS::GetItemsCount() {
    // двойник - для совместимости с отладочной версией.
    return ProcessesInfo.size();
}

void DebugOutput2(const std::string& s) {
    LPCSTR p = s.c_str();
    OutputDebugStringA(p);
};

void DebugOutput2(const std::wstring& s) {
    LPCWSTR p = s.c_str();
    OutputDebugStringW(p);
};

//debug
#define TRACE_COLLECT_no

bool ProcessesList_CLASS::CollectProcesses() {
    bool ok = AllocBuffers();
    if (!ok)
        return false;

    ProcessesInfo.clear();

    #ifdef AccessDeniedLast
    // первичное накопление процессов с ошибкой доступа
    std::vector<ProcessDescription_STRUCT> BadProcessesInfo;
    #endif

    DWORD in_cb = ProccessIdsArraySize_in_bytes;   // размер буфера в байтах!
    DWORD out_cbNeeded = 0;                        // сколько вернули id процессов - в байтах !!!        

    BOOL res_enum = EnumProcesses(TempBufPtr, in_cb, &out_cbNeeded);

    if (res_enum != TRUE) {
        int ErrCode = GetLastError();
        //std::cout << "Error in EnumProcesses = " << ErrCode << std::endl;
        return false;
    }

    DWORD ProcessesCount = out_cbNeeded / sizeof(DWORD);    // столько процессов мы получили

    // ------------------ Получаем имена процессов --------------------------
    DWORD* out_lpidProcess = reinterpret_cast<DWORD*>(TempBufPtr);

    #ifdef TRACE_COLLECT
    DebugOutput2("CollectProcesses().start \n");
    #endif

    for (int i = 0; i < ProcessesCount; i++) {

        // здесь накапливается инф-ция о процессе
        ProcessDescription_STRUCT PD;
        PD.Ready = false;

        DWORD ProcessId = out_lpidProcess[i];
        PD.Id = ProcessId;

        if (ProcessId == 0) {
            PD.Name = L"IDLE !!!";
            continue;   // следующий процесс в for, этот процесс не запоминаем в массиве
        }
        else if (ProcessId == 4) {
            PD.Name = L"System !!!";
            continue;   // следующий процесс в for, этот процесс не запоминаем в массиве
        }

        // получим handle процесса
        DWORD AccessFlags = MAXIMUM_ALLOWED;
        //DWORD AccessFlags = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ;
        //DWORD AccessFlags = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;

        // Информация об ошибках доступа к процессам
        bool ProcessOpenedOK = false;       // процесс открыт успешно
        bool ProcessGetNameOK = false;      // имя процесса получено успешно

        HANDLE hProcess = OpenProcess(AccessFlags, FALSE, ProcessId);
        ProcessOpenedOK = (hProcess != NULL);

        if (!ProcessOpenedOK) {
            // формируем запись об ошибке
            // --- ошибка получения инф-ции о процессе ---
            PD.ErrCode = GetLastError();
            if (PD.ErrCode == ERROR_ACCESS_DENIED) {
                PD.Name =
                    #ifdef AppendIndexToProcess
                    #ifndef AccessDeniedLast
                    L" " + std::to_wstring(ProcessesInfo.size()) + L": " +
                    #endif
                    #endif
                    L"1--Can't open process. Access denied (Err=5)";
            }
            else {
                PD.Name = 
                    #ifdef AppendIndexToProcess
                    #ifndef AccessDeniedLast
                    L" " + std::to_wstring(ProcessesInfo.size()) + L": " +
                    #endif                    
                    #endif                    
                    L"2--Can't open process (Err=" + std::to_wstring(PD.ErrCode) + L")";
            }
            PD.Ready = true;
            // ERROR_INVALID_PARAMETER = 87 (IDLE)
            // ERROR_ACCESS_DENIED = 5
        }
        
        if (ProcessOpenedOK) {
            // ------------- получаем имя процесса (файла) -----------------------
            HMODULE hMod;
            DWORD cbNeeded;

            HMODULE hModule = NULL;
            //DWORD OutLen = GetModuleFileNameEx(hProcess, hModule, ModuleNameBufPtr, ModuleNameSize_in_tchars);
            DWORD OutLen = GetModuleBaseName(hProcess, hModule, ModuleNameBufPtr, ModuleNameSize_in_tchars);
            ProcessGetNameOK = (OutLen > 0);

            if (ProcessGetNameOK) {
                // формируем нормальную запись
                PD.Name =
                    #ifdef AppendIndexToProcess
                    L" " + std::to_wstring(ProcessesInfo.size()) + L": " +
                    #endif
                    PD.Name.append(ModuleNameBufPtr, OutLen);
                PD.ErrCode = 0;
                PD.Ready = true;

            }
            else {
                //ошибка
                PD.ErrCode = GetLastError();
                PD.Name =
                    #ifdef AppendIndexToProcess
                    #ifndef AccessDeniedLast
                    L" " + std::to_wstring(ProcessesInfo.size()) + L": " +
                    #endif                  
                    #endif                  
                    L"Can't get process name (Err = " + std::to_wstring(PD.ErrCode) + L")";
                PD.Ready = true;

                //std::cout << "=== GetModuleFileNameEx error === " << PD.ErrCode << std::endl;
            }
            //std::wcout << PD.Id << L" .... " << PD.Name << std::endl;
        } // if (ProcessOpenedOK) {

        // --- здесь структура PD должна быть сформирована ---
        //assert(PD.Ready);

        // --- добавляем запись в массив ---
        bool ErrorOccured = !ProcessOpenedOK || !ProcessGetNameOK;
        #ifdef AccessDeniedLast
        // Записи об ошибках не добавляем - только нормальные,
        // а ошибочные добавляем все скопом после нормальных


        if (ErrorOccured) {
            BadProcessesInfo.push_back(std::move(PD));
        }
        else {
            #ifdef TRACE_COLLECT
            int CurIndex = ProcessesInfo.size();
            if (CurIndex < 5)
                DebugOutput2(std::to_wstring(CurIndex) + L": " + PD.Name + L" \n");
            #endif

            ProcessesInfo.push_back(std::move(PD));
        }
        #else
            // Все записи добавляем сразу
            ProcessesInfo.push_back(std::move(PD));
        #endif

    } // for (int i = 0; i < ProcessesCount; i++) {
    

    #ifdef AccessDeniedLast
    // копируем данные обо всех процессах с ошибкой к основному списку
    //ProcessesInfo.

    int BadProcSize = BadProcessesInfo.size();
    std::wstring IndexStr;
    for (int i = 0; i < BadProcSize; i++) {
        ProcessDescription_STRUCT& CurBad = BadProcessesInfo[i];

        #ifdef AppendIndexToProcess
        int NewIndex = ProcessesInfo.size();
        IndexStr = L"<" + std::to_wstring(NewIndex) + L">: ";
        CurBad.Name.insert(0, IndexStr);
        #endif 

        ProcessesInfo.push_back(std::move(CurBad));
    }

        /*
    ProcessesInfo.insert(ProcessesInfo.end(), 
                            std::make_move_iterator(BadProcessesInfo.begin()), 
                            std::make_move_iterator(BadProcessesInfo.end()));
        */
    #endif


} // bool ProcessesList_CLASS::CollectProcesses() {


bool ProcessesList_CLASS::GetProcessInfo(int Index, ProcessDescription_STRUCT& pd) {
    if ((Index < 0) || (Index > ProcessesInfo.size() - 1))
        return false;

    pd = ProcessesInfo[Index];

    return true;
}



std::wstring ProcessesList_CLASS::GetItem(int ItemIndex, int SubItemIndex, LPARAM& ItemData) {
    // Выдача информации о процессе в формате ListView
        if ((ItemIndex < 0) || (ItemIndex > ProcessesInfo.size() - 1))
            return L"Item not found!!!";

        ItemData = ProcessesInfo[ItemIndex].Id;

        if (SubItemIndex == 0)
            return std::to_wstring(ProcessesInfo[ItemIndex].Id);
        else if (SubItemIndex == 1)
            return ProcessesInfo[ItemIndex].Name;
        else 
            return L"SubItem not found!!!";
       
}

ProcessesList_CLASS::~ProcessesList_CLASS() {
    FreeBufs();
}


bool ProcessesList_CLASS::AllocBuffers() {
    TempBufPtr = (DWORD*)malloc(ProccessIdsArraySize_in_bytes);
    bool ok = TempBufPtr != nullptr;

    if (ok) {
        ModuleNameBufPtr = (LPWSTR)malloc(ModuleNameSize_in_bytes);
        ok = ModuleNameBufPtr != nullptr;
    }

    if (!ok)
        FreeBufs();

    return ok;
}

void ProcessesList_CLASS::FreeBufs() {
    if (TempBufPtr != nullptr) {
        free(TempBufPtr);
        TempBufPtr = nullptr;
    }

    if (ModuleNameBufPtr != nullptr) {
        free(ModuleNameBufPtr);
        ModuleNameBufPtr = nullptr;
    }
}



int GetProcessModulesOfProcess(DWORD ProcessID, ModulesDescription_STRUCT& Modules) {
    // Возвращает перечень модулей процесса в _Modules_.
    // ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ: 0 - удачно, иначе - код ошибки.

    Modules.clear();

    // --- буфер для массива модулей ---
    const int ModsNumber = 1000;
    const int ModsNumberInBytes = ModsNumber * sizeof(HMODULE);
    std::unique_ptr<HMODULE[]> ModsPtr{ std::make_unique<HMODULE[]>(ModsNumber) };

    // --- буфер для имени модуля
    const int ModNameSizeInTChars = MAX_PATH;
    const int ModNameSizeInBytes = ModNameSizeInTChars * sizeof(TCHAR);
    std::unique_ptr<TCHAR[]> ModuleNameBufPtr{ std::make_unique<TCHAR[]>(ModNameSizeInTChars) };

    HANDLE hProcess;
    DWORD cbNeeded;
    unsigned int i;

    DWORD dwFlags = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ;
    hProcess = OpenProcess(dwFlags, FALSE, ProcessID);

    if (hProcess == NULL) {
        int ErrCode = GetLastError();
        return ErrCode;
    }

    std::wstring ModName;

    // Получаем перечень модулей
    if (EnumProcessModules(hProcess, ModsPtr.get(), ModsNumberInBytes, &cbNeeded)) {
        for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {

            // Get the full path to the module's file.
            int res = 0;

            // получить имя + путь
            // res = GetModuleFileNameEx(hProcess, ModsPtr.get()[i], ModuleNameBufPtr.get(), ModNameSizeInTChars);

            // получить имя без пути
            res = GetModuleBaseName(hProcess, ModsPtr.get()[i], ModuleNameBufPtr.get(), ModNameSizeInTChars);

            if (res == 0) {
                int ErrCode = GetLastError();
                ModName = L"Error = " + std::to_wstring(ErrCode);
                if (ErrCode == ERROR_ACCESS_DENIED)
                    ModName.append(L"  1--Access denied.");
            }
            else {
                ModName.assign(ModuleNameBufPtr.get());
            }

            ProcessDescription_STRUCT pd{true, ProcessID, std::move(ModName), 0 };
                Modules.emplace_back(std::move(pd));
        } // for

    }

    // Release the handle to the process.

    CloseHandle(hProcess);


    return 0;
}


int KillProcess(DWORD ProcessId) noexcept {
    // Завершить процесс по его идентификатору.
    // ВОЗВРАЩАЕМОЕ ЗНАЧЕНИЕ:
    //      0 - успешно,
    //      иначе - код ошибки

    int ErrCode = 0;

    // получим handle процесса
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, ProcessId);

    BOOL ok = TerminateProcess(hProcess, 4);

    if (ok == 0) {
        ErrCode = GetLastError();
    }

    return ErrCode;
}


void AppendStrings(const ModulesDescription_STRUCT& md, int FromIndex, int ToIndex, std::wstring& Result) {
    for (int i = FromIndex; i <= ToIndex; i++) {
        if (i > FromIndex)
            Result.append(L", ");

        Result.append(md[i].Name);
    }

}

void GetModulesInfo(int MaxPart, const ModulesDescription_STRUCT& md, 
                                std::wstring& out_Result) {

    // Присоединяет к out_Result склеенные имен модулей.
    // Если общеей кол-во > (MaxPart*2 + 1), то возвращаются
    // первые MaxPart и последние MaxPart имен, а между ними - многоточие

    out_Result.reserve(200);
    int TotalStrings = md.size();

    out_Result.append(L"Total DLLs = " + std::to_wstring(TotalStrings) + L"\r\n");

    if (TotalStrings <= (MaxPart * 2 + 1)) {
        // объединяем все
        AppendStrings(md, 0, TotalStrings - 1, out_Result);
    }
    else {
        // объединяем по кусочкам
        AppendStrings(md, 0, MaxPart - 1, out_Result);
        out_Result.append(L" ... ");
        AppendStrings(md, TotalStrings - 1 - MaxPart + 1, TotalStrings - 1, out_Result);
    }

    return;
} 