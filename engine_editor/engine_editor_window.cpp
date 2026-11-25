#include "engine_editor_window.h"
#include <CommCtrl.h>
#include <CommDlg.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

namespace engine_editor {
namespace {
struct SliderSpec { const wchar_t *label; const char *key; double minimum; double maximum; int precision; };
constexpr SliderSpec kSpecs[] = {
 {L"Redline (RPM)", "redline_rpm", 1000.0, 18000.0, 0},
 {L"Starter torque (Nm)", "starter_torque_nm", 0.0, 1200.0, 1},
 {L"Starter speed (RPM)", "starter_speed_rpm", 50.0, 1500.0, 0},
 {L"Dyno minimum (RPM)", "dyno_min_rpm", 0.0, 10000.0, 0},
 {L"Dyno maximum (RPM)", "dyno_max_rpm", 1000.0, 20000.0, 0},
 {L"Dyno hold step (RPM)", "dyno_hold_rpm", 10.0, 1000.0, 0},
 {L"Simulation frequency (Hz)", "simulation_frequency_hz", 400.0, 400000.0, 0},
 {L"High-frequency gain", "initial_high_frequency_gain", 0.0, 1.0, 3},
 {L"Air noise", "initial_noise", 0.0, 4.0, 3},
 {L"Input jitter", "initial_jitter", 0.0, 2.0, 3}
};
constexpr wchar_t kClassName[] = L"EngineSimParameterEditorWindow";

double getValue(const EngineConfig &config, const char *key, double fallback) {
 const auto it=config.parameters.find(key);
 if(it==config.parameters.end()) return fallback;
 if(const auto *v=std::get_if<double>(&it->second)) return *v;
 if(const auto *v=std::get_if<std::int64_t>(&it->second)) return static_cast<double>(*v);
 return fallback;
}
std::wstring formatValue(double v,int precision) {
 std::wostringstream s; s<<std::fixed<<std::setprecision(precision)<<v; return s.str();
}
bool parseValue(const std::wstring &s,double *value) {
 wchar_t *end=nullptr; const double v=std::wcstod(s.c_str(),&end);
 if(end==s.c_str()||!std::isfinite(v)) return false;
 while(*end!=L'\0'&&std::iswspace(*end)) ++end;
 if(*end!=L'\0') return false;
 *value=v; return true;
}
void setFont(HWND h) {
 SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
}
std::string toAnsiPath(const wchar_t *path) {
 const int length=WideCharToMultiByte(CP_ACP,0,path,-1,nullptr,0,nullptr,nullptr);
 if(length<=1) return {};
 std::string result(static_cast<std::size_t>(length),'\\0');
 if(WideCharToMultiByte(CP_ACP,0,path,-1,&result[0],length,nullptr,nullptr)!=length) return {};
 result.resize(static_cast<std::size_t>(length-1));
 return result;
}
} // namespace

EngineEditorWindow::~EngineEditorWindow(){ destroy(); }

bool EngineEditorWindow::initialize(HINSTANCE instance,const EngineConfig &initial,
 ApplyHandler apply,CurrentConfigHandler current) {
 if(m_window) return true;
 m_instance=instance; m_draft=initial;
 m_applyHandler=std::move(apply); m_currentConfigHandler=std::move(current);
 INITCOMMONCONTROLSEX controls{}; controls.dwSize=sizeof(controls); controls.dwICC=ICC_BAR_CLASSES;
 InitCommonControlsEx(&controls);
 WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.lpfnWndProc=&EngineEditorWindow::windowProcedure;
 wc.hInstance=m_instance; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
 wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1); wc.lpszClassName=kClassName;
 if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return false;
 m_window=CreateWindowExW(WS_EX_APPWINDOW,kClassName,L"Engine Simulator - Engine Editor",
  WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,760,760,
  nullptr,nullptr,m_instance,this);
 if(!m_window) return false;
 ShowWindow(m_window,SW_SHOW); UpdateWindow(m_window); return true;
}

void EngineEditorWindow::destroy(){
 if(m_window) DestroyWindow(m_window);
 m_window=nullptr; m_sliders.clear(); m_edits.clear(); m_status=nullptr;
}
void EngineEditorWindow::pumpMessages(){
 if(!m_window) return;
 MSG msg{};
 while(m_window&&PeekMessageW(&msg,m_window,0,0,PM_REMOVE)){
  TranslateMessage(&msg); DispatchMessageW(&msg);
 }
}

