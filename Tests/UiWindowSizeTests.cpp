#include <windows.h>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace windowFixture {
DWORD record[3]{1, 1000, 700}, type = REG_BINARY, bytes = sizeof(record);
struct PositionRecord { DWORD version; LONG x; LONG y; } positionRecord{1,-1500,200};
bool positionPresent = false;
DWORD positionType = REG_BINARY, positionBytes = sizeof(positionRecord);
bool maximizedPresent = false;
DWORD maximizedValue = 0, maximizedType = REG_DWORD, maximizedBytes = sizeof(DWORD);
bool openFails = false, queryFails = false, createFails = false, writeFails = false;
int opened = 0, closed = 0, creates = 0, writes = 0, passed = 0;
void require(bool value) { if (!value) throw std::runtime_error("Assertion failed"); }
void path(HKEY root, LPCWSTR name) { require(root == HKEY_CURRENT_USER && std::wstring(name) == L"SOFTWARE\\TuyenMai\\OpenKey"); }
LSTATUS open(HKEY root, LPCWSTR name, DWORD, REGSAM access, PHKEY key) {
    path(root, name); require(access == KEY_READ);
    if (openFails) return ERROR_FILE_NOT_FOUND;
    *key = reinterpret_cast<HKEY>(1); ++opened; return ERROR_SUCCESS;
}
LSTATUS query(HKEY, LPCWSTR name, LPDWORD, LPDWORD outType, LPBYTE data, LPDWORD size) {
    const bool position = std::wstring(name) == L"uiWindowPosition";
    const bool maximized = std::wstring(name) == L"uiWindowMaximized";
    require(position || maximized || std::wstring(name) == L"uiWindowSize");
    if (position && !positionPresent) return ERROR_FILE_NOT_FOUND;
    if (maximized && !maximizedPresent) return ERROR_FILE_NOT_FOUND;
    if (queryFails) return ERROR_ACCESS_DENIED;
    const DWORD needed = maximized ? maximizedBytes : position ? positionBytes : bytes;
    *outType = maximized ? maximizedType : position ? positionType : type;
    if (*size < needed) { *size = needed; return ERROR_MORE_DATA; }
    std::memcpy(data, maximized ? static_cast<const void*>(&maximizedValue) : position ? static_cast<const void*>(&positionRecord) : record, needed);
    *size = needed; return ERROR_SUCCESS;
}
LSTATUS create(HKEY root, LPCWSTR name, DWORD, LPWSTR, DWORD, REGSAM access, LPSECURITY_ATTRIBUTES, PHKEY key, LPDWORD disposition) {
    path(root, name); require(access == KEY_SET_VALUE); ++creates;
    if (createFails) return ERROR_ACCESS_DENIED;
    *key = reinterpret_cast<HKEY>(1); *disposition = REG_OPENED_EXISTING_KEY; ++opened; return ERROR_SUCCESS;
}
LSTATUS set(HKEY, LPCWSTR name, DWORD, DWORD dataType, const BYTE* data, DWORD size) {
    const bool position = std::wstring(name) == L"uiWindowPosition";
    const bool maximized = std::wstring(name) == L"uiWindowMaximized";
    require((position || maximized || std::wstring(name) == L"uiWindowSize") &&
        dataType == (maximized ? REG_DWORD : REG_BINARY) && size == (maximized ? sizeof(DWORD) : sizeof(record))); ++writes;
    if (writeFails) return ERROR_ACCESS_DENIED;
    if (maximized) { std::memcpy(&maximizedValue,data,size); maximizedPresent=true; }
    else if (position) { std::memcpy(&positionRecord,data,size); positionPresent=true; }
    else std::memcpy(record, data, size);
    return ERROR_SUCCESS;
}
LSTATUS close(HKEY key) { require(key == reinterpret_cast<HKEY>(1)); ++closed; return ERROR_SUCCESS; }
template<class Action> void test(const char* name, Action action) {
    record[0]=1; record[1]=1000; record[2]=700; type=REG_BINARY; bytes=sizeof(record);
    positionRecord={1,-1500,200}; positionPresent=false; positionType=REG_BINARY; positionBytes=sizeof(positionRecord);
    maximizedPresent=false; maximizedValue=0; maximizedType=REG_DWORD; maximizedBytes=sizeof(DWORD);
    openFails=queryFails=createFails=writeFails=false; opened=closed=creates=writes=0;
    try { action(); require(opened == closed); ++passed; }
    catch (...) { std::cerr << "Failed: " << name << '\n'; throw; }
}
}
#define RegOpenKeyExW windowFixture::open
#define RegQueryValueExW windowFixture::query
#define RegCreateKeyExW windowFixture::create
#define RegSetValueExW windowFixture::set
#define RegCloseKey windowFixture::close
#include "../Sources/OpenKey/win32/OpenKey/OpenKey.UI/UiWindowSizeSettings.h"

