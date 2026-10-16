#include "openpuzzle/ui/WindowsUiProcess.hpp"
#include "openpuzzle/core/WindowsCommandLine.hpp"
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/hardware/GpuControl.hpp"
#include <shellapi.h>
#include <array>
#include <algorithm>
#include <stdexcept>
namespace openpuzzle::ui {
namespace {
struct Handle {HANDLE value=nullptr;~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}HANDLE take(){auto v=value;value=nullptr;return v;}};
struct Attributes {
 std::vector<unsigned char> data;LPPROC_THREAD_ATTRIBUTE_LIST list=nullptr;
 Attributes(HANDLE* handles,SIZE_T bytes){
  SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);if(!size)throw std::runtime_error("Lista de handles indisponível");
  data.resize(size);auto p=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(data.data());
  if(!InitializeProcThreadAttributeList(p,1,0,&size))throw std::runtime_error("Lista de handles inválida");list=p;
  if(!UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,bytes,nullptr,nullptr)){DeleteProcThreadAttributeList(list);list=nullptr;throw std::runtime_error("Herança de handles recusada");}
 }
 ~Attributes(){if(list)DeleteProcThreadAttributeList(list);}
};
}
void WindowsUiProcess::release(){if(pipe_)CloseHandle(pipe_);if(process_)CloseHandle(process_);pipe_=process_=nullptr;}
WindowsUiProcess::~WindowsUiProcess(){if(busy()&&!runtime_)TerminateProcess(process_,125);release();}
bool WindowsUiProcess::cancelOwnedCommand(){return busy()&&!runtime_&&TerminateProcess(process_,1223)!=0;}
void WindowsUiProcess::start(const std::filesystem::path& cli,const std::vector<std::string>& args,bool runtime,DWORD timeoutMs){
 if(busy()||!cli.is_absolute()||(!runtime&&!timeoutMs))throw std::invalid_argument("Processo ocupado ou caminho inválido");
 auto command=pathFromUtf8(WindowsCommandLine::build(pathUtf8(cli),args)).wstring();
 if(command.size()>=32767)throw std::invalid_argument("Comando demasiado longo");
 SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};Handle read,write,input;
 if(!CreatePipe(&read.value,&write.value,&security,0)||!SetHandleInformation(read.value,HANDLE_FLAG_INHERIT,0))throw std::runtime_error("Pipe de saída indisponível");
 input.value=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(input.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Entrada de processo indisponível");
 HANDLE inherited[]{write.value,input.value};Attributes attributes(inherited,sizeof(inherited));
 STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
 startup.StartupInfo.hStdInput=input.value;startup.StartupInfo.hStdOutput=write.value;startup.StartupInfo.hStdError=write.value;startup.lpAttributeList=attributes.list;
 PROCESS_INFORMATION info{};
 const auto directory=cli.parent_path().wstring();
 if(!CreateProcessW(cli.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,
  nullptr,directory.c_str(),&startup.StartupInfo,&info))throw std::runtime_error("Não foi possível iniciar a CLI: "+std::to_string(GetLastError()));
 CloseHandle(info.hThread);process_=info.hProcess;pipe_=read.take();runtime_=runtime;
 deadline_=runtime?0:GetTickCount64()+timeoutMs;
}
void WindowsUiProcess::startGpuControlElevated(HWND owner,const std::filesystem::path& helper,const std::vector<std::string>& args){
 if(busy()||!helper.is_absolute()||helper.filename()!=L"openpuzzle-gpu-control.exe"||args.empty()||args.front()!="apply")throw std::invalid_argument("Invalid GPU helper request");
 // Parse the same whitelist before asking UAC; this callback performs no write.
 std::ostringstream ignored;gpuControlCommand(args,ignored,[]{return std::vector<GpuControlDevice>{};},[](const auto&){});
 std::wstring parameters;for(const auto& arg:args){if(!parameters.empty())parameters+=L' ';parameters+=pathFromUtf8(WindowsCommandLine::quote(arg)).wstring();}
 SHELLEXECUTEINFOW info{};info.cbSize=sizeof info;info.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;info.hwnd=owner;info.lpVerb=L"runas";info.lpFile=helper.c_str();info.lpParameters=parameters.c_str();info.nShow=SW_HIDE;
 if(!ShellExecuteExW(&info)||!info.hProcess)throw std::runtime_error("GPU administrator request failed or was cancelled: "+std::to_string(GetLastError()));
 process_=info.hProcess;pipe_=nullptr;runtime_=true;deadline_=0;
}
UiProcessUpdate WindowsUiProcess::poll(){
 UiProcessUpdate result;if(!busy())return result;
 std::array<char,4096> buffer{};DWORD available=0;
 while(pipe_&&result.output.size()<65536&&PeekNamedPipe(pipe_,nullptr,0,nullptr,&available,nullptr)&&available){
  DWORD count=0;const auto wanted=(std::min)(available,static_cast<DWORD>(buffer.size()));
  if(!ReadFile(pipe_,buffer.data(),wanted,&count,nullptr)||!count)break;result.output.append(buffer.data(),count);
 }
 const auto wait=WaitForSingleObject(process_,0);
 if(wait==WAIT_OBJECT_0){
  // Drain a large final output across subsequent ticks, preserving the tail.
  if(pipe_&&PeekNamedPipe(pipe_,nullptr,0,nullptr,&available,nullptr)&&available)return result;
  DWORD code=1;GetExitCodeProcess(process_,&code);result.exitCode=static_cast<int>(code);result.finished=true;release();
 }else if(!runtime_&&GetTickCount64()>=deadline_){
  TerminateProcess(process_,124);result.timedOut=true;result.finished=true;result.exitCode=124;release();
 }else if(wait==WAIT_FAILED){result.finished=true;result.exitCode=125;release();}
 return result;
}
}
