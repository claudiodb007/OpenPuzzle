#include "openpuzzle/ui/WindowsUiWindow.hpp"
#include "openpuzzle/ui/WindowsUiProcess.hpp"
#include "openpuzzle/ui/WindowsUiModel.hpp"
#include "openpuzzle/ui/WindowsUiTheme.hpp"
#include "openpuzzle/services/PuzzleMetadataCatalog.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#include <commdlg.h>
// WIN32_LEAN_AND_MEAN omits COM declarations required by the Windows SDK GDI+ headers.
#include <objidl.h>
#include <gdiplus.h>
#include <uxtheme.h>
#include <fstream>
#include <map>
#include <chrono>
#include <memory>
#include <set>
#include <tuple>
namespace openpuzzle::ui {
namespace {
enum Id {Profile=100,OpenProfile,Server,Slot,Puzzle,Duration,Once,Preflight,Start,Stop,SafeStop,Recover,Refresh,Enabled,CriticalStop,Warning,Critical,SaveThermal,State,Identity,Telemetry,Log,Theme,Details,Doctor,Badge,TemperatureMetric,PowerMetric,LimitMetric};
std::wstring wide(const std::string& text){
 if(text.empty())return {};const int n=MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0);
 std::wstring out(n,L'\0');if(n)MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),out.data(),n);return out;
}
std::string utf8(const std::wstring& text){
 if(text.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
 if(!n)throw std::invalid_argument("Texto Unicode inválido");std::string out(n,'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),out.data(),n,nullptr,nullptr);return out;
}
struct ProfileEntry {std::filesystem::path path;WindowsRunProfile profile;std::string contents;};
struct Panel {
 HWND window=nullptr;std::map<int,HWND> controls;HFONT font=nullptr,titleFont=nullptr,labelFont=nullptr,cardFont=nullptr,monoFont=nullptr;std::filesystem::path cli;
 bool dark=false;WindowsUiPalette palette=WindowsUiPalette::get(false);HBRUSH panelBrush=nullptr,inputBrush=nullptr,backBrush=nullptr,softBrush=nullptr;
 std::set<int> checkboxIds;std::map<int,bool> checks;std::map<int,RECT> cards;int scroll=0,contentLeft=0,contentWidth=0,pageHeight=1230;
 ULONG_PTR gdiplus=0;std::unique_ptr<Gdiplus::Image> logo;
 std::vector<ProfileEntry> profiles;std::optional<ProfileEntry> selected;
 WindowsUiProcess runtime,control,monitor,sensors;UiAction controlAction=UiAction::ThermalRead;
 WindowsUiStatus status;std::string monitorSlot="primary",activeSlot,monitorOutput,controlOutput,sensorOutput,log;
 ULONGLONG nextRefresh=0,opened=GetTickCount64();bool smoke=false,statusProved=false,sensorProved=false,thermalLoaded=false;int result=0,smokeStage=0;
 explicit Panel(std::filesystem::path path,bool test):cli(std::move(path)),smoke(test){}
 ~Panel(){logo.reset();if(gdiplus)Gdiplus::GdiplusShutdown(gdiplus);for(auto f:{font,titleFont,labelFont,cardFont,monoFont})if(f)DeleteObject(f);for(auto b:{panelBrush,inputBrush,backBrush,softBrush})if(b)DeleteObject(b);}
 HWND make(int id,const wchar_t* kind,const wchar_t* title,DWORD style=0){
  if(kind==std::wstring(L"BUTTON")){
   if((style&BS_TYPEMASK)==BS_AUTOCHECKBOX){checkboxIds.insert(id);style&=~BS_TYPEMASK;}
   style|=BS_OWNERDRAW|BS_NOTIFY;
  }
  if(kind==std::wstring(L"COMBOBOX"))style|=CBS_OWNERDRAWFIXED|CBS_HASSTRINGS;
  HWND h=CreateWindowExW(kind==std::wstring(L"EDIT")?WS_EX_CLIENTEDGE:0,kind,title,WS_CHILD|WS_VISIBLE|(kind==std::wstring(L"STATIC")?0:WS_TABSTOP)|style,
   0,0,10,10,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
  if(!h)throw std::runtime_error("Controlo Windows indisponível");controls[id]=h;SetWindowTheme(h,L"",L"");SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return h;
 }
 void label(int id,const wchar_t* title){make(id,L"STATIC",title,SS_LEFT);}
 void set(int id,const std::string& value){SetWindowTextW(controls.at(id),wide(value).c_str());}
 std::string get(int id){const auto h=controls.at(id);const int length=GetWindowTextLengthW(h);std::wstring value(length+1,L'\0');GetWindowTextW(h,value.data(),length+1);value.resize(length);return utf8(value);}
 bool checked(int id){return checks[id];}
 void checkmark(int id,bool value){checks[id]=value;InvalidateRect(controls[id],nullptr,TRUE);}
 std::optional<PuzzleExecutionMetadata> puzzleMetadata(){
  try{const auto number=cliIntegerArgument({"--puzzle",get(Puzzle)},{"--puzzle"},0);bool missing=false;
   const auto value=windowsFile::readPrivate(cli.parent_path()/"puzzles"/(std::to_string(number)+".json"),missing,16384);
   return value?PuzzleMetadataCatalog::parse(number,*value):std::nullopt;
  }catch(...){return {};}
 }
 void append(const std::string& value){if(value.empty())return;log+=value;if(log.size()>196608)log.erase(0,log.size()-131072);
  std::string display;display.reserve(log.size()*2);for(char c:log){if(c=='\r')continue;if(c=='\n')display+='\r';display+=c;}set(Log,display);SendMessageW(controls[Log],EM_SETSEL,static_cast<WPARAM>(-1),static_cast<LPARAM>(-1));SendMessageW(controls[Log],EM_SCROLLCARET,0,0);
 }
 void error(const std::exception& e){append(std::string("\nErro: ")+e.what()+"\n");if(!smoke)MessageBoxW(window,wide(e.what()).c_str(),L"OpenPuzzle",MB_OK|MB_ICONERROR);}
 void addProfile(const std::filesystem::path& path){
  if(!path.is_absolute()||!windowsFile::safePath(path))throw std::invalid_argument("Caminho de perfil inválido");
  bool missing=false;auto contents=windowsFile::readPrivate(path,missing,16384);if(!contents)throw std::runtime_error("Perfil ausente ou ilegível");
  auto profile=WindowsRunProfile::parse(*contents);if(profile.backend!="cpu"&&profile.gpuUuid.empty()&&profile.pciBusId.empty())throw std::invalid_argument("Seleciona uma cópia GPU com UUID/PCI associado");
  for(std::size_t i=0;i<profiles.size();++i)if(profiles[i].path==path){profiles[i]={path,profile,*contents};SendMessageW(controls[Profile],CB_SETCURSEL,i,0);chooseProfile();return;}
  const auto title=profile.backend+" / "+profileSlot(profile)+" / "+pathUtf8(path.filename());
  SendMessageW(controls[Profile],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(title).c_str()));profiles.push_back({path,profile,*contents});
  SendMessageW(controls[Profile],CB_SETCURSEL,profiles.size()-1,0);chooseProfile();
 }
 void chooseProfile(){
  const auto index=SendMessageW(controls[Profile],CB_GETCURSEL,0,0);if(index==CB_ERR||static_cast<std::size_t>(index)>=profiles.size()){selected.reset();return;}
  selected=profiles[static_cast<std::size_t>(index)];set(Slot,profileSlot(selected->profile));
  const auto& p=selected->profile;set(Identity,p.deviceIdentity+"\nUUID: "+(p.gpuUuid.empty()?"—":p.gpuUuid)+"   PCI: "+(p.pciBusId.empty()?"—":p.pciBusId)+"\nPerfil: "+pathUtf8(selected->path));
  status={};nextRefresh=0;sensorMetrics();update();
 }
 WindowsUiSelection selection(bool numbers=true){
  WindowsUiSelection s;s.server=get(Server);s.slot=activeSlot.empty()?get(Slot):activeSlot;
  if(selected){s.profile=selected->profile;s.profilePath=pathUtf8(selected->path);}
  if(numbers){s.puzzle=cliIntegerArgument({"--puzzle",get(Puzzle)},{"--puzzle"},0);s.duration=cliIntegerArgument({"--duration",get(Duration)},{"--duration"},0);}
  s.once=checked(Once);return s;
 }
 void update(){
  const bool lock=runtime.busy()||control.busy();for(int id:{Profile,OpenProfile,Server,Slot,Puzzle,Duration,Once})EnableWindow(controls[id],!lock);
  const auto metadata=puzzleMetadata();const bool endpoint=client::HttpRequestPolicy::endpoint(get(Server)).has_value();
  EnableWindow(controls[Start],!lock&&selected.has_value()&&status.canStart()&&endpoint&&metadata&&metadata->searchMode=="linear");EnableWindow(controls[Preflight],!lock&&selected.has_value());
  EnableWindow(controls[Stop],!control.busy()&&(runtime.busy()||status.running));EnableWindow(controls[SafeStop],!control.busy()&&(runtime.busy()||status.running));
  EnableWindow(controls[Recover],!lock&&status.known&&!status.running&&status.preserved);
  for(int id:{Enabled,CriticalStop,Warning,Critical,SaveThermal})EnableWindow(controls[id],!lock&&thermalLoaded);
  set(State,runtime.busy()?"Pesquisa em execução — a paragem é cooperativa":!status.known?"Estado indisponível — início bloqueado":status.running?"Execução ativa neste slot":status.preserved?"Trabalho preservado — verifica o registo e recupera":!endpoint?"Slot livre · indica o servidor para iniciar":!metadata?"Slot livre · puzzle não disponível neste painel":"Slot livre · pronto para iniciar");
  set(Badge,runtime.busy()||status.running?"EM EXECUÇÃO":!status.known?"A VERIFICAR":status.preserved?"PRESERVADO":"PARADO");
 }
 void refresh(){
  if(!monitor.busy()){
   try{monitorSlot=activeSlot.empty()?get(Slot):activeSlot;WindowsUiSelection s;s.slot=monitorSlot;monitorOutput.clear();monitor.start(cli,windowsUiCommand(UiAction::Status,s));}
   catch(const std::exception& e){status={};append(std::string("Estado: ")+e.what()+"\n");}
  }
  if(!sensors.busy()){sensorOutput.clear();try{sensors.start(cli,{"telemetry"});}catch(const std::exception& e){set(Telemetry,e.what());}}
  nextRefresh=GetTickCount64()+3000;
 }
 void action(UiAction which){
  if(control.busy())return;auto s=selection(which==UiAction::Start);
  if(which==UiAction::Start||which==UiAction::Preflight){
   if(!selected)throw std::runtime_error("Seleciona um perfil medido");bool missing=false;
   auto contents=windowsFile::readPrivate(selected->path,missing,16384);
   if(!contents||*contents!=selected->contents)throw std::runtime_error("O perfil mudou. Volta a abrir o ficheiro para rever a seleção.");
  }
  if(which==UiAction::Start){
   if(runtime.busy()||!status.canStart())throw std::runtime_error("É necessário confirmar que o slot está livre");
   const auto metadata=puzzleMetadata();if(!metadata||metadata->searchMode!="linear")throw std::runtime_error("Puzzle não disponível neste painel");
   const auto args=windowsUiCommand(which,s);
   if(!smoke&&MessageBoxW(window,wide("Iniciar Puzzle "+std::to_string(s.puzzle)+" em "+s.slot+"?\nServidor: "+s.server).c_str(),L"Iniciar pesquisa",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
   runtime.start(cli,args,true);activeSlot=s.slot;append("\nA iniciar pesquisa; aguarda a confirmação da CLI.\n");
  }else{
   if(which==UiAction::Recover&&(!status.known||status.running))throw std::runtime_error("Recuperação bloqueada enquanto o estado não for confirmado");
   if(which==UiAction::Recover&&!smoke&&MessageBoxW(window,L"Reconciliar o trabalho preservado com o servidor? Não será pedido novo trabalho.",L"Recuperar",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
   controlAction=which;controlOutput.clear();control.start(cli,windowsUiCommand(which,s),false,which==UiAction::Recover?120000:60000);
  }update();
 }
 void saveThermal(){
  auto s=selection(false);s.thermal.enabled=checked(Enabled);s.thermal.stopOnCritical=checked(CriticalStop);
  s.thermal.warningC=windowsUiThermalThreshold(get(Warning));s.thermal.criticalC=windowsUiThermalThreshold(get(Critical));
  auto args=windowsUiCommand(UiAction::ThermalSave,s);controlAction=UiAction::ThermalSave;controlOutput.clear();control.start(cli,args);update();
 }
 void readThermal(){
  for(const auto& item:std::vector<std::pair<std::string,int>>{{"Warning C: ",Warning},{"Critical C: ",Critical}}){const auto p=controlOutput.find(item.first);if(p==std::string::npos)throw std::runtime_error("Resposta térmica incompleta");const auto end=controlOutput.find_first_of("\r\n",p);set(item.second,controlOutput.substr(p+item.first.size(),end-p-item.first.size()));}
  checkmark(Enabled,controlOutput.find("Thermal mode: enabled")!=std::string::npos);
  checkmark(CriticalStop,controlOutput.find("Stop on critical: yes")!=std::string::npos);thermalLoaded=true;
 }
 void tick(){
  auto r=runtime.poll();append(r.output);if(r.finished){append("\nPesquisa terminou. Código: "+std::to_string(r.exitCode)+"\n");activeSlot.clear();status={};nextRefresh=0;}
  auto c=control.poll();if(controlOutput.size()<262144)controlOutput+=c.output;append(c.output);
  if(c.finished){append("\nComando terminou. Código: "+std::to_string(c.exitCode)+(c.timedOut?" (tempo limite)":"")+"\n");
   if(c.exitCode==0&&(controlAction==UiAction::ThermalRead||controlAction==UiAction::ThermalSave))readThermal();nextRefresh=0;
  }
  auto m=monitor.poll();if(monitorOutput.size()<262144)monitorOutput+=m.output;
  if(m.finished){
   const auto current=activeSlot.empty()?get(Slot):activeSlot;
   if(current==monitorSlot){status=WindowsUiStatus::parse(m.exitCode,monitorOutput,monitorSlot);statusProved=status.known;if(!status.known)append("Estado indisponível: "+monitorOutput+"\n");}
  }
  auto t=sensors.poll();if(sensorOutput.size()<262144)sensorOutput+=t.output;
  if(t.finished){sensorMetrics();set(Telemetry,(t.exitCode==0?"":"Leitura indisponível\n")+sensorOutput);sensorProved=t.exitCode==0;}
  if(GetTickCount64()>=nextRefresh)refresh();update();
  if(smoke&&statusProved&&sensorProved&&thermalLoaded&&smokeStage==0){
   if(controls.size()<28||IsWindowEnabled(controls[Start])||!IsWindowEnabled(controls[SaveThermal])){result=1;DestroyWindow(window);return;}
   SendMessageW(controls[Theme],CB_SETCURSEL,1,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(Theme,CBN_SELCHANGE),reinterpret_cast<LPARAM>(controls[Theme]));
   SetWindowPos(window,nullptr,0,0,840,660,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);scrollTo(450);
   if(!dark||scroll==0||!logo||logo->GetLastStatus()!=Gdiplus::Ok)throw std::runtime_error("Tema, scroll ou logótipo indisponível");
   runtime.start(cli,{"fixture-run"},true);activeSlot="primary";smokeStage=1;
   SendMessageW(window,WM_CLOSE,0,0);if(!IsWindow(window))throw std::runtime_error("A janela fechou durante a pesquisa");
  }else if(smoke&&smokeStage==1&&!runtime.busy()){
   result=log.find("runtime-finished")!=std::string::npos?0:1;DestroyWindow(window);
  }else if(smoke&&GetTickCount64()-opened>20000){result=1;DestroyWindow(window);}
 }
 void applyTheme(){
  palette=WindowsUiPalette::get(dark);for(auto b:{panelBrush,inputBrush,backBrush,softBrush})if(b)DeleteObject(b);
  panelBrush=CreateSolidBrush(palette.panel);inputBrush=CreateSolidBrush(palette.input);backBrush=CreateSolidBrush(palette.background);softBrush=CreateSolidBrush(palette.soft);
  RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
 }
 void rounded(HDC dc,RECT rect,COLORREF color,COLORREF border,int radius=12){
  auto brush=CreateSolidBrush(color);auto pen=CreatePen(PS_SOLID,1,border);auto previousBrush=SelectObject(dc,brush),previousPen=SelectObject(dc,pen);
  RoundRect(dc,rect.left,rect.top,rect.right,rect.bottom,radius*2,radius*2);SelectObject(dc,previousBrush);SelectObject(dc,previousPen);DeleteObject(brush);DeleteObject(pen);
 }
 void paint(HDC dc){
  RECT client{};GetClientRect(window,&client);FillRect(dc,&client,backBrush);
  for(const auto& [id,rect]:cards)rounded(dc,rect,palette.panel,palette.border);
  RECT plate{contentLeft+18,12-scroll,contentLeft+182,104-scroll};rounded(dc,plate,RGB(8,10,13),palette.border,13);
  if(logo&&logo->GetLastStatus()==Gdiplus::Ok){Gdiplus::Graphics graphics(dc);graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
   const double scale=(std::min)(154.0/logo->GetWidth(),82.0/logo->GetHeight());const int w=static_cast<int>(logo->GetWidth()*scale),h=static_cast<int>(logo->GetHeight()*scale);
   graphics.DrawImage(logo.get(),plate.left+(164-w)/2,plate.top+(92-h)/2,w,h);
  }
  const int gap=12,inner=contentWidth-36,cell=(inner-2*gap)/3;
  for(int i=0;i<3;++i){RECT metric{contentLeft+18+i*(cell+gap),212-scroll,contentLeft+18+i*(cell+gap)+cell,276-scroll};rounded(dc,metric,palette.soft,palette.border,8);}
 }
 LRESULT color(HDC dc,HWND child){
  const int id=GetDlgCtrlID(child);const bool edit=id==Server||id==Slot||id==Puzzle||id==Duration||id==Warning||id==Critical||id==Log||id==Telemetry;
  const bool metric=id==10||id==12||id==13||id==TemperatureMetric||id==PowerMetric||id==LimitMetric;
  const bool header=id==1||id==2||id==14;
  const auto background=edit?palette.input:metric?palette.soft:header?palette.background:palette.panel;
  const bool muted=(id>=3&&id<=14)||id==Identity;
  SetTextColor(dc,muted?palette.muted:palette.text);SetBkColor(dc,background);SetBkMode(dc,OPAQUE);
  return reinterpret_cast<LRESULT>(edit?inputBrush:metric?softBrush:header?backBrush:panelBrush);
 }
 void draw(DRAWITEMSTRUCT* item){
  if(!item)return;const int id=static_cast<int>(item->CtlID);auto dc=item->hDC;auto rect=item->rcItem;SetBkMode(dc,TRANSPARENT);
  if(item->CtlType==ODT_COMBOBOX){
   auto index=item->itemID;if(index==static_cast<UINT>(-1))index=static_cast<UINT>(SendMessageW(item->hwndItem,CB_GETCURSEL,0,0));
   const auto length=SendMessageW(item->hwndItem,CB_GETLBTEXTLEN,index,0);std::wstring text;
   if(length!=CB_ERR){text.resize(static_cast<std::size_t>(length)+1);SendMessageW(item->hwndItem,CB_GETLBTEXT,index,reinterpret_cast<LPARAM>(text.data()));text.resize(static_cast<std::size_t>(length));}
   auto brush=CreateSolidBrush(item->itemState&ODS_SELECTED?palette.secondary:palette.input);FillRect(dc,&rect,brush);DeleteObject(brush);
   SetTextColor(dc,item->itemState&ODS_DISABLED?palette.muted:palette.text);SelectObject(dc,font);rect.left+=10;DrawTextW(dc,text.c_str(),static_cast<int>(text.size()),&rect,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);return;
  }
  auto text=wide(get(id));const bool disabled=(item->itemState&ODS_DISABLED)!=0;
  if(checkboxIds.count(id)){
   FillRect(dc,&rect,panelBrush);RECT box{rect.left+2,rect.top+(rect.bottom-rect.top-18)/2,rect.left+20,rect.top+(rect.bottom-rect.top+18)/2};rounded(dc,box,palette.input,disabled?palette.border:palette.muted,3);
   if(checked(id)){auto pen=CreatePen(PS_SOLID,2,disabled?palette.muted:palette.accent);auto old=SelectObject(dc,pen);MoveToEx(dc,box.left+4,box.top+9,nullptr);LineTo(dc,box.left+8,box.top+13);LineTo(dc,box.left+14,box.top+5);SelectObject(dc,old);DeleteObject(pen);}
   rect.left+=30;SetTextColor(dc,disabled?palette.muted:palette.text);SelectObject(dc,font);DrawTextW(dc,text.c_str(),-1,&rect,DT_LEFT|DT_VCENTER|DT_SINGLELINE);return;
  }
  COLORREF background=palette.secondary,textColor=palette.text,border=palette.border;
  if(id==Start||(id==Badge&&(runtime.busy()||status.running))){background=palette.accent;textColor=palette.accentText;border=palette.accent;}
  if(id==Stop){background=RGB(185,28,28);textColor=RGB(255,255,255);border=background;}
  if(disabled){background=palette.soft;textColor=palette.muted;border=palette.border;}
  if(item->itemState&ODS_SELECTED)background=palette.soft;
  FillRect(dc,&rect,panelBrush);rounded(dc,rect,background,border,id==Badge?13:7);SetTextColor(dc,textColor);SelectObject(dc,id==Badge?labelFont:font);DrawTextW(dc,text.c_str(),-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
  if(item->itemState&ODS_FOCUS){InflateRect(&rect,-4,-4);DrawFocusRect(dc,&rect);}
 }
 void scrollTo(int value){RECT r{};GetClientRect(window,&r);scroll=(std::clamp)(value,0,(std::max)(0,pageHeight-static_cast<int>(r.bottom)));layout();InvalidateRect(window,nullptr,TRUE);}
 void ensureVisible(HWND child){RECT r{},client{};GetWindowRect(child,&r);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&r),2);GetClientRect(window,&client);if(r.top<8)scrollTo(scroll+r.top-16);else if(r.bottom>client.bottom-8)scrollTo(scroll+r.bottom-client.bottom+16);}
 void sensorMetrics(){
  std::string section=sensorOutput;
  if(selected&&selected->profile.backend!="cpu"){
   const auto& profile=selected->profile;const auto key=profile.gpuUuid.empty()?profile.pciBusId:profile.gpuUuid;const auto found=section.find(key);
   if(found==std::string::npos)section.clear();else {const auto begin=section.rfind("Device: ",found),end=section.find("Device: ",found);section=section.substr(begin==std::string::npos?0:begin,end==std::string::npos?std::string::npos:end-(begin==std::string::npos?0:begin));}
  }
  for(const auto& value:std::vector<std::tuple<int,std::string,std::string>>{{TemperatureMetric,"Temperature C: "," °C"},{PowerMetric,"Power W: "," W"},{LimitMetric,"Power limit W: "," W"}}){const auto at=section.find(std::get<1>(value));const auto start=at==std::string::npos?at:at+std::get<1>(value).size();const auto end=section.find_first_of("\r\n",start);set(std::get<0>(value),at==std::string::npos?"—":section.substr(start,end-start)+std::get<2>(value));}
 }
 void build(){
  font=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  titleFont=CreateFontW(-27,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  cardFont=CreateFontW(-17,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  labelFont=CreateFontW(-12,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  monoFont=CreateFontW(-13,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_PITCH,L"Consolas");
  Gdiplus::GdiplusStartupInput startup;if(Gdiplus::GdiplusStartup(&gdiplus,&startup,nullptr)==Gdiplus::Ok)logo=std::make_unique<Gdiplus::Image>((cli.parent_path()/"ui-assets"/"openpuzzle-logo-header.png").c_str());
  label(1,L"OpenPuzzle");label(2,L"Pesquisa distribuída · painel Windows");label(14,L"Tema");make(Theme,L"COMBOBOX",L"",CBS_DROPDOWNLIST);SendMessageW(controls[Theme],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Claro"));SendMessageW(controls[Theme],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Escuro"));SendMessageW(controls[Theme],CB_SETCURSEL,0,0);make(Badge,L"STATIC",L"A VERIFICAR",SS_OWNERDRAW);
  label(30,L"Estado atual");label(31,L"Nova execução");label(32,L"Proteção térmica");label(33,L"Ferramentas");label(34,L"Detalhes");
  label(3,L"Motor / GPU — perfil medido");make(Profile,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL);make(OpenProfile,L"BUTTON",L"Abrir perfil…");label(Identity,L"Seleciona um perfil medido.");
  label(4,L"Servidor");make(Server,L"EDIT",L"",ES_AUTOHSCROLL);label(5,L"Slot");make(Slot,L"EDIT",L"primary",ES_AUTOHSCROLL);
  label(6,L"Puzzle");make(Puzzle,L"EDIT",L"71",ES_NUMBER);label(7,L"Duração (min)");make(Duration,L"EDIT",L"60",ES_NUMBER);make(Once,L"BUTTON",L"Uma atribuição",BS_AUTOCHECKBOX);
  make(Preflight,L"BUTTON",L"Verificar perfil");make(Start,L"BUTTON",L"Iniciar");make(Stop,L"BUTTON",L"Parar");make(SafeStop,L"BUTTON",L"Terminar atribuição");make(Recover,L"BUTTON",L"Recuperar");make(Refresh,L"BUTTON",L"Atualizar");make(Doctor,L"BUTTON",L"Diagnóstico offline");label(State,L"A verificar estado…");
  make(Enabled,L"BUTTON",L"Ativar proteção térmica",BS_AUTOCHECKBOX);make(CriticalStop,L"BUTTON",L"Parar no limite crítico",BS_AUTOCHECKBOX);label(8,L"Aviso °C");make(Warning,L"EDIT",L"",ES_AUTOHSCROLL);label(9,L"Crítico °C");make(Critical,L"EDIT",L"",ES_AUTOHSCROLL);make(SaveThermal,L"BUTTON",L"Guardar limites");
  label(10,L"TEMPERATURA GPU");label(12,L"POTÊNCIA");label(13,L"LIMITE DE POTÊNCIA");label(TemperatureMetric,L"—");label(PowerMetric,L"—");label(LimitMetric,L"—");
  make(Details,L"COMBOBOX",L"",CBS_DROPDOWNLIST);SendMessageW(controls[Details],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Registo / mensagens"));SendMessageW(controls[Details],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Telemetria NVIDIA"));SendMessageW(controls[Details],CB_SETCURSEL,0,0);
  make(Telemetry,L"EDIT",L"A ler sensores…",ES_MULTILINE|ES_READONLY|WS_VSCROLL|ES_AUTOVSCROLL);make(Log,L"EDIT",L"",ES_MULTILINE|ES_READONLY|WS_VSCROLL|ES_AUTOVSCROLL|WS_HSCROLL|ES_AUTOHSCROLL);ShowWindow(controls[Telemetry],SW_HIDE);
  for(int id:{1})SendMessageW(controls[id],WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
  for(int id:std::vector<int>{30,31,32,33,34,TemperatureMetric,PowerMetric,LimitMetric})SendMessageW(controls[id],WM_SETFONT,reinterpret_cast<WPARAM>(cardFont),TRUE);
  for(int id:std::vector<int>{2,3,4,5,6,7,8,9,10,12,13,14,Identity})SendMessageW(controls[id],WM_SETFONT,reinterpret_cast<WPARAM>(labelFont),TRUE);
  for(int id:{Log,Telemetry})SendMessageW(controls[id],WM_SETFONT,reinterpret_cast<WPARAM>(monoFont),TRUE);
  SendMessageW(controls[Log],EM_SETLIMITTEXT,400000,0);SendMessageW(controls[Telemetry],EM_SETLIMITTEXT,300000,0);
  for(int id:{Server,Slot,Puzzle,Duration,Warning,Critical})SendMessageW(controls[id],EM_SETLIMITTEXT,id==Server?16384:id==Slot?32:8,0);
  if(!smoke){for(const auto backend:{"cpu","cuda","opencl"}){const auto path=cli.parent_path()/"profiles"/backend/"result.json";if(std::filesystem::exists(path)){try{addProfile(path);}catch(const std::exception& e){append(std::string("Perfil: ")+e.what()+"\n");}}}}
  controlAction=UiAction::ThermalRead;try{control.start(cli,{"thermal"});}catch(const std::exception& e){append(e.what());}
  applyTheme();SetTimer(window,1,150,nullptr);update();
 }
 void layout(){
  RECT r{};GetClientRect(window,&r);const int width=r.right,height=r.bottom;contentWidth=(std::min)(1240,width-40);contentLeft=(width-contentWidth)/2;
  scroll=(std::clamp)(scroll,0,(std::max)(0,pageHeight-height));SCROLLINFO info{sizeof(SCROLLINFO),SIF_RANGE|SIF_PAGE|SIF_POS,0,pageHeight-1,static_cast<UINT>(height),scroll,0};SetScrollInfo(window,SB_VERT,&info,TRUE);
  const int left=contentLeft+18,full=contentWidth-36,right=left+full;
  auto move=[&](int id,int x,int y,int w,int h=28){MoveWindow(controls[id],x,y-scroll,(std::max)(20,w),h,TRUE);};
  cards={{30,{contentLeft,126-scroll,contentLeft+contentWidth,288-scroll}},{31,{contentLeft,306-scroll,contentLeft+contentWidth,666-scroll}},{32,{contentLeft,684-scroll,contentLeft+contentWidth,840-scroll}},{33,{contentLeft,858-scroll,contentLeft+contentWidth,942-scroll}},{34,{contentLeft,960-scroll,contentLeft+contentWidth,1210-scroll}}};
  move(1,contentLeft+202,22,full-430,36);move(2,contentLeft+202,66,full-430,36);move(14,right-238,28,90);move(Theme,right-238,52,100,250);move(Badge,right-128,50,128,36);
  move(30,left,144,full-140);move(Refresh,right-122,142,122,32);move(State,left,180,full,24);
  const int cell=(full-24)/3;for(const auto [label,value,index]:std::vector<std::tuple<int,int,int>>{{10,TemperatureMetric,0},{12,PowerMetric,1},{13,LimitMetric,2}}){move(label,left+index*(cell+12)+14,220,cell-28,18);move(value,left+index*(cell+12)+14,242,cell-28,28);}
  move(31,left,324,full);move(3,left,360,full);move(Profile,left,382,full-146,320);move(OpenProfile,right-132,382,132,34);move(Identity,left,427,full,56);
  move(4,left,490,full-212);move(Server,left,512,full-212,32);move(5,right-196,490,196);move(Slot,right-196,512,196,32);
  move(6,left,554,85,18);move(Puzzle,left,575,85,30);move(7,left+104,554,120,18);move(Duration,left+104,575,120,30);move(Once,left+246,575,205,30);
  move(Start,left,618,115,34);move(SafeStop,right-287,618,177,34);move(Stop,right-98,618,98,34);
  move(32,left,702,full);move(Enabled,left,740,225);move(CriticalStop,left,786,240);move(8,left+268,738,82,18);move(Warning,left+268,760,85,32);move(9,left+370,738,85,18);move(Critical,left+370,760,85,32);move(SaveThermal,right-150,784,150,34);
  move(33,left,876,full);move(Preflight,left,900,145,32);move(Doctor,left+157,900,174,32);move(Recover,left+343,900,126,32);
  move(34,left,978,full-270);move(Details,right-250,974,250,300);move(Log,left,1018,full,172);move(Telemetry,left,1018,full,172);
  InvalidateRect(window,nullptr,FALSE);
 }
 void command(int id,int notification){
  if(notification==EN_SETFOCUS||notification==BN_SETFOCUS||notification==CBN_SETFOCUS){if(controls.count(id))ensureVisible(controls[id]);}
  if(id==Theme&&notification==CBN_SELCHANGE){dark=SendMessageW(controls[Theme],CB_GETCURSEL,0,0)==1;applyTheme();return;}
  if(id==Details&&notification==CBN_SELCHANGE){const bool telemetry=SendMessageW(controls[Details],CB_GETCURSEL,0,0)==1;ShowWindow(controls[Telemetry],telemetry?SW_SHOW:SW_HIDE);ShowWindow(controls[Log],telemetry?SW_HIDE:SW_SHOW);return;}
  if((id==Server||id==Puzzle)&&notification==EN_CHANGE){if(controls.count(Badge)&&controls.count(Log))update();return;}
  if(id==Profile&&notification==CBN_SELCHANGE){chooseProfile();return;}
  if(id==Slot&&notification==EN_CHANGE){if(!runtime.busy()){status={};nextRefresh=0;}return;}
  if(notification!=BN_CLICKED)return;
  if(checkboxIds.count(id)){checkmark(id,!checked(id));return;}
  if(id==OpenProfile){wchar_t name[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;dialog.lpstrFilter=L"Perfis medidos (*.json)\0*.json\0\0";dialog.lpstrFile=name;dialog.nMaxFile=32768;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(GetOpenFileNameW(&dialog))addProfile(name);}
  if(id==Preflight)action(UiAction::Preflight);if(id==Start)action(UiAction::Start);if(id==Stop)action(UiAction::Stop);if(id==SafeStop)action(UiAction::SafeStop);if(id==Recover)action(UiAction::Recover);if(id==Refresh)nextRefresh=0;if(id==Doctor&&!control.busy()){controlOutput.clear();controlAction=UiAction::Status;control.start(cli,{"doctor","--offline"});}if(id==SaveThermal&&!control.busy()&&!runtime.busy())saveThermal();update();
 }
};
LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l){
 auto* p=reinterpret_cast<Panel*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
 if(message==WM_NCCREATE){p=static_cast<Panel*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);p->window=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}
 try{if(p){
  if(message==WM_CREATE){p->build();return 0;}
  if(message==WM_SIZE&&!p->controls.empty()){p->layout();return 0;}
  if(message==WM_COMMAND){p->command(LOWORD(w),HIWORD(w));return 0;}
  if(message==WM_TIMER){p->tick();return 0;}
  if(message==WM_ERASEBKGND){return 1;}
  if(message==WM_PAINT){PAINTSTRUCT paint{};auto dc=BeginPaint(hwnd,&paint);p->paint(dc);EndPaint(hwnd,&paint);return 0;}
  if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLOREDIT||message==WM_CTLCOLORLISTBOX){return p->color(reinterpret_cast<HDC>(w),reinterpret_cast<HWND>(l));}
  if(message==WM_MEASUREITEM){reinterpret_cast<MEASUREITEMSTRUCT*>(l)->itemHeight=30;return TRUE;}
  if(message==WM_DRAWITEM){p->draw(reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;}
  if(message==WM_VSCROLL){int value=p->scroll;const auto code=LOWORD(w);if(code==SB_LINEUP)value-=30;if(code==SB_LINEDOWN)value+=30;if(code==SB_PAGEUP)value-=300;if(code==SB_PAGEDOWN)value+=300;if(code==SB_THUMBTRACK||code==SB_THUMBPOSITION){SCROLLINFO info{sizeof(SCROLLINFO),SIF_TRACKPOS};GetScrollInfo(hwnd,SB_VERT,&info);value=info.nTrackPos;}p->scrollTo(value);return 0;}
  if(message==WM_MOUSEWHEEL){p->scrollTo(p->scroll-GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*90);return 0;}
  if(message==WM_GETMINMAXINFO){auto* info=reinterpret_cast<MINMAXINFO*>(l);info->ptMinTrackSize={820,620};return 0;}
  if(message==WM_CLOSE){if(p->runtime.busy()||p->control.busy()){
   if(!p->smoke)MessageBoxW(hwnd,L"Há uma pesquisa ou um comando ativo. Usa Parar e aguarda a conclusão antes de fechar.",L"OpenPuzzle",MB_OK|MB_ICONINFORMATION);return 0;}
   DestroyWindow(hwnd);return 0;}
  if(message==WM_DESTROY){KillTimer(hwnd,1);PostQuitMessage(p->result);return 0;}
 }}catch(const std::exception& e){if(p){if(p->smoke){p->result=1;DestroyWindow(hwnd);}else p->error(e);}return message==WM_CREATE?-1:0;}
 return DefWindowProcW(hwnd,message,w,l);
}
}
int runWindowsUi(const std::filesystem::path& cli,bool smokeTest){
 Panel panel(cli,smokeTest);WNDCLASSW type{};type.lpfnWndProc=procedure;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"OpenPuzzleWindowsPanel";type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
 if(!RegisterClassW(&type)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 1;
 const auto window=CreateWindowExW(0,type.lpszClassName,L"OpenPuzzle",WS_OVERLAPPEDWINDOW|WS_VSCROLL|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1120,860,nullptr,nullptr,type.hInstance,&panel);
 if(!window)return 1;ShowWindow(window,smokeTest?SW_HIDE:SW_SHOW);UpdateWindow(window);
 MSG message{};int status;while((status=GetMessageW(&message,nullptr,0,0))>0){if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
 return status<0?1:static_cast<int>(message.wParam);
}
}