LRESULT CALLBACK EngineEditorWindow::windowProcedure(HWND window,UINT msg,WPARAM wp,LPARAM lp){
 EngineEditorWindow *self=reinterpret_cast<EngineEditorWindow*>(GetWindowLongPtrW(window,GWLP_USERDATA));
 if(msg==WM_NCCREATE){
  const auto *cs=reinterpret_cast<const CREATESTRUCTW*>(lp);
  self=static_cast<EngineEditorWindow*>(cs->lpCreateParams);
  if(!self) return FALSE;
  self->m_window=window;
  SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
 }
 return self?self->handleMessage(msg,wp,lp):DefWindowProcW(window,msg,wp,lp);
}
LRESULT EngineEditorWindow::handleMessage(UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_CREATE: createControls(); return 0;
 case WM_HSCROLL:
  if(reinterpret_cast<HWND>(lp)!=nullptr)
   updateParameterFromSlider(GetDlgCtrlID(reinterpret_cast<HWND>(lp))-SliderFirstId);
  return 0;
 case WM_COMMAND:
  if(HIWORD(wp)==BN_CLICKED){
   if(LOWORD(wp)==ApplyButtonId){applyDraft();return 0;}
   if(LOWORD(wp)==ReloadButtonId){reloadDraft();return 0;}
   if(LOWORD(wp)==LoadButtonId){loadConfigFile();return 0;}
   if(LOWORD(wp)==SaveButtonId){saveConfigFile();return 0;}
  }
  break;
 case WM_CLOSE: DestroyWindow(m_window); return 0;
 case WM_NCDESTROY:{
  HWND window=m_window;
  SetWindowLongPtrW(window,GWLP_USERDATA,0);
  m_window=nullptr;
  return DefWindowProcW(window,msg,wp,lp);
 }
 default: break;
 }
 return DefWindowProcW(m_window,msg,wp,lp);
}

void EngineEditorWindow::createControls(){
 HWND group=CreateWindowExW(0,L"BUTTON",L"Parametry aktywnego silnika",
  WS_CHILD|WS_VISIBLE|BS_GROUPBOX,12,12,720,610,m_window,
  reinterpret_cast<HMENU>(2000),m_instance,nullptr); setFont(group);
 m_sliders.resize(ParameterCount,nullptr); m_edits.resize(ParameterCount,nullptr);
 for(int i=0;i<ParameterCount;++i){
  const int y=48+i*55;
  HWND label=CreateWindowExW(0,L"STATIC",kSpecs[i].label,WS_CHILD|WS_VISIBLE|SS_LEFT,
   28,y+5,220,24,m_window,nullptr,m_instance,nullptr); setFont(label);
  m_sliders[i]=CreateWindowExW(0,TRACKBAR_CLASSW,L"",
   WS_CHILD|WS_VISIBLE|TBS_AUTOTICKS|WS_TABSTOP,250,y,370,34,m_window,
   reinterpret_cast<HMENU>(static_cast<INT_PTR>(SliderFirstId+i)),m_instance,nullptr);
  SendMessageW(m_sliders[i],TBM_SETRANGE,TRUE,MAKELPARAM(0,1000));
  SendMessageW(m_sliders[i],TBM_SETTICFREQ,100,0); setFont(m_sliders[i]);
  m_edits[i]=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",
   WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_RIGHT|WS_TABSTOP,632,y+4,82,25,m_window,
   reinterpret_cast<HMENU>(static_cast<INT_PTR>(EditFirstId+i)),m_instance,nullptr);
  setFont(m_edits[i]);
 }
 HWND hint=CreateWindowExW(0,L"STATIC",
  L"Suwaki zmieniaja kopie robocza. Aktywny silnik zmieni sie dopiero po zatwierdzeniu.",
  WS_CHILD|WS_VISIBLE|SS_LEFT,28,598,690,22,m_window,nullptr,m_instance,nullptr); setFont(hint);
 m_status=CreateWindowExW(0,L"STATIC",L"Gotowy",WS_CHILD|WS_VISIBLE|SS_LEFT,
  28,635,690,24,m_window,reinterpret_cast<HMENU>(3300),m_instance,nullptr); setFont(m_status);
 HWND load=CreateWindowExW(0,L"BUTTON",L"WCZYTAJ PLIK",
  WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP,28,675,150,34,m_window,
  reinterpret_cast<HMENU>(LoadButtonId),m_instance,nullptr); setFont(load);
 HWND save=CreateWindowExW(0,L"BUTTON",L"ZAPISZ PLIK",
  WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP,190,675,150,34,m_window,
  reinterpret_cast<HMENU>(SaveButtonId),m_instance,nullptr); setFont(save);
 HWND reload=CreateWindowExW(0,L"BUTTON",L"ODCZYTAJ AKTYWNY",
  WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP,352,675,175,34,m_window,
  reinterpret_cast<HMENU>(ReloadButtonId),m_instance,nullptr); setFont(reload);
 HWND apply=CreateWindowExW(0,L"BUTTON",L"MODYFIKUJ SILNIK",
  WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON|WS_TABSTOP,539,675,175,34,m_window,
  reinterpret_cast<HMENU>(ApplyButtonId),m_instance,nullptr); setFont(apply);
 updateControls();
}
void EngineEditorWindow::updateControls(){
 for(int i=0;i<ParameterCount;++i){
  const double v=getValue(m_draft,kSpecs[i].key,kSpecs[i].minimum);
  const double f=(v-kSpecs[i].minimum)/(kSpecs[i].maximum-kSpecs[i].minimum);
  const int p=static_cast<int>(std::round(std::max(0.0,std::min(1.0,f))*1000.0));
  SendMessageW(m_sliders[i],TBM_SETPOS,TRUE,p); updateEditFromParameter(i);
 }
}
void EngineEditorWindow::updateParameterFromSlider(int i){
 if(i<0||i>=ParameterCount||!m_sliders[i]) return;
 const int p=static_cast<int>(SendMessageW(m_sliders[i],TBM_GETPOS,0,0));
 const double v=kSpecs[i].minimum+(p/1000.0)*(kSpecs[i].maximum-kSpecs[i].minimum);
 m_draft.parameters[kSpecs[i].key]=v; updateEditFromParameter(i);
}
void EngineEditorWindow::updateEditFromParameter(int i){
 if(i<0||i>=ParameterCount||!m_edits[i]) return;
 const auto text=formatValue(getValue(m_draft,kSpecs[i].key,kSpecs[i].minimum),kSpecs[i].precision);
 SetWindowTextW(m_edits[i],text.c_str());
}
void EngineEditorWindow::applyDraft(){
 for(int i=0;i<ParameterCount;++i){
  wchar_t buffer[128]{}; GetWindowTextW(m_edits[i],buffer,128);
  double v=0.0;
  if(!parseValue(buffer,&v)){setStatus(L"Nieprawidlowa wartosc parametru.");return;}
  if(v<kSpecs[i].minimum||v>kSpecs[i].maximum){setStatus(L"Wartosc poza zakresem suwaka.");return;}
  m_draft.parameters[kSpecs[i].key]=v;
 }
 std::vector<ValidationIssue> issues;
 if(!validate(m_draft,&issues)){
  const std::string msg=issues.empty()?"Invalid configuration":issues.front().path+": "+issues.front().message;
  setStatus(std::wstring(msg.begin(),msg.end())); return;
 }
 std::string error;
 if(m_applyHandler&&m_applyHandler(m_draft,error)) setStatus(L"Zmiany zastosowane do aktywnego silnika.");
 else {if(error.empty())error="Nie udalo sie zastosowac zmian.";setStatus(std::wstring(error.begin(),error.end()));}
}
void EngineEditorWindow::reloadDraft(){
 if(m_currentConfigHandler){m_draft=m_currentConfigHandler();updateControls();setStatus(L"Przywrocono wartosci aktywnego silnika.");}
}
void EngineEditorWindow::loadConfigFile(){
 wchar_t fileName[MAX_PATH]{};
 OPENFILENAMEW dialog{};
 dialog.lStructSize=sizeof(dialog);
 dialog.hwndOwner=m_window;
 dialog.lpstrFilter=L"Engine Sim config (*.esconfig)\0*.esconfig\0All files (*.*)\0*.*\0\0";
 dialog.lpstrFile=fileName;
 dialog.nMaxFile=MAX_PATH;
 dialog.nFilterIndex=1;
 dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY;
 if(!GetOpenFileNameW(&dialog)) return;
 std::string path=toAnsiPath(fileName);
 if(path.empty()){setStatus(L"Nie mozna odczytac sciezki pliku.");return;}
 EngineConfig loaded;
 std::string error;
 if(!loadConfig(path,&loaded,&error)){
  setStatus(std::wstring(error.begin(),error.end()));return;
 }
 m_draft=std::move(loaded);
 updateControls();
 setStatus(L"Wczytano konfiguracje do kopii roboczej. Kliknij MODYFIKUJ SILNIK, aby ja zastosowac.");
}