int main() {
    using namespace windowFixture;
    namespace S = OpenKeyUIWindowSize;
    const auto fallback = [] { const auto size=S::Read(); require(size.width==1120 && size.height==800); };
    try {
        test("valid read", [] { const auto size=S::Read(); require(size.width==1000 && size.height==700 && writes==0); });
        test("missing key", [&] { openFails=true; fallback(); });
        test("query denied", [&] { queryFails=true; fallback(); });
        test("wrong type", [&] { type=REG_DWORD; fallback(); });
        test("truncated record", [&] { bytes=8; fallback(); });
        test("oversized record", [&] { bytes=16; fallback(); });
        test("unknown version", [&] { record[0]=2; fallback(); });
        test("invalid dimensions", [&] { for (DWORD value : {0u, 639u, 8193u, MAXDWORD}) { record[1]=value; fallback(); } });
        test("invalid height", [&] { record[2]=399; fallback(); });
        test("save roundtrip", [] { require(S::Save({1200,750})); const auto size=S::Read(); require(size.width==1200 && size.height==750 && writes==1 && record[0]==1); });
        test("invalid save no write", [] { require(!S::Save({0,800}) && !S::Save({1120,8193}) && creates==0 && writes==0); });
        test("create denied", [] { createFails=true; require(!S::Save({1200,750}) && writes==0); });
        test("write denied", [] { writeFails=true; require(!S::Save({1200,750}) && record[1]==1000); });
        test("DPI scaling", [] { require(S::Pixels(1120,96)==1120 && S::Pixels(1120,120)==1400 && S::Pixels(1120,144)==1680 && S::Pixels(1120,192)==2240); });
        test("invalid DPI fallback", [] { require(S::Pixels(1120,0)==1120 && S::Pixels(1120,MAXDWORD)==1120); });
        test("logical roundtrip", [] { for (UINT dpi : {96u,120u,144u,192u}) { const auto size=S::LogicalSize(S::Pixels(1120,dpi),S::Pixels(800,dpi),dpi); require(size.width==1120 && size.height==800); } });
        test("invalid physical bounds", [] { const auto size=S::LogicalSize(-1,800,96); require(!S::Valid(size)); });
        test("fit large work area", [] { require(S::Fit(1120,1920,96)==1120); });
        test("fit small work area", [] { require(S::Fit(1120,800,96)==752 && S::Fit(800,600,96)==552); });
        test("fit high DPI", [] { require(S::Fit(1120,1920,192)==1824); });
        test("degenerate work area", [] { require(S::Fit(1120,0,96)==1 && S::Fit(1120,-1,96)==1); });
        test("position missing preserves caller", [] { S::Position position{7,8}; require(!S::ReadPosition(position) && position.x==7 && position.y==8); });
        test("negative position roundtrip preserves size", [] { require(S::SavePosition({-1500,200})); S::Position position{}; require(S::ReadPosition(position) && position.x==-1500 && position.y==200); require(S::Read().width==1000 && record[2]==700); });
        test("position wrong type", [] { positionPresent=true; positionType=REG_SZ; S::Position position{7,8}; require(!S::ReadPosition(position) && position.x==7); });
        test("position invalid byte sizes", [] { positionPresent=true; for (DWORD count : {8u,16u}) { positionBytes=count; S::Position position{7,8}; require(!S::ReadPosition(position) && position.y==8); } });
        test("position unknown version", [] { positionPresent=true; positionRecord.version=2; S::Position position{}; require(!S::ReadPosition(position)); });
        test("position invalid bounds", [] { positionPresent=true; positionRecord.x=INT32_MIN; S::Position position{7,8}; require(!S::ReadPosition(position) && position.x==7); positionRecord.x=0; positionRecord.y=INT32_MAX; require(!S::ReadPosition(position)); });
        test("position read errors", [] { positionPresent=true; S::Position position{7,8}; openFails=true; require(!S::ReadPosition(position)); openFails=false; queryFails=true; require(!S::ReadPosition(position) && position.x==7 && writes==0); });
        test("invalid position save no write", [] { require(!S::SavePosition({1000001,0}) && !S::SavePosition({0,-1000001}) && creates==0 && writes==0); });
        test("position write errors preserve saved data", [] { createFails=true; require(!S::SavePosition({100,100}) && writes==0); createFails=false; writeFails=true; require(!S::SavePosition({100,100}) && positionRecord.x==-1500 && !positionPresent); });
        test("position clamp primary monitor", [] { require(S::ClampPosition(100,0,1920,1120)==100 && S::ClampPosition(-100,0,1920,1120)==0 && S::ClampPosition(5000,0,1920,1120)==800); });
        test("position clamp negative monitor", [] { require(S::ClampPosition(-1500,-1920,1920,1120)==-1500 && S::ClampPosition(-5000,-1920,1920,1120)==-1920 && S::ClampPosition(100,-1920,1920,1120)==-1120); });
        test("position clamp taskbar offset", [] { require(S::ClampPosition(0,40,1040,800)==40 && S::ClampPosition(9999,40,1040,800)==280); });
        test("position clamp tiny work area", [] { require(S::ClampPosition(999,20,10,100)==20 && S::ClampPosition(-999,20,0,100)==20); });
        test("position clamp extreme arithmetic", [] { require(S::ClampPosition(INT32_MAX,INT32_MAX-5,1000,100)==INT32_MAX && S::ClampPosition(INT32_MIN,INT32_MIN,1000,100)==INT32_MIN); });
        test("maximized missing defaults normal", [] { require(!S::ReadMaximized() && writes==0); });
        test("maximized true false roundtrip", [] { require(S::SaveMaximized(true) && S::ReadMaximized()); require(S::SaveMaximized(false) && !S::ReadMaximized()); require(record[1]==1000 && !positionPresent && writes==2); });
        test("maximized wrong type", [] { maximizedPresent=true; maximizedValue=1; maximizedType=REG_BINARY; require(!S::ReadMaximized()); });
        test("maximized invalid sizes", [] { maximizedPresent=true; maximizedValue=1; for (DWORD count : {0u,2u,8u}) { maximizedBytes=count; require(!S::ReadMaximized()); } });
        test("maximized invalid values", [] { maximizedPresent=true; for (DWORD value : {2u,MAXDWORD}) { maximizedValue=value; require(!S::ReadMaximized()); } });
        test("maximized read errors", [] { maximizedPresent=true; maximizedValue=1; openFails=true; require(!S::ReadMaximized()); openFails=false; queryFails=true; require(!S::ReadMaximized()); });
        test("maximized write errors preserve preference", [] { createFails=true; require(!S::SaveMaximized(true) && writes==0); createFails=false; writeFails=true; require(!S::SaveMaximized(true) && maximizedValue==0 && !maximizedPresent); });
        test("normal geometry captured", [] { S::NormalGeometry geometry; require(S::CaptureNormalGeometry(geometry,{1120,800},{-1500,200},false,false)); require(geometry.valid && geometry.size.width==1120 && geometry.position.x==-1500); });
        test("maximized geometry ignored", [] { S::NormalGeometry geometry{{1120,800},{100,200},true}; require(!S::CaptureNormalGeometry(geometry,{1920,1080},{0,0},false,true)); require(geometry.size.width==1120 && geometry.position.x==100); });
        test("minimized geometry ignored", [] { S::NormalGeometry geometry{{1120,800},{100,200},true}; require(!S::CaptureNormalGeometry(geometry,{640,400},{-32000,-32000},true,false)); require(geometry.size.height==800 && geometry.position.y==200); });
        test("invalid geometry preserves cache", [] { S::NormalGeometry geometry{{1120,800},{100,200},true}; require(!S::CaptureNormalGeometry(geometry,{0,800},{100,200},false,false)); require(!S::CaptureNormalGeometry(geometry,{1120,800},{INT32_MAX,200},false,false)); require(geometry.valid && geometry.position.x==100); });
        test("restore updates normal cache", [] { S::NormalGeometry geometry{{1120,800},{100,200},true}; require(S::CaptureNormalGeometry(geometry,{1200,750},{300,400},false,false)); require(geometry.size.width==1200 && geometry.size.height==750 && geometry.position.y==400); });
        test("title bar inset DPI scaling", [] { require(S::LogicalMetric(138,96)==138 && S::LogicalMetric(207,144)==138 && S::LogicalMetric(276,192)==138); });
        test("title bar invalid metrics", [] { require(S::LogicalMetric(-1,96)==0 && S::LogicalMetric(138,0)==138 && S::LogicalMetric(138,MAXDWORD)==138); });
        test("title bar fractional metric", [] { require(S::LogicalMetric(1,192)==0.5 && S::LogicalMetric(40,120)==32); });
        std::cout << "UI window geometry/state/titlebar: " << passed << "/" << passed << " passed (fake registry; pure layout/state logic)\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
