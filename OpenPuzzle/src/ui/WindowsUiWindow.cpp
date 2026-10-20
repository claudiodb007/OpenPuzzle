#include "openpuzzle/ui/WindowsUiWindow.hpp"
#include "openpuzzle/ui/WindowsUiProcess.hpp"
#include "openpuzzle/ui/WindowsUiModel.hpp"
#include "openpuzzle/ui/WindowsUiProfileStore.hpp"
#include "openpuzzle/ui/GpuControlUiModel.hpp"
#include "openpuzzle/ui/WindowsUiTheme.hpp"
#include "openpuzzle/ui/WindowsUiLanguageStore.hpp"
#include "openpuzzle/services/PuzzleMetadataCatalog.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#include <commdlg.h>
// WIN32_LEAN_AND_MEAN omits COM declarations required by the Windows SDK GDI+ headers.
#include <objidl.h>
#include <gdiplus.h>
#include <uxtheme.h>
#include <fstream>
#include <iostream>
#include <map>
#include <chrono>
#include <memory>
#include <set>
#include <tuple>
namespace openpuzzle::ui {
namespace {
enum Id {Profile=100,OpenProfile,Server,Slot,Puzzle,Duration,Once,Preflight,Start,Stop,SafeStop,Recover,Refresh,Enabled,CriticalStop,Warning,Critical,SaveThermal,State,Identity,Telemetry,Log,Theme,Details,Doctor,Badge,TemperatureMetric,PowerMetric,LimitMetric,Language,Benchmark,GpuPick,GpuRefresh,GpuInfo,GpuPowerLabel,GpuPowerValue,GpuPowerApply,GpuPowerDefault,GpuFanLabel,GpuFanMode,GpuFanValue,GpuFanApply,GpuHint,SpeedMetric,BenchmarkSource,BenchmarkDevice,BenchmarkThreads,BenchmarkSourceLabel,BenchmarkDeviceLabel,BenchmarkThreadsLabel};
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
 HWND window=nullptr;std::map<int,HWND> controls;HFONT font=nullptr,titleFont=nullptr,labelFont=nullptr,cardFont=nullptr,monoFont=nullptr;std::filesystem::path cli,fixtureProfile,benchmarkResult,benchmarkBound;std::map<int,std::string> textCache;
 std::optional<PuzzleExecutionMetadata> metadataCache;std::string metadataPuzzle;bool metadataLoaded=false;
 WindowsUiLanguageStore languageStore;UiLanguage language=UiLanguage::English;
 bool dark=false;WindowsUiPalette palette=WindowsUiPalette::get(false);HBRUSH panelBrush=nullptr,inputBrush=nullptr,backBrush=nullptr,softBrush=nullptr;
 std::set<int> checkboxIds;std::map<int,bool> checks;std::map<int,RECT> cards;int scroll=0,wheelRemainder=0,contentLeft=0,contentWidth=0,pageHeight=1598;
 ULONG_PTR gdiplus=0;std::unique_ptr<Gdiplus::Image> logo;
 std::vector<ProfileEntry> profiles;std::optional<ProfileEntry> selected;std::optional<WindowsUiSelection> benchmarkRequest;
 WindowsUiProcess runtime,control,monitor,sensors,gpuReader,gpuWriter;
 std::vector<GpuControlDevice> gpuDevices;std::string gpuOutput,gpuWriteOutput;std::optional<GpuControlRequest> gpuPending;bool gpuVerify=false;int gpuSmokeStep=0;ULONGLONG nextGpuRefresh=0;UiAction controlAction=UiAction::ThermalRead;
 WindowsUiStatus status;std::string firstBackend="cpu",monitorSlot="primary",activeSlot,monitorOutput,controlOutput,sensorOutput,log;
 ULONGLONG nextRefresh=0,opened=GetTickCount64();bool smoke=false,statusProved=false,sensorProved=false,thermalLoaded=false,speedProved=false;int result=0,smokeStage=0;
 explicit Panel(std::filesystem::path path,bool test,const std::wstring& key,std::filesystem::path profile,const std::string& backend):cli(std::move(path)),fixtureProfile(std::move(profile)),languageStore(key),language(languageStore.load()),smoke(test),firstBackend(backend){}
 std::string t(std::string_view key)const{return std::string(uiText(language,key));}
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
 void set(int id,const std::string& value){const bool editable=id==Slot||id==Puzzle||id==Duration||id==Warning||id==Critical||id==GpuPowerValue||id==GpuFanValue;
  if(!editable){auto it=textCache.find(id);if(it!=textCache.end()&&it->second==value)return;}SetWindowTextW(controls.at(id),wide(value).c_str());textCache[id]=value;}
 std::string get(int id){const auto h=controls.at(id);const int length=GetWindowTextLengthW(h);std::wstring value(length+1,L'\0');GetWindowTextW(h,value.data(),length+1);value.resize(length);return utf8(value);}
 bool checked(int id){return checks[id];}
 void checkmark(int id,bool value){if(checks[id]==value)return;checks[id]=value;InvalidateRect(controls[id],nullptr,FALSE);}
 std::optional<PuzzleExecutionMetadata> puzzleMetadata(bool fresh=false){
  const auto requested=get(Puzzle);if(!fresh&&metadataLoaded&&metadataPuzzle==requested)return metadataCache;metadataLoaded=true;metadataPuzzle=requested;metadataCache.reset();
  try{const auto number=cliIntegerArgument({"--puzzle",get(Puzzle)},{"--puzzle"},0);bool missing=false;
   const auto value=windowsFile::readPrivate(cli.parent_path()/"puzzles"/(std::to_string(number)+".json"),missing,16384);
   metadataCache=value?PuzzleMetadataCatalog::parse(number,*value):std::nullopt;return metadataCache;
  }catch(...){return {};}
 }
 void append(const std::string& value){if(value.empty())return;log+=value;if(log.size()>196608)log.erase(0,log.size()-131072);
  renderLog();
 }
 void renderLog(){const auto translated=uiDisplayText(language,log);std::string display;display.reserve(translated.size()*2);for(char c:translated){if(c=='\r')continue;if(c=='\n')display+='\r';display+=c;}set(Log,display);SendMessageW(controls[Log],EM_SETSEL,static_cast<WPARAM>(-1),static_cast<LPARAM>(-1));SendMessageW(controls[Log],EM_SCROLLCARET,0,0);
 }
 void error(const std::exception& e){append(std::string("\nErro: ")+e.what()+"\n");if(!smoke)MessageBoxW(window,wide(uiDisplayText(language,e.what())).c_str(),L"OpenPuzzle",MB_OK|MB_ICONERROR);}
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
  SendMessageW(controls[BenchmarkSource],CB_SETCURSEL,0,0);benchmarkInputs();
  profileIdentity();
  status={};nextRefresh=0;sensorMetrics();update();
 }
 std::filesystem::path profileRoot()const{return smoke?localPaths::dataDirectory()/pathFromUtf8(firstBackend=="cpu"?"ui-profiles-fixture":"ui-profiles-fixture-"+firstBackend):cli.parent_path();}
 void benchmarkInputs(){
  const auto choice=SendMessageW(controls[BenchmarkSource],CB_GETCURSEL,0,0);
  if(choice==0&&selected){set(BenchmarkDevice,std::to_string(selected->profile.device));set(BenchmarkThreads,selected->profile.backend=="cpu"?std::to_string(selected->profile.threads):t("benchmark_automatic"));}
  else {set(BenchmarkDevice,"0");set(BenchmarkThreads,choice==1?"1":t("benchmark_automatic"));}
  update();
 }
 WindowsUiSelection benchmarkSelection(){
  auto s=selection(false);const auto choice=SendMessageW(controls[BenchmarkSource],CB_GETCURSEL,0,0);
  if(choice==0){
   if(!selected)throw std::runtime_error(t("benchmark_choose_backend"));
   bool missing=false;const auto contents=windowsFile::readPrivate(selected->path,missing,16384);
   if(!contents||*contents!=selected->contents)throw std::runtime_error(t("profile_changed"));
  }else{
   if(choice<1||choice>3)throw std::runtime_error(t("benchmark_choose_backend"));
   WindowsUiBenchmarkParameters b;b.backend=choice==1?"cpu":choice==2?"cuda":"opencl";
   b.device=b.backend=="cpu"?0:cliIntegerArgument({"--device",get(BenchmarkDevice)},{"--device"},-1);
   b.automatic=b.backend!="cpu";
   b.threads=b.automatic?256:cliIntegerArgument({"--threads",get(BenchmarkThreads)},{"--threads"},0);
   s.benchmark=b;s.profile.reset();s.profilePath.clear();
  }
  (void)windowsUiCommand(UiAction::BenchmarkPreview,s);
  s.slot=windowsUiBenchmarkSlot(windowsUiBenchmarkParameters(s));return s;
 }
 void profileIdentity(){if(!selected){set(Identity,t("benchmark_choose_backend"));return;}const auto& p=selected->profile;set(Identity,p.deviceIdentity+"\nUUID: "+(p.gpuUuid.empty()?"—":p.gpuUuid)+"   PCI: "+(p.pciBusId.empty()?"—":p.pciBusId)+"\n"+t("profile")+": "+pathUtf8(selected->path));}
 void applyLanguage(){
  for(const auto& [id,key]:std::vector<std::pair<int,std::string_view>>{{2,"windows_subtitle"},{14,"theme"},{15,"language"},{30,"current_state"},{31,"new_execution"},{32,"thermal_safety"},{33,"tools"},{34,"details"},{3,"profile_label"},{OpenProfile,"open_profile"},{5,"slot"},{6,"puzzle"},{7,"duration_minutes"},{Once,"one_assignment"},{Preflight,"preflight"},{Start,"start"},{Stop,"stop"},{SafeStop,"safe_stop"},{Benchmark,"benchmark"},{Recover,"recover"},{Refresh,"refresh"},{Doctor,"offline_doctor"},{Enabled,"thermal_enable_short"},{CriticalStop,"critical_stop_short"},{8,"warning_c"},{9,"critical_c"},{SaveThermal,"save_limits"},{16,"last_search_speed"},{10,"gpu_temperature"},{12,"power_upper"},{13,"power_limit_upper"}})set(id,t(key));
  for(const auto& [id,key]:std::vector<std::pair<int,std::string_view>>{{BenchmarkSourceLabel,"backend"},{BenchmarkDeviceLabel,"device"},{BenchmarkThreadsLabel,"benchmark_threads"},{35,"gpu_controls"},{GpuRefresh,"refresh"},{GpuPowerLabel,"gpu_power_limit"},{GpuPowerApply,"gpu_apply"},{GpuPowerDefault,"gpu_default"},{GpuFanLabel,"gpu_fan_mode"},{GpuFanApply,"gpu_apply"},{GpuHint,"gpu_controls_hint"}})set(id,t(key));
  const auto sourceChoice=SendMessageW(controls[BenchmarkSource],CB_GETCURSEL,0,0);SendMessageW(controls[BenchmarkSource],CB_RESETCONTENT,0,0);
  for(const auto name:{t("benchmark_from_profile"),std::string("CPU"),std::string("CUDA"),std::string("OpenCL")})SendMessageW(controls[BenchmarkSource],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(name).c_str()));
  SendMessageW(controls[BenchmarkSource],CB_SETCURSEL,sourceChoice>=0?sourceChoice:selected?0:2,0);
  if(sourceChoice<0&&selected){set(BenchmarkDevice,std::to_string(selected->profile.device));set(BenchmarkThreads,selected->profile.backend=="cpu"?std::to_string(selected->profile.threads):t("benchmark_automatic"));}
  const auto fanChoice=SendMessageW(controls[GpuFanMode],CB_GETCURSEL,0,0);SendMessageW(controls[GpuFanMode],CB_RESETCONTENT,0,0);
  for(const auto key:{"gpu_fan_auto","gpu_fan_manual"})SendMessageW(controls[GpuFanMode],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(t(key)).c_str()));
  SendMessageW(controls[GpuFanMode],CB_SETCURSEL,fanChoice==1?1:0,0);renderGpu(false);
  const auto detail=SendMessageW(controls[Details],CB_GETCURSEL,0,0);
  for(int id:{Theme,Details})SendMessageW(controls[id],CB_RESETCONTENT,0,0);
  for(const auto key:{"light","dark"})SendMessageW(controls[Theme],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(t(key)).c_str()));
  for(const auto key:{"log_messages","nvidia_telemetry"})SendMessageW(controls[Details],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(t(key)).c_str()));
  SendMessageW(controls[Theme],CB_SETCURSEL,dark?1:0,0);SendMessageW(controls[Details],CB_SETCURSEL,detail==1?1:0,0);
  SendMessageW(controls[Language],CB_SETCURSEL,static_cast<unsigned>(language),0);
  profileIdentity();renderLog();set(Telemetry,sensorOutput.empty()?t("reading_sensors"):uiDisplayText(language,sensorOutput));update();InvalidateRect(window,nullptr,TRUE);
 }
 WindowsUiSelection selection(bool numbers=true){
  WindowsUiSelection s;s.server=windowsUiServer;s.slot=activeSlot.empty()?get(Slot):activeSlot;
  if(selected){s.profile=selected->profile;s.profilePath=pathUtf8(selected->path);}
  if(numbers){s.puzzle=cliIntegerArgument({"--puzzle",get(Puzzle)},{"--puzzle"},0);s.duration=cliIntegerArgument({"--duration",get(Duration)},{"--duration"},0);}
  s.once=checked(Once);return s;
 }
 void update(){
  const bool lock=runtime.busy()||control.busy();for(int id:{Profile,OpenProfile,Slot,Puzzle,Duration,Once})EnableWindow(controls[id],!lock);
  updateGpu();
  const auto metadata=puzzleMetadata();
  EnableWindow(controls[Start],!lock&&selected.has_value()&&status.canStart()&&metadata&&metadata->searchMode=="linear");EnableWindow(controls[Preflight],!lock&&selected.has_value());
  EnableWindow(controls[Benchmark],!lock&&status.canStart());
  const auto benchmarkChoice=SendMessageW(controls[BenchmarkSource],CB_GETCURSEL,0,0);
  EnableWindow(controls[BenchmarkSource],!lock);
  EnableWindow(controls[BenchmarkDevice],!lock&&benchmarkChoice>1);
  EnableWindow(controls[BenchmarkThreads],!lock&&benchmarkChoice==1);
  EnableWindow(controls[Stop],(control.busy()&&controlAction==UiAction::Benchmark)||(!control.busy()&&(runtime.busy()||status.running)));EnableWindow(controls[SafeStop],!control.busy()&&(runtime.busy()||status.running));
  EnableWindow(controls[Recover],!lock&&status.known&&!status.running&&status.preserved);
  for(int id:{Enabled,CriticalStop,Warning,Critical,SaveThermal})EnableWindow(controls[id],!lock&&thermalLoaded);
  set(State,t(control.busy()&&(controlAction==UiAction::BenchmarkPreflight||controlAction==UiAction::BenchmarkStatus)?"preflight":control.busy()&&controlAction==UiAction::Benchmark?"benchmark_running":control.busy()&&controlAction==UiAction::BenchmarkBind?"benchmark_binding":runtime.busy()?"state_searching":!status.known?"state_unknown":status.running?"state_slot_active":status.preserved?"state_preserved":!metadata?"state_no_puzzle":"state_ready"));
  set(SpeedMetric,status.progress.display());
  set(Badge,t(runtime.busy()||status.running?"badge_running":!status.known?"badge_checking":status.preserved?"badge_preserved":"badge_stopped"));
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
   const auto metadata=puzzleMetadata(true);if(!metadata||metadata->searchMode!="linear")throw std::runtime_error("Puzzle não disponível neste painel");
   const auto args=windowsUiStartCommand(s);
   if(!smoke&&MessageBoxW(window,wide(uiFormat(language,"start_question",std::to_string(s.puzzle),s.slot)).c_str(),wide(t("start_title")).c_str(),MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
   runtime.start(cli,args,true);activeSlot=s.slot;append("\nA iniciar pesquisa; aguarda a confirmação da CLI.\n");
  }else{
   if(which==UiAction::Recover&&(!status.known||status.running))throw std::runtime_error("Recuperação bloqueada enquanto o estado não for confirmado");
   if(which==UiAction::Recover)s.server.clear(); // Existing work keeps its persisted server.
   if(which==UiAction::Recover&&!smoke&&MessageBoxW(window,wide(t("recover_question")).c_str(),wide(t("recover")).c_str(),MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
   controlAction=which;controlOutput.clear();control.start(cli,windowsUiCommand(which,s),false,which==UiAction::Recover?120000:60000);
  }update();
 }
 void benchmark(){
  if(runtime.busy()||control.busy()||!status.canStart())throw std::runtime_error(t("benchmark_busy"));
  const auto s=benchmarkSelection();const auto profile=windowsUiBenchmarkParameters(s);
  std::string parameters=profile.backend+" · "+t("device")+": "+std::to_string(profile.device)+"\n";
  if(profile.automatic)parameters+=t("benchmark_auto_description");
  else parameters+=t("benchmark_threads")+": "+std::to_string(profile.threads);
  const auto identity=s.profile?s.profile->deviceIdentity:t("benchmark_new_profile");
  if(!smoke&&MessageBoxW(window,wide(uiFormat(language,profile.automatic?"benchmark_auto_question":"benchmark_prepare_question",identity,parameters)).c_str(),wide(t("benchmark")).c_str(),MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
  // Keep the exact reviewed request across asynchronous status/preview/measure.
  benchmarkRequest=s;benchmarkResult.clear();benchmarkBound.clear();controlOutput.clear();
  controlAction=UiAction::BenchmarkStatus;control.start(cli,windowsUiCommand(UiAction::Status,s),false,60000);
  append("\n"+t("benchmark_scope")+"\n");update();
 }
 void selectBenchmarkResult(const std::filesystem::path& source){
  bool missing=false;const auto raw=windowsFile::readPrivate(source,missing,16384);
  if(!raw)throw std::runtime_error("Benchmark result missing");
  addProfile(WindowsRunProfile::parse(*raw).autoTuned?storeWindowsUiAutomaticProfile(profileRoot(),source):storeFirstWindowsUiProfile(profileRoot(),source));benchmarkRequest.reset();append("\n"+t("benchmark_saved")+"\n");
 }
 void finishBenchmark(){
  if(!benchmarkRequest)throw std::runtime_error("Benchmark request unavailable");
  const auto report=pathFromUtf8(windowsUiBenchmarkReport(controlOutput));
  const auto expected=localPaths::dataDirectory()/"engine-diagnostics";
  if(!report.is_absolute()||!windowsFile::safePath(report)||report.parent_path()!=expected)throw std::runtime_error("Unexpected benchmark report directory");
  benchmarkResult=report/"result.json";bool missing=false;const auto raw=windowsFile::readPrivate(benchmarkResult,missing,16384);
  if(!raw)throw std::runtime_error("Measured benchmark profile unavailable");const auto measured=WindowsRunProfile::parse(*raw);
  if(!windowsUiBenchmarkMatches(measured,windowsUiBenchmarkParameters(*benchmarkRequest)))throw std::runtime_error("Benchmark result does not match the reviewed device/parameters");
  if(measured.backend=="cpu"){selectBenchmarkResult(benchmarkResult);return;}
  benchmarkBound=report/"ui-bound-profile.json";controlOutput.clear();controlAction=UiAction::BenchmarkBind;
  control.start(cli,{"bind-profile","--profile",pathUtf8(benchmarkResult),"--output",pathUtf8(benchmarkBound)},false,120000);
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
 const GpuControlDevice* selectedGpu(){const auto index=SendMessageW(controls[GpuPick],CB_GETCURSEL,0,0);return index>=0&&static_cast<std::size_t>(index)<gpuDevices.size()?&gpuDevices[index]:nullptr;}
 void updateGpu(){
  if(!controls.count(GpuPick))return;const auto* d=selectedGpu();const bool ready=!gpuReader.busy()&&!gpuWriter.busy();
  EnableWindow(controls[GpuPick],ready);EnableWindow(controls[GpuRefresh],ready);
  const bool power=ready&&d&&d->power;EnableWindow(controls[GpuPowerValue],power);EnableWindow(controls[GpuPowerApply],power);
  EnableWindow(controls[GpuPowerDefault],power&&gpuControlRange(d->powerDefault,d->powerMin,d->powerMax));
  EnableWindow(controls[GpuFanMode],ready&&d&&d->automatic);const bool manual=SendMessageW(controls[GpuFanMode],CB_GETCURSEL,0,0)==1;
  EnableWindow(controls[GpuFanValue],ready&&d&&d->fan&&manual);EnableWindow(controls[GpuFanApply],ready&&d&&d->automatic&&(!manual||(d->fan&&gpuControlRange(d->temperature,0,79.999))));
 }
 void renderGpu(bool inputs){
  if(!controls.count(GpuPick))return;const auto* d=selectedGpu();if(!d){set(GpuInfo,t("gpu_no_controls"));updateGpu();return;}
  std::ostringstream text;text<<d->pci<<"  "<<d->uuid<<"\n"<<t("gpu_current")<<": ";
  if(d->power)text<<d->powerNow<<' '<<d->powerUnit<<"  ("<<t("gpu_range")<<": "<<d->powerMin<<"–"<<d->powerMax<<' '<<d->powerUnit<<")";else text<<t("unavailable");
  text<<"  |  "<<t("gpu_fan_mode")<<": "<<t(d->fanMode=="auto"?"gpu_fan_auto":d->fanMode=="manual"?"gpu_fan_manual":"gpu_mixed")<<" "<<d->fanNow<<"%  |  ";
  if(d->temperature>=0)text<<d->temperature<<" °C";else text<<t("unavailable");text<<"\n"<<t("gpu_permission");set(GpuInfo,text.str());
  set(GpuPowerLabel,t("gpu_power_limit")+" ("+d->powerUnit+")");
  if(inputs){std::ostringstream value;value<<d->powerNow;set(GpuPowerValue,value.str());set(GpuFanValue,std::to_string(static_cast<int>((std::clamp)(std::round(d->fanNow),30.0,100.0))));SendMessageW(controls[GpuFanMode],CB_SETCURSEL,d->fanMode=="manual"?1:0,0);}
  updateGpu();
 }
 void readGpu(){if(gpuReader.busy()||gpuWriter.busy())return;gpuOutput.clear();gpuReader.start(cli,{"gpu-control","list"},false,15000);nextGpuRefresh=GetTickCount64()+10000;updateGpu();}
 void applyGpu(int id){
  const auto* d=selectedGpu();if(!d||gpuReader.busy()||gpuWriter.busy())return;
  const bool manual=SendMessageW(controls[GpuFanMode],CB_GETCURSEL,0,0)==1;
  const auto action=id==GpuPowerDefault?GpuControlAction::PowerDefault:id==GpuPowerApply?GpuControlAction::Power:manual?GpuControlAction::FanManual:GpuControlAction::FanAuto;
  const double value=action==GpuControlAction::Power?gpuControlNumber(get(GpuPowerValue)):action==GpuControlAction::FanManual?gpuControlNumber(get(GpuFanValue)):0;
  const auto args=gpuControlArguments(*d,action,value);std::string review=t("gpu_apply_confirm")+"\n\n"+d->name+"\nPCI: "+d->pci+"\n";
  if(action==GpuControlAction::Power)review+=t("gpu_power_limit")+": "+get(GpuPowerValue)+" "+d->powerUnit;
  else if(action==GpuControlAction::PowerDefault)review+=t("gpu_default")+": "+std::to_string(d->powerDefault)+" "+d->powerUnit;
  else review+=t(manual?"gpu_fan_manual":"gpu_fan_auto")+(manual?" "+get(GpuFanValue)+"%":"");
  if(!smoke&&MessageBoxW(window,wide(review).c_str(),wide(t("gpu_controls")).c_str(),MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
  gpuWriteOutput.clear();gpuPending=GpuControlRequest{d->vendor,d->pci,d->uuid,action,value};
  if(smoke)gpuWriter.start(cli,args,false,20000);else gpuWriter.startGpuControlElevated(window,cli.parent_path()/"openpuzzle-gpu-control.exe",{args.begin()+1,args.end()});updateGpu();
 }
 void tick(){
  auto gr=gpuReader.poll();if(gpuOutput.size()<65536)gpuOutput+=gr.output;
  if(gr.finished){if(gr.exitCode==0){try{const auto* prior=selectedGpu();const auto pci=prior?prior->pci:std::string();gpuDevices=parseGpuControls(gpuOutput);SendMessageW(controls[GpuPick],CB_RESETCONTENT,0,0);int index=0;
    for(std::size_t i=0;i<gpuDevices.size();++i){const auto& d=gpuDevices[i];const auto title=d.name+" / "+d.pci;SendMessageW(controls[GpuPick],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(title).c_str()));if(d.pci==pci)index=static_cast<int>(i);}
    SendMessageW(controls[GpuPick],CB_SETCURSEL,index,0);renderGpu(pci.empty());
    if(gpuVerify&&gpuPending){const auto r=*gpuPending;const GpuControlDevice* actual=nullptr;for(const auto& d:gpuDevices)if(d.pci==r.pci&&d.vendor==r.vendor&&d.uuid==r.uuid)actual=&d;
     bool matched=actual!=nullptr;
     if(actual){if(r.action==GpuControlAction::Power)matched=actual->power&&std::abs(actual->powerNow-r.value)<0.001;
      else if(r.action==GpuControlAction::PowerDefault)matched=actual->power&&std::abs(actual->powerNow-actual->powerDefault)<0.001;
      else if(r.action==GpuControlAction::FanAuto)matched=actual->fanMode=="auto";
      else matched=actual->fanMode=="manual"&&std::abs(actual->fanNow-r.value)<=0.5;}
     append("\n"+t(matched?"gpu_applied":"gpu_failed")+"\n");gpuVerify=false;gpuPending.reset();
    }
   }catch(const std::exception& e){gpuDevices.clear();set(GpuInfo,t("gpu_failed"));append(std::string(e.what())+"\n");}}
   else {gpuDevices.clear();set(GpuInfo,t("gpu_failed"));}updateGpu();}
  auto gw=gpuWriter.poll();if(gpuWriteOutput.size()<65536)gpuWriteOutput+=gw.output;
  if(gw.finished){gpuVerify=gw.exitCode==0;if(!gpuVerify){append("\n"+t(gw.exitCode==69?"gpu_not_supported":gw.exitCode==77?"gpu_permission_denied":gw.exitCode==70?"gpu_rollback_failed":gw.exitCode==78?"gpu_reset_needed":"gpu_failed")+" ("+std::to_string(gw.exitCode)+")\n"+gpuWriteOutput);gpuPending.reset();}nextGpuRefresh=0;readGpu();}
  if(!smoke&&GetTickCount64()>=nextGpuRefresh)readGpu();
  auto r=runtime.poll();append(r.output);if(r.finished){append("\nPesquisa terminou. Código: "+std::to_string(r.exitCode)+"\n");activeSlot.clear();status={};nextRefresh=0;}
  auto c=control.poll();if(controlOutput.size()<262144)controlOutput+=c.output;append(c.output);
  if(c.finished){const auto finishedAction=controlAction;append("\nComando terminou. Código: "+std::to_string(c.exitCode)+(c.timedOut?" ("+t("time_limit")+")":"")+"\n");
   if(c.exitCode==0){if(finishedAction==UiAction::ThermalRead||finishedAction==UiAction::ThermalSave)readThermal();
    else if(finishedAction==UiAction::BenchmarkStatus){
     if(!benchmarkRequest||!WindowsUiStatus::parse(c.exitCode,controlOutput,benchmarkRequest->slot).canStart())throw std::runtime_error(t("benchmark_busy"));
     controlOutput.clear();controlAction=UiAction::BenchmarkPreflight;
     control.start(cli,windowsUiCommand(benchmarkRequest->profile?UiAction::Preflight:UiAction::BenchmarkPreview,*benchmarkRequest),false,120000);
    }
    else if(finishedAction==UiAction::BenchmarkPreflight){if(!benchmarkRequest)throw std::runtime_error("Benchmark request unavailable");controlOutput.clear();controlAction=UiAction::Benchmark;control.start(cli,windowsUiCommand(UiAction::Benchmark,*benchmarkRequest),false,windowsUiBenchmarkParameters(*benchmarkRequest).automatic?3600000:600000);}
    else if(finishedAction==UiAction::Benchmark)finishBenchmark();else if(finishedAction==UiAction::BenchmarkBind){
     bool missing=false;const auto value=windowsFile::readPrivate(benchmarkBound,missing,16384);
     if(!benchmarkRequest||!value||!windowsUiBenchmarkMatches(WindowsRunProfile::parse(*value),windowsUiBenchmarkParameters(*benchmarkRequest)))throw std::runtime_error("Bound benchmark result differs from reviewed parameters");
     selectBenchmarkResult(benchmarkBound);
    }}nextRefresh=0;
  }
  auto m=monitor.poll();if(monitorOutput.size()<262144)monitorOutput+=m.output;
  if(m.finished){
   const auto current=activeSlot.empty()?get(Slot):activeSlot;
   if(current==monitorSlot){status=WindowsUiStatus::parse(m.exitCode,monitorOutput,monitorSlot);statusProved=status.known;if(!status.known)append("Estado indisponível: "+monitorOutput+"\n");}
  }
  auto reading=sensors.poll();if(sensorOutput.size()<262144)sensorOutput+=reading.output;
  if(reading.finished){sensorMetrics();set(Telemetry,(reading.exitCode==0?"":t("read_unavailable")+"\n")+uiDisplayText(language,sensorOutput));sensorProved=reading.exitCode==0;}
  if(GetTickCount64()>=nextRefresh)refresh();update();
  if(smoke&&smokeStage==1&&status.running&&get(SpeedMetric)=="1379.58 MKey/s")speedProved=true;
  if(smoke&&fixtureProfile.empty()){
   if(GetTickCount64()-opened>20000)throw std::runtime_error("First-profile fixture timeout");
   if(statusProved&&sensorProved&&thermalLoaded&&smokeStage==0){
    if(selected){
     if(selected->profile.backend!=firstBackend||selected->profile.speed!=(firstBackend=="cpu"?2.0:3.88)||log.find("benchmark-fixture-finished")!=std::string::npos)throw std::runtime_error("First profile did not reload without another benchmark");
     result=0;DestroyWindow(window);return;
    }
    if(!IsWindowEnabled(controls[Benchmark])||IsWindowEnabled(controls[Start]))throw std::runtime_error("Empty-profile benchmark/start gating failed");
    SendMessageW(controls[BenchmarkSource],CB_SETCURSEL,firstBackend=="cpu"?1:firstBackend=="cuda"?2:3,0);benchmarkInputs();
    if(firstBackend!="cpu"&&(IsWindowEnabled(controls[BenchmarkThreads])||get(BenchmarkThreads)!=t("benchmark_automatic")))throw std::runtime_error("GPU thread count was not automatic");
    benchmark();smokeStage=1;
    if(IsWindowEnabled(controls[Start])||IsWindowEnabled(controls[BenchmarkSource]))throw std::runtime_error("First benchmark did not lock conflicting controls");
   }else if(smokeStage==1&&!control.busy()){
    if(!selected||selected->profile.backend!=firstBackend||selected->profile.speed!=(firstBackend=="cpu"?2.0:3.88)||
      (firstBackend=="cpu"?selected->path!=profileRoot()/"profiles"/"cpu"/"result.json":!selected->profile.autoTuned||loadWindowsUiAutomaticProfile(profileRoot(),firstBackend)!=selected->path))throw std::runtime_error("First measured profile was not saved and selected");
    result=0;DestroyWindow(window);
   }
   return;
  }
  if(smoke&&statusProved&&sensorProved&&thermalLoaded&&smokeStage==0){
   if(get(SpeedMetric)!="—")throw std::runtime_error("Idle UI invented a live search speed");
   if(controls.count(Server)||controls.count(4)||selection(false).server!=windowsUiServer||controls.size()<28||IsWindowEnabled(controls[Start])||!IsWindowEnabled(controls[SaveThermal])){result=1;DestroyWindow(window);return;}
   const auto savedLanguage=language;const auto selectedBefore=get(Slot);const auto thermalBefore=get(Warning);
   if(language!=languageStore.load()||get(Start)!=uiText(language,"start"))throw std::runtime_error("Initial persisted language not applied");
   for(unsigned i=0;i<4;++i){SendMessageW(controls[Language],CB_SETCURSEL,i,0);command(Language,CBN_SELCHANGE);
    if(get(SafeStop)!=uiText(static_cast<UiLanguage>(i),"safe_stop")||get(Benchmark)!=uiText(static_cast<UiLanguage>(i),"benchmark")||get(Start)!=uiText(static_cast<UiLanguage>(i),"start")||get(32)!=uiText(static_cast<UiLanguage>(i),"thermal_safety")||languageStore.load()!=static_cast<UiLanguage>(i)||get(Slot)!=selectedBefore||get(Warning)!=thermalBefore||selection(false).server!=windowsUiServer)throw std::runtime_error("Language switch changed state or did not translate controls");}
   SendMessageW(controls[Language],CB_SETCURSEL,static_cast<unsigned>(savedLanguage),0);command(Language,CBN_SELCHANGE);
   SendMessageW(controls[Theme],CB_SETCURSEL,1,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(Theme,CBN_SELCHANGE),reinterpret_cast<LPARAM>(controls[Theme]));
   SetWindowPos(window,nullptr,0,0,840,660,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);scrollTo(450);
   for(int i=0;i<80;++i){scrollTo((i%2)*400);UpdateWindow(window);}scrollTo(450);
   if(!dark||scroll==0||!logo||logo->GetLastStatus()!=Gdiplus::Ok)throw std::runtime_error("Tema, scroll ou logótipo indisponível");
   runtime.start(cli,{"fixture-run"},true);activeSlot="primary";nextRefresh=0;smokeStage=1;
   SendMessageW(controls[Language],CB_SETCURSEL,2,0);command(Language,CBN_SELCHANGE);
   if(!runtime.busy()||get(Start)!="Démarrer"||get(Slot)!=selectedBefore||get(Warning)!=thermalBefore||activeSlot!="primary")throw std::runtime_error("Language switch disturbed an active runtime");
   SendMessageW(controls[Language],CB_SETCURSEL,static_cast<unsigned>(savedLanguage),0);command(Language,CBN_SELCHANGE);
   SendMessageW(window,WM_CLOSE,0,0);if(!IsWindow(window))throw std::runtime_error("A janela fechou durante a pesquisa");
  }else if(smoke&&smokeStage==1&&!runtime.busy()&&!monitor.busy()){
   if(!speedProved||log.find("runtime-finished")==std::string::npos||fixtureProfile.empty())throw std::runtime_error("Runtime fixture did not complete");addProfile(fixtureProfile);smokeStage=2;
  }else if(smoke&&smokeStage==2&&status.canStart()&&!control.busy()){
   if(!IsWindowEnabled(controls[Benchmark]))throw std::runtime_error("Benchmark button not enabled for an idle measured profile");
   command(Benchmark,BN_CLICKED);if(!control.busy()||runtime.busy()||IsWindowEnabled(controls[Start])||IsWindowEnabled(controls[SafeStop]))throw std::runtime_error("Benchmark did not lock conflicting actions");
   SendMessageW(window,WM_CLOSE,0,0);if(!IsWindow(window))throw std::runtime_error("Window closed during benchmark");smokeStage=3;
  }else if(smoke&&smokeStage==3&&!control.busy()){
   if(!selected||selected->path==fixtureProfile||selected->profile.backend!=firstBackend||selected->profile.speed!=(firstBackend=="cpu"?2.0:3.88)||log.find("benchmark-fixture-finished")==std::string::npos)throw std::runtime_error("Benchmark result was not selected");smokeStage=4;
  }else if(smoke&&smokeStage==4&&!gpuReader.busy()&&!gpuWriter.busy()){
   if(gpuDevices.size()!=2||!selectedGpu()||(gpuSmokeStep<4&&(selectedGpu()->pci!="00000000:08:00.0"||selectedGpu()->powerUnit!="W")))throw std::runtime_error("GPU control fixture inspection/selection failed");
   if(gpuSmokeStep==0){set(GpuPowerValue,"250");applyGpu(GpuPowerApply);SendMessageW(window,WM_CLOSE,0,0);if(!IsWindow(window))throw std::runtime_error("Window closed during GPU write");++gpuSmokeStep;}
   else if(gpuSmokeStep==1){if(selectedGpu()->powerNow!=250)throw std::runtime_error("GPU power UI readback failed");SendMessageW(controls[GpuFanMode],CB_SETCURSEL,1,0);set(GpuFanValue,"70");applyGpu(GpuFanApply);++gpuSmokeStep;}
   else if(gpuSmokeStep==2){if(selectedGpu()->fanMode!="manual"||selectedGpu()->fanNow!=70)throw std::runtime_error("GPU manual fan UI readback failed");SendMessageW(controls[GpuFanMode],CB_SETCURSEL,0,0);applyGpu(GpuFanApply);++gpuSmokeStep;}
   else if(gpuSmokeStep==3){if(selectedGpu()->fanMode!="auto")throw std::runtime_error("GPU automatic fan UI readback failed");SendMessageW(controls[GpuPick],CB_SETCURSEL,1,0);renderGpu(true);if(get(GpuPowerValue)!="-10"||selectedGpu()->powerUnit!="%")throw std::runtime_error("AMD UI percent units failed");set(GpuPowerValue,"-20");applyGpu(GpuPowerApply);++gpuSmokeStep;}
   else {if(selectedGpu()->powerNow!=-20)throw std::runtime_error("AMD UI power readback failed");result=0;DestroyWindow(window);}

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
  const int gap=12,inner=contentWidth-36,cell=(inner-3*gap)/4;
  for(int i=0;i<4;++i){RECT metric{contentLeft+18+i*(cell+gap),212-scroll,contentLeft+18+i*(cell+gap)+cell,276-scroll};rounded(dc,metric,palette.soft,palette.border,8);}
 }
 LRESULT color(HDC dc,HWND child){
  const int id=GetDlgCtrlID(child);const bool edit=id==Slot||id==Puzzle||id==Duration||id==Warning||id==Critical||id==Log||id==Telemetry;
  const bool metric=id==16||id==SpeedMetric||id==10||id==12||id==13||id==TemperatureMetric||id==PowerMetric||id==LimitMetric;
  const bool header=id==1||id==2||id==14||id==15;
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
 void scrollTo(int value){RECT r{};GetClientRect(window,&r);const auto next=(std::clamp)(value,0,(std::max)(0,pageHeight-static_cast<int>(r.bottom)));if(next==scroll)return;scroll=next;layout();}
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
  label(15,L"");make(Language,L"COMBOBOX",L"",CBS_DROPDOWNLIST);
  for(const auto name:uiLanguageNames)SendMessageW(controls[Language],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(std::string(name)).c_str()));
  label(35,L"");make(GpuPick,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL);make(GpuRefresh,L"BUTTON",L"");label(GpuInfo,L"");
  label(GpuPowerLabel,L"");make(GpuPowerValue,L"EDIT",L"",ES_AUTOHSCROLL);make(GpuPowerApply,L"BUTTON",L"");make(GpuPowerDefault,L"BUTTON",L"");
  label(GpuFanLabel,L"");make(GpuFanMode,L"COMBOBOX",L"",CBS_DROPDOWNLIST);make(GpuFanValue,L"EDIT",L"60",ES_NUMBER);make(GpuFanApply,L"BUTTON",L"");label(GpuHint,L"");
  for(int id:{GpuPowerValue,GpuFanValue})SendMessageW(controls[id],EM_SETLIMITTEXT,12,0);
  label(30,L"Estado atual");label(31,L"Nova execução");label(32,L"Proteção térmica");label(33,L"Ferramentas");label(34,L"Detalhes");
  label(3,L"Motor / GPU — perfil medido");make(Profile,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL);make(OpenProfile,L"BUTTON",L"Abrir perfil…");label(Identity,L"Seleciona um perfil medido.");
  label(5,L"Slot");make(Slot,L"EDIT",L"primary",ES_AUTOHSCROLL);
  label(6,L"Puzzle");make(Puzzle,L"EDIT",L"71",ES_NUMBER);label(7,L"Duração (min)");make(Duration,L"EDIT",L"60",ES_NUMBER);make(Once,L"BUTTON",L"Uma atribuição",BS_AUTOCHECKBOX);
  label(BenchmarkSourceLabel,L"");make(BenchmarkSource,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL);
  label(BenchmarkDeviceLabel,L"");make(BenchmarkDevice,L"EDIT",L"0",ES_NUMBER);
  label(BenchmarkThreadsLabel,L"");make(BenchmarkThreads,L"EDIT",L"256",ES_NUMBER);
  for(int id:{BenchmarkDevice,BenchmarkThreads})SendMessageW(controls[id],EM_SETLIMITTEXT,4,0);
  make(Benchmark,L"BUTTON",L"Benchmark");make(Preflight,L"BUTTON",L"Verificar perfil");make(Start,L"BUTTON",L"Iniciar");make(Stop,L"BUTTON",L"Parar");make(SafeStop,L"BUTTON",L"Safe Stop");make(Recover,L"BUTTON",L"Recuperar");make(Refresh,L"BUTTON",L"Atualizar");make(Doctor,L"BUTTON",L"Diagnóstico offline");label(State,L"A verificar estado…");
  make(Enabled,L"BUTTON",L"Ativar proteção térmica",BS_AUTOCHECKBOX);make(CriticalStop,L"BUTTON",L"Parar no limite crítico",BS_AUTOCHECKBOX);label(8,L"Aviso °C");make(Warning,L"EDIT",L"",ES_AUTOHSCROLL);label(9,L"Crítico °C");make(Critical,L"EDIT",L"",ES_AUTOHSCROLL);make(SaveThermal,L"BUTTON",L"Guardar limites");
  label(16,L"");label(SpeedMetric,L"—");
  label(10,L"TEMPERATURA GPU");label(12,L"POTÊNCIA");label(13,L"LIMITE DE POTÊNCIA");label(TemperatureMetric,L"—");label(PowerMetric,L"—");label(LimitMetric,L"—");
  make(Details,L"COMBOBOX",L"",CBS_DROPDOWNLIST);SendMessageW(controls[Details],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Registo / mensagens"));SendMessageW(controls[Details],CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Telemetria NVIDIA"));SendMessageW(controls[Details],CB_SETCURSEL,0,0);
  make(Telemetry,L"EDIT",L"A ler sensores…",ES_MULTILINE|ES_READONLY|WS_VSCROLL|ES_AUTOVSCROLL);make(Log,L"EDIT",L"",ES_MULTILINE|ES_READONLY|WS_VSCROLL|ES_AUTOVSCROLL|WS_HSCROLL|ES_AUTOHSCROLL);ShowWindow(controls[Telemetry],SW_HIDE);
  for(int id:{1})SendMessageW(controls[id],WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
  for(int id:std::vector<int>{30,31,32,33,34,35,SpeedMetric,TemperatureMetric,PowerMetric,LimitMetric})SendMessageW(controls[id],WM_SETFONT,reinterpret_cast<WPARAM>(cardFont),TRUE);
  for(int id:std::vector<int>{2,3,5,6,7,8,9,10,12,13,14,15,16,Identity})SendMessageW(controls[id],WM_SETFONT,reinterpret_cast<WPARAM>(labelFont),TRUE);
  for(int id:{Log,Telemetry})SendMessageW(controls[id],WM_SETFONT,reinterpret_cast<WPARAM>(monoFont),TRUE);
  SendMessageW(controls[Log],EM_SETLIMITTEXT,400000,0);SendMessageW(controls[Telemetry],EM_SETLIMITTEXT,300000,0);
  for(int id:{Slot,Puzzle,Duration,Warning,Critical})SendMessageW(controls[id],EM_SETLIMITTEXT,id==Slot?32:8,0);
  if(!smoke||fixtureProfile.empty()){for(const auto backend:{"cpu","cuda","opencl"}){const auto path=profileRoot()/"profiles"/backend/"result.json";if(std::filesystem::exists(path)){try{addProfile(path);}catch(const std::exception& e){append(std::string("Perfil: ")+e.what()+"\n");}}}}
  if(!smoke||fixtureProfile.empty())for(const auto backend:{"cuda","opencl"})try{const auto path=loadWindowsUiAutomaticProfile(profileRoot(),backend);if(path)addProfile(*path);}catch(const std::exception& e){append(std::string("Perfil: ")+e.what()+"\n");}
  controlAction=UiAction::ThermalRead;try{control.start(cli,{"thermal"});}catch(const std::exception& e){append(e.what());}
  applyLanguage();applyTheme();SetTimer(window,1,150,nullptr);readGpu();update();
 }
 void layout(){
  RECT r{};GetClientRect(window,&r);const int width=r.right,height=r.bottom;contentWidth=(std::min)(1240,width-40);contentLeft=(width-contentWidth)/2;
  scroll=(std::clamp)(scroll,0,(std::max)(0,pageHeight-height));SCROLLINFO info{sizeof(SCROLLINFO),SIF_RANGE|SIF_PAGE|SIF_POS,0,pageHeight-1,static_cast<UINT>(height),scroll,0};SetScrollInfo(window,SB_VERT,&info,TRUE);
  const int left=contentLeft+18,full=contentWidth-36,right=left+full;
  HDWP positions=BeginDeferWindowPos(static_cast<int>(controls.size()));if(!positions)throw std::runtime_error("Window layout allocation failed");
  auto move=[&](int id,int x,int y,int w,int h=28){if(!positions)return;positions=DeferWindowPos(positions,controls.at(id),nullptr,x,y-scroll,(std::max)(20,w),h,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOREDRAW|SWP_NOCOPYBITS);};
  cards={{30,{contentLeft,126-scroll,contentLeft+contentWidth,288-scroll}},{31,{contentLeft,306-scroll,contentLeft+contentWidth,602-scroll}},{32,{contentLeft,620-scroll,contentLeft+contentWidth,776-scroll}},{33,{contentLeft,794-scroll,contentLeft+contentWidth,878-scroll}},{34,{contentLeft,896-scroll,contentLeft+contentWidth,1146-scroll}}};
  move(1,contentLeft+202,22,full-504,36);move(2,contentLeft+202,66,full-504,38);move(15,right-288,28,144);move(Language,right-288,52,144,250);move(14,right-128,28,128);move(Theme,right-128,52,128,250);move(Badge,right-270,142,136,32);
  move(30,left,144,full-420);move(Refresh,right-122,142,122,32);move(State,left,180,full,24);
  const int cell=(full-36)/4;for(const auto [label,value,index]:std::vector<std::tuple<int,int,int>>{{16,SpeedMetric,0},{10,TemperatureMetric,1},{12,PowerMetric,2},{13,LimitMetric,3}}){move(label,left+index*(cell+12)+14,220,cell-28,18);move(value,left+index*(cell+12)+14,242,cell-28,28);}
  move(31,left,324,full);move(3,left,360,full);move(Profile,left,382,full-146,320);move(OpenProfile,right-132,382,132,34);move(Identity,left,427,full,56);
  move(5,right-196,490,196);move(Slot,right-196,512,196,32);
  move(6,left,490,85,18);move(Puzzle,left,512,85,30);move(7,left+104,490,120,18);move(Duration,left+104,512,120,30);move(Once,left+246,512,205,30);
  move(Start,left,554,115,34);move(SafeStop,right-287,554,177,34);move(Stop,right-98,554,98,34);
  move(32,left,638,full);move(Enabled,left,676,225);move(CriticalStop,left,722,240);move(8,left+268,674,82,18);move(Warning,left+268,696,85,32);move(9,left+370,674,85,18);move(Critical,left+370,696,85,32);move(SaveThermal,right-150,720,150,34);
  cards[35]={contentLeft,794-scroll,contentLeft+contentWidth,1138-scroll};cards[33]={contentLeft,1154-scroll,contentLeft+contentWidth,1310-scroll};cards[34]={contentLeft,1328-scroll,contentLeft+contentWidth,1578-scroll};
  move(35,left,810,full);move(GpuPick,left,844,full-146,300);move(GpuRefresh,right-132,844,132,32);move(GpuInfo,left,888,full,66);
  move(GpuPowerLabel,left,964,180,22);move(GpuPowerValue,left,990,120,32);move(GpuPowerApply,left+132,990,100,32);move(GpuPowerDefault,left+244,990,180,32);
  move(GpuFanLabel,left,1034,100,24);move(GpuFanMode,left+108,1030,165,250);move(GpuFanValue,left+285,1030,70,32);move(GpuFanApply,left+367,1030,100,32);move(GpuHint,left,1074,full,52);
  move(33,left,1172,full);move(BenchmarkSourceLabel,left,1196,250,18);move(BenchmarkSource,left,1218,260,250);
  move(BenchmarkDeviceLabel,left+278,1196,100,18);move(BenchmarkDevice,left+278,1218,100,32);
  move(BenchmarkThreadsLabel,left+396,1196,110,18);move(BenchmarkThreads,left+396,1218,110,32);
  move(Benchmark,left,1262,125,32);move(Preflight,left+137,1262,145,32);move(Doctor,left+294,1262,174,32);move(Recover,left+480,1262,126,32);
  move(34,left,1346,full-270);move(Details,right-250,1342,250,300);move(Log,left,1386,full,172);move(Telemetry,left,1386,full,172);
  if(!positions||!EndDeferWindowPos(positions))throw std::runtime_error("Window layout update failed");
  RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
 }
 void command(int id,int notification){
  if(notification==EN_SETFOCUS||notification==BN_SETFOCUS||notification==CBN_SETFOCUS){if(controls.count(id))ensureVisible(controls[id]);}
  if(id==Language&&notification==CBN_SELCHANGE){const auto index=SendMessageW(controls[Language],CB_GETCURSEL,0,0);if(index<0||index>=4)return;const auto next=static_cast<UiLanguage>(index);
   if(!languageStore.save(next)){SendMessageW(controls[Language],CB_SETCURSEL,static_cast<unsigned>(language),0);throw std::runtime_error(t("language_save_failed"));}
   language=next;applyLanguage();return;
  }
  if(id==Theme&&notification==CBN_SELCHANGE){dark=SendMessageW(controls[Theme],CB_GETCURSEL,0,0)==1;applyTheme();return;}
  if(id==Details&&notification==CBN_SELCHANGE){const bool telemetry=SendMessageW(controls[Details],CB_GETCURSEL,0,0)==1;ShowWindow(controls[Telemetry],telemetry?SW_SHOW:SW_HIDE);ShowWindow(controls[Log],telemetry?SW_HIDE:SW_SHOW);return;}
  if(id==Puzzle&&notification==EN_CHANGE){if(controls.count(Badge)&&controls.count(Log))update();return;}
  if(id==BenchmarkSource&&notification==CBN_SELCHANGE){benchmarkInputs();return;}
  if(id==GpuPick&&notification==CBN_SELCHANGE){renderGpu(true);return;}
  if(id==GpuFanMode&&notification==CBN_SELCHANGE){updateGpu();return;}
  if(id==Profile&&notification==CBN_SELCHANGE){chooseProfile();return;}
  if(id==Slot&&notification==EN_CHANGE){if(!runtime.busy()){status={};nextRefresh=0;}return;}
  if(notification!=BN_CLICKED)return;
  if(checkboxIds.count(id)){checkmark(id,!checked(id));return;}
  if(id==OpenProfile){wchar_t name[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;std::wstring filter=wide(t("profile_filter"));filter.push_back(L'\0');filter+=L"*.json";filter.push_back(L'\0');filter.push_back(L'\0');dialog.lpstrFilter=filter.c_str();dialog.lpstrFile=name;dialog.nMaxFile=32768;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;if(GetOpenFileNameW(&dialog))addProfile(name);}
  if(id==GpuRefresh){readGpu();return;}
  if(id==GpuPowerApply||id==GpuPowerDefault||id==GpuFanApply){applyGpu(id);return;}
  if(id==Benchmark){benchmark();return;}
  if(id==Stop&&control.busy()&&controlAction==UiAction::Benchmark){if(!control.cancelOwnedCommand())throw std::runtime_error(t("benchmark_cancel_failed"));append("\n"+t("benchmark_cancelled")+"\n");return;}
  if(id==Preflight)action(UiAction::Preflight);if(id==Start)action(UiAction::Start);if(id==Stop)action(UiAction::Stop);if(id==SafeStop)action(UiAction::SafeStop);if(id==Recover)action(UiAction::Recover);if(id==Refresh){metadataLoaded=false;nextRefresh=0;}if(id==Doctor&&!control.busy()){controlOutput.clear();controlAction=UiAction::Status;control.start(cli,{"doctor","--offline"});}if(id==SaveThermal&&!control.busy()&&!runtime.busy())saveThermal();update();
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
  if(message==WM_PAINT){PAINTSTRUCT paint{};auto dc=BeginPaint(hwnd,&paint);RECT rect{};GetClientRect(hwnd,&rect);
   auto memory=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,(std::max)(1L,rect.right),(std::max)(1L,rect.bottom));
   if(memory&&bitmap){auto previous=SelectObject(memory,bitmap);p->paint(memory);BitBlt(dc,0,0,rect.right,rect.bottom,memory,0,0,SRCCOPY);SelectObject(memory,previous);}else p->paint(dc);
   if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);EndPaint(hwnd,&paint);return 0;}
  if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLOREDIT||message==WM_CTLCOLORLISTBOX){return p->color(reinterpret_cast<HDC>(w),reinterpret_cast<HWND>(l));}
  if(message==WM_MEASUREITEM){reinterpret_cast<MEASUREITEMSTRUCT*>(l)->itemHeight=30;return TRUE;}
  if(message==WM_DRAWITEM){p->draw(reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;}
  if(message==WM_VSCROLL){int value=p->scroll;const auto code=LOWORD(w);if(code==SB_LINEUP)value-=30;if(code==SB_LINEDOWN)value+=30;if(code==SB_PAGEUP)value-=300;if(code==SB_PAGEDOWN)value+=300;if(code==SB_THUMBTRACK||code==SB_THUMBPOSITION){SCROLLINFO info{sizeof(SCROLLINFO),SIF_TRACKPOS};GetScrollInfo(hwnd,SB_VERT,&info);value=info.nTrackPos;}p->scrollTo(value);return 0;}
  if(message==WM_MOUSEWHEEL){p->wheelRemainder+=GET_WHEEL_DELTA_WPARAM(w)*90;const auto pixels=p->wheelRemainder/WHEEL_DELTA;p->wheelRemainder%=WHEEL_DELTA;p->scrollTo(p->scroll-pixels);return 0;}
  if(message==WM_GETMINMAXINFO){auto* info=reinterpret_cast<MINMAXINFO*>(l);info->ptMinTrackSize={820,620};return 0;}
  if(message==WM_CLOSE){if(p->runtime.busy()||p->control.busy()||p->gpuWriter.busy()){
   if(!p->smoke)MessageBoxW(hwnd,wide(p->t(p->gpuWriter.busy()?"gpu_write_busy":"close_busy")).c_str(),L"OpenPuzzle",MB_OK|MB_ICONINFORMATION);return 0;}
   DestroyWindow(hwnd);return 0;}
  if(message==WM_DESTROY){KillTimer(hwnd,1);PostQuitMessage(p->result);return 0;}
 }}catch(const std::exception& e){if(p){if(p->smoke){std::cerr<<"Window fixture: "<<e.what()<<'\n';p->result=1;DestroyWindow(hwnd);}else p->error(e);}return message==WM_CREATE?-1:0;}
 return DefWindowProcW(hwnd,message,w,l);
}
}
int runWindowsUi(const std::filesystem::path& cli,bool smokeTest,const std::wstring& languageKey,const std::filesystem::path& fixtureProfile,const std::string& firstBackend){
 Panel panel(cli,smokeTest,languageKey,fixtureProfile,firstBackend);WNDCLASSW type{};type.lpfnWndProc=procedure;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"OpenPuzzleWindowsPanel";type.hCursor=LoadCursorW(nullptr,IDC_ARROW);type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
 if(!RegisterClassW(&type)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 1;
 const auto window=CreateWindowExW(0,type.lpszClassName,L"OpenPuzzle",WS_OVERLAPPEDWINDOW|WS_VSCROLL|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1120,860,nullptr,nullptr,type.hInstance,&panel);
 if(!window)return 1;ShowWindow(window,smokeTest?SW_HIDE:SW_SHOW);UpdateWindow(window);
 MSG message{};int status;while((status=GetMessageW(&message,nullptr,0,0))>0){if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
 return status<0?1:static_cast<int>(message.wParam);
}
}