void EngineEditorWindow::saveConfigFile(){
 wchar_t fileName[MAX_PATH]=L"engine.esconfig";
 OPENFILENAMEW dialog{};
 dialog.lStructSize=sizeof(dialog);
 dialog.hwndOwner=m_window;
 dialog.lpstrFilter=L"Engine Sim config (*.esconfig)\0*.esconfig\0All files (*.*)\0*.*\0\0";
 dialog.lpstrFile=fileName;
 dialog.nMaxFile=MAX_PATH;
 dialog.nFilterIndex=1;
 dialog.lpstrDefExt=L"esconfig";
 dialog.Flags=OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY;
 if(!GetSaveFileNameW(&dialog)) return;
 std::string path=toAnsiPath(fileName);
 if(path.empty()){setStatus(L"Nie mozna odczytac sciezki pliku.");return;}
 for(int i=0;i<ParameterCount;++i){
  wchar_t buffer[128]{};
  GetWindowTextW(m_edits[i],buffer,128);
  double value=0.0;
  if(!parseValue(buffer,&value)||value<kSpecs[i].minimum||value>kSpecs[i].maximum){
   setStatus(L"Nie mozna zapisac: sprawdz wartosci parametrow.");
   return;
  }
  m_draft.parameters[kSpecs[i].key]=value;
 }
 std::vector<ValidationIssue> issues;
 if(!validate(m_draft,&issues)){
  const std::string message=issues.empty()?"Invalid configuration":issues.front().message;
  setStatus(std::wstring(message.begin(),message.end()));
  return;
 }
 std::string error;
 if(!saveConfig(m_draft,path,&error)){
  setStatus(std::wstring(error.begin(),error.end()));return;
 }
 setStatus(L"Zapisano konfiguracje.");
}

void EngineEditorWindow::setStatus(const std::wstring &message){
 if(m_status) SetWindowTextW(m_status,message.c_str());
}
} // namespace engine_editor
