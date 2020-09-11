#include "pch.h"
#include "PdbHelper.h"

CComPtr<IDiaDataSource> PdbHelper::source;

typedef HRESULT (*PFNDllGetClassObject)(
    REFCLSID rclsid,
    REFIID   riid,
    LPVOID* ppv
);

std::unique_ptr<PdbHelper> PdbHelper::Load(const wchar_t* path) {
    if (!source) {
        if (FAILED(CoCreateInstance(CLSID_DiaSource, NULL, CLSCTX_INPROC_SERVER, __uuidof(IDiaDataSource), reinterpret_cast<void**>(&source)))) {
            fprintf(stderr, "[!] failed to CoCreateInstance, retry with LoadLibrary\n");
            // try use it without registration
            HMODULE hMsdia = LoadLibraryA("msdia140.dll");
            if (!hMsdia) {
                fprintf(stderr, "[!] failed to LoadLibraryA(\"msdia140.dll\"): 0x%x\n", GetLastError());
                return nullptr;
            }
            // fprintf(stderr, "getaddr\n");
            PFNDllGetClassObject pfnGetObject = (PFNDllGetClassObject)GetProcAddress(hMsdia, "DllGetClassObject");
            if (!pfnGetObject) {
                fprintf(stderr, "[!] failed to GetProcAddress(DllGetClassObject)\n");
                FreeLibrary(hMsdia);
                return nullptr;
            }
            // fprintf(stderr, "getobj\n");
            CComPtr<IClassFactory> cf;
            if (FAILED(pfnGetObject(CLSID_DiaSource, IID_IClassFactory, reinterpret_cast<void**>(&cf)))) {
                fprintf(stderr, "[!] failed to DllGetClassObject\n");
                FreeLibrary(hMsdia);
                return nullptr;
            }
            // fprintf(stderr, "createobj\n");
            if (FAILED(cf->CreateInstance(0, __uuidof(IDiaDataSource), reinterpret_cast<void**>(&source)))) {
                fprintf(stderr, "[!] failed to CreateInstance\n");
                FreeLibrary(hMsdia);
                return nullptr;
            }
        }
        fprintf(stderr, "[-] IDiaDataSource is successfully created\n");
    }
    std::unique_ptr<PdbHelper> ret(new PdbHelper(path));
    if (ret->session && ret->global) {
        return ret;
    }
    return nullptr;
}

PdbHelper::PdbHelper(const wchar_t* path) {
    if (FAILED(source->loadDataFromPdb(path)))
        return;

    if (FAILED(source->openSession(&session)))
        return;

    if (FAILED(session->get_globalScope(&global)))
        return;
}

DWORD PdbHelper::get_function_rva(const wchar_t* function) {
    CComPtr<IDiaEnumSymbols> enum_symbols;
    CComPtr<IDiaSymbol> current_symbol;
    ULONG celt = 0;

    //filter the results so it only gives us symbols with the name we want
    if (FAILED(global->findChildren(SymTagNull, function, nsNone, &enum_symbols)))
        return 0;

    while (SUCCEEDED(enum_symbols->Next(1, &current_symbol, &celt)) && celt == 1)
    {
        DWORD relative_function_address;
        BSTR name;
        if (FAILED(current_symbol->get_relativeVirtualAddress(&relative_function_address)))
            continue;

        if (FAILED(current_symbol->get_name(&name)))
            continue;

        if (!relative_function_address)
            continue;

        if (wcscmp(function, name) == 0) {
            return relative_function_address;
        }
    }
    return 0;
}

ULONGLONG PdbHelper::sizeof_udt(const wchar_t* udt)
{
    CComPtr<IDiaEnumSymbols> enum_symbols;
    CComPtr<IDiaSymbol> current_symbol;

    ULONG celt = 0;

    //filter the results so it only gives us symbols with the name we want
    if (FAILED(global->findChildren(SymTagUDT, udt, nsNone, &enum_symbols)))
        return 0;
    while (SUCCEEDED(enum_symbols->Next(1, &current_symbol, &celt)) && celt == 1)
    {
        BSTR name;
        ULONGLONG len;

        if (FAILED(current_symbol->get_name(&name)))
            continue;
        if (FAILED(current_symbol->get_length(&len)))
            continue;
        if (wcscmp(udt, name) == 0) {
            return len;
        }
    }
    return 0;
}

DWORD PdbHelper::offsetof_field(const wchar_t* udt, const wchar_t* field)
{
    CComPtr<IDiaEnumSymbols> enum_udts;
    CComPtr<IDiaSymbol> current_utd;

    ULONG celt = 0;

    //filter the results so it only gives us symbols with the name we want
    if (FAILED(global->findChildren(SymTagUDT, udt, nsNone, &enum_udts)))
        return MAXDWORD;
    //loop just in case? ive only ever seen this need to be a conditional
    while (SUCCEEDED(enum_udts->Next(1, &current_utd, &celt)) && celt == 1) {
        CComPtr<IDiaEnumSymbols> enum_fields;
        CComPtr<IDiaSymbol> current_field;
        BSTR udt_name;
        ULONGLONG len;

        if (FAILED(current_utd->get_name(&udt_name)))
            continue;
        if (FAILED(current_utd->get_length(&len)))
            continue;
        if (wcscmp(udt, udt_name)) continue;
        if (FAILED(current_utd->findChildren(SymTagNull, field, nsNone, &enum_fields))) {
            continue;
        }
        while (SUCCEEDED(enum_fields->Next(1, &current_field, &celt)) && celt == 1) {
            BSTR field_name;
            LONG offset;

            if (FAILED(current_field->get_name(&field_name)))
                continue;
            if (FAILED(current_field->get_offset(&offset)))
                continue;
            if (wcscmp(field, field_name) == 0) {
                return offset;
            }
        }
    }
    return MAXDWORD;
}

