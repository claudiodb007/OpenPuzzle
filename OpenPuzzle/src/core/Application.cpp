#include "openpuzzle/core/Application.hpp"
#include "openpuzzle/allocator/RangeAllocator.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/tools/ToolManager.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <regex>
#include <sys/wait.h>
#ifndef WEXITSTATUS
#define WEXITSTATUS(x) (x)
#endif
namespace fs=std::filesystem; namespace openpuzzle {
static std::string st(RangeStatus s){ switch(s){case RangeStatus::Reserved:return"RESERVED";case RangeStatus::Running:return"RUNNING";case RangeStatus::Completed:return"COMPLETED";case RangeStatus::Failed:return"FAILED";case RangeStatus::Cancelled:return"CANCELLED";case RangeStatus::External:return"EXTERNAL";} return"UNKNOWN";}
std::string Application::dbPath() const{ const char* h=getenv("HOME"); fs::path p=h?fs::path(h):fs::current_path(); p/=".local/share/OpenPuzzle/openpuzzle.db"; fs::create_directories(p.parent_path()); return p.string(); }
bool Application::ensureDb(Database& db){ return db.open(dbPath()) && db.createSchema(); }
bool Application::hasArg(const std::vector<std::string>& a,const std::string& n){ for(auto&s:a) if(s==n) return true; return false; }
std::string Application::getArg(const std::vector<std::string>& a,const std::string& n,const std::string& d){ for(size_t i=0;i+1<a.size();++i) if(a[i]==n) return a[i+1]; return d; }
int Application::getIntArg(const std::vector<std::string>& a,const std::string& n,int d){ auto s=getArg(a,n,""); return s.empty()?d:std::stoi(s); }
int Application::run(int argc,char** argv){ std::vector<std::string> args; for(int i=1;i<argc;++i) args.emplace_back(argv[i]); if(args.empty()){ std::cout<<"OpenPuzzle 0.11.1-dev\n"; return 0;} auto cmd=args[0]; std::vector<std::string> r(args.begin()+1,args.end()); try{ if(cmd=="init")return cmdInit(); if(cmd=="import-puzzle-json")return cmdImportPuzzleJson(r); if(cmd=="list-puzzles")return cmdListPuzzles(); if(cmd=="create-job")return cmdCreateJob(r); if(cmd=="list-ranges")return cmdListRanges(r); if(cmd=="complete-job")return cmdCompleteJob(r); if(cmd=="stats")return cmdStats(r); if(cmd=="configure-tool")return cmdConfigureTool(r); if(cmd=="tools")return cmdTools(); if(cmd=="gpu-list")return cmdGpuList(); if(cmd=="gpu-select")return cmdGpuSelect(r); if(cmd=="bitcrack-command")return cmdBitcrackCommand(r); if(cmd=="start-job")return cmdStartJob(r); if(cmd=="dashboard")return cmdDashboard(r); if(cmd=="audit")return cmdAudit(r);}catch(const std::exception&e){ std::cerr<<"Error: "<<e.what()<<"\n"; return 1;} std::cerr<<"Unknown command\n"; return 1; }
int Application::cmdInit(){ Database db; if(!ensureDb(db)) return 1; std::cout<<"Database initialized: "<<dbPath()<<"\n"; return 0; }
static std::string readFile(const std::string& f){ for(auto p:{fs::path(f),fs::current_path()/f,fs::current_path().parent_path()/f,fs::current_path().parent_path()/"resources/puzzles"/f}){ std::ifstream in(p); if(in){ std::stringstream b; b<<in.rdbuf(); return b.str(); }} throw std::runtime_error("Could not open puzzle JSON: "+f); }
static std::string js(const std::string&t,const std::string&k,const std::string&d=""){ auto p=t.find("\""+k+"\""); if(p==std::string::npos)return d; auto c=t.find(':',p); auto f=t.find('"',c); auto s=t.find('"',f+1); if(f==std::string::npos||s==std::string::npos)return d; return t.substr(f+1,s-f-1);}
static int ji(const std::string&t,const std::string&k){ auto p=t.find("\""+k+"\""); auto c=t.find(':',p); return std::stoi(t.substr(c+1));}
static double jd(const std::string&t,const std::string&k){ auto p=t.find("\""+k+"\""); auto c=t.find(':',p); return std::stod(t.substr(c+1));}
int Application::cmdImportPuzzleJson(const std::vector<std::string>& a){ auto file=getArg(a,"--file"); auto t=readFile(file); PuzzleRecord p; p.number=ji(t,"number"); p.name=js(t,"name"); p.address=js(t,"address"); p.reward=jd(t,"reward"); p.sharing=js(t,"sharing","private"); auto ks=js(t,"keyspace"); auto pos=ks.find(':'); p.rangeStart=ks.substr(0,pos); p.rangeEnd=ks.substr(pos+1); Database db; if(!ensureDb(db)) return 1; db.upsertPuzzle(p); std::cout<<"Imported puzzle "<<p.number<<"\n"; return 0; }
int Application::cmdListPuzzles(){ Database db; if(!ensureDb(db))return 1; for(auto&p:db.listPuzzles()) std::cout<<"#"<<p.number<<" "<<p.name<<" "<<p.rangeStart<<":"<<p.rangeEnd<<"\n"; return 0; }
int Application::cmdCreateJob(const std::vector<std::string>& a){ int n=getIntArg(a,"--puzzle",71), bits=getIntArg(a,"--block-bits",40); Database db; if(!ensureDb(db))return 1; auto p=db.getPuzzleByNumber(n); if(!p) throw std::runtime_error("Puzzle not found"); RangeAllocator al(db); auto range=al.allocateNext(*p,bits); if(!range) throw std::runtime_error("No range available"); JobRecord j; j.puzzleId=p->id; j.rangeId=range->id; j.id=db.insertJob(j); std::cout<<"Reserved Job ID...... "<<j.id<<"\nReserved Range ID.... "<<range->id<<"\nKeyspace............. "<<range->startKey<<":"<<range->endKey<<"\n"; return 0; }
int Application::cmdListRanges(const std::vector<std::string>& a){ int n=getIntArg(a,"--puzzle",71); Database db; if(!ensureDb(db))return 1; auto p=db.getPuzzleByNumber(n); if(!p) throw std::runtime_error("Puzzle not found"); for(auto&r:db.listRanges(p->id)) std::cout<<"#"<<r.id<<" "<<r.startKey<<":"<<r.endKey<<" "<<st(r.status)<<" block_bits="<<r.blockBits<<"\n"; return 0; }
int Application::cmdCompleteJob(const std::vector<std::string>& a){ int id=getIntArg(a,"--job",0); Database db; if(!ensureDb(db))return 1; auto j=db.getJob(id); if(!j) throw std::runtime_error("Job not found"); db.updateJobState(id,JobState::Completed); db.updateRangeStatus(j->rangeId,RangeStatus::Completed); std::cout<<"Job completed......... "<<id<<"\nRange completed....... "<<j->rangeId<<"\n"; return 0; }
int Application::cmdStats(const std::vector<std::string>& a){ int n=getIntArg(a,"--puzzle",71); Database db; if(!ensureDb(db))return 1; auto p=db.getPuzzleByNumber(n); if(!p) throw std::runtime_error("Puzzle not found"); std::cout<<"Puzzle............... "<<p->name<<"\nRanges RESERVED...... "<<db.countRangesByStatus(p->id,RangeStatus::Reserved)<<"\nRanges RUNNING....... "<<db.countRangesByStatus(p->id,RangeStatus::Running)<<"\nRanges COMPLETED..... "<<db.countRangesByStatus(p->id,RangeStatus::Completed)<<"\nJobs RESERVED........ "<<db.countJobsByState(p->id,JobState::Reserved)<<"\nJobs RUNNING......... "<<db.countJobsByState(p->id,JobState::Running)<<"\nJobs COMPLETED....... "<<db.countJobsByState(p->id,JobState::Completed)<<"\n"; return 0; }
int Application::cmdConfigureTool(const std::vector<std::string>& a){ auto p=getArg(a,"--bitcrack"); if(p.empty()) throw std::runtime_error("--bitcrack required"); ToolManager::configureBitCrack(p); std::cout<<"Configured BitCrack... "<<p<<"\n"; return 0; }
int Application::cmdTools(){ auto p=ToolManager::bitcrackPath(); std::cout<<"BitCrack............. "<<(p?*p:"(not configured)")<<"\n"; return 0; }
int Application::cmdGpuList(){ for(auto&g:GpuManager::listGpus()) std::cout<<"GPU "<<g.device<<": "<<g.name<<" | "<<g.memory<<" | "<<g.uuid<<"\n"; return 0; }
int Application::cmdGpuSelect(const std::vector<std::string>& a){ int d=getIntArg(a,"--device",0); GpuManager::selectGpu(d); std::cout<<"Selected GPU device... "<<d<<"\n"; return 0; }
static std::string wsFor(int job){ const char* h=getenv("HOME"); fs::path p=h?fs::path(h):fs::current_path(); std::ostringstream s; s<<std::setw(6)<<std::setfill('0')<<job; p/=".local/share/OpenPuzzle/jobs"; p/=s.str(); fs::create_directories(p); return p.string(); }
static std::string bcCmd(const std::string& bc,const PuzzleRecord&p,const RangeRecord&r,int dev,int b,int t,int pt,const std::string& out){ std::ostringstream c; c<<bc<<" "<<p.address<<" --keyspace "<<r.startKey<<":"<<r.endKey<<" --out "<<out<<" -d "<<dev<<" -b "<<b<<" -t "<<t<<" -p "<<pt; return c.str(); }
int Application::cmdBitcrackCommand(const std::vector<std::string>& a){ int n=getIntArg(a,"--puzzle",71), jid=getIntArg(a,"--job",0), b=getIntArg(a,"--blocks",256), t=getIntArg(a,"--threads",256), pt=getIntArg(a,"--points",1024); Database db; if(!ensureDb(db))return 1; auto p=db.getPuzzleByNumber(n); auto j=db.getJob(jid); if(!p||!j) throw std::runtime_error("Puzzle/job not found"); auto r=db.getRange(j->rangeId); auto bc=ToolManager::bitcrackPath(); if(!r||!bc) throw std::runtime_error("Range/BitCrack not found"); auto out=(fs::path(wsFor(jid))/"found.txt").string(); std::cout<<bcCmd(*bc,*p,*r,GpuManager::selectedGpu(),b,t,pt,out)<<"\n"; return 0; }
int Application::cmdStartJob(const std::vector<std::string>& a){ int n=getIntArg(a,"--puzzle",71), jid=getIntArg(a,"--job",0), b=getIntArg(a,"--blocks",256), t=getIntArg(a,"--threads",256), pt=getIntArg(a,"--points",1024); bool dry=hasArg(a,"--dry-run"); Database db; if(!ensureDb(db))return 1; auto p=db.getPuzzleByNumber(n); auto j=db.getJob(jid); if(!p||!j) throw std::runtime_error("Puzzle/job not found"); auto r=db.getRange(j->rangeId); auto bc=ToolManager::bitcrackPath(); if(!r||!bc) throw std::runtime_error("Range/BitCrack not found"); auto ws=wsFor(jid); auto out=(fs::path(ws)/"found.txt").string(); auto log=(fs::path(ws)/"bitcrack.log").string(); auto cmd=bcCmd(*bc,*p,*r,GpuManager::selectedGpu(),b,t,pt,out)+" 2>&1 | tee -a "+log; int ex=db.insertExecution(jid,ws,cmd,dry?"dry-run":"running"); std::cout<<"Workspace............ "<<ws<<"\nExecution ID......... "<<ex<<"\nCommand.............. "<<cmd<<"\n"; if(dry){ std::cout<<"Dry run only.\n"; return 0;} db.updateJobState(jid,JobState::Running); db.updateRangeStatus(r->id,RangeStatus::Running); int rc=std::system(cmd.c_str()); int code=WEXITSTATUS(rc); db.finishExecution(ex,code==0?"finished":"failed",code); if(code==0){ db.updateJobState(jid,JobState::Completed); db.updateRangeStatus(r->id,RangeStatus::Completed);} else { db.updateJobState(jid,JobState::Failed); db.updateRangeStatus(r->id,RangeStatus::Failed);} return code; }


int Application::cmdDashboard(const std::vector<std::string>& args) {
    int number = getIntArg(args, "--puzzle", 71);

    Database db;
    if (!ensureDb(db)) {
        std::cerr << "Database error\n";
        return 1;
    }

    auto puzzle = db.getPuzzleByNumber(number);
    if (!puzzle) {
        std::cerr << "Puzzle not found\n";
        return 1;
    }

    std::cout << "======================================\n";
    std::cout << "        OpenPuzzle Dashboard\n";
    std::cout << "======================================\n\n";
    std::cout << "Puzzle............... " << puzzle->name << "\n";
    std::cout << "Address.............. " << puzzle->address << "\n";
    std::cout << "Keyspace............. " << puzzle->rangeStart << ":" << puzzle->rangeEnd << "\n\n";

    std::cout << "Ranges RESERVED...... " << db.countRangesByStatus(puzzle->id, RangeStatus::Reserved) << "\n";
    std::cout << "Ranges RUNNING....... " << db.countRangesByStatus(puzzle->id, RangeStatus::Running) << "\n";
    std::cout << "Ranges COMPLETED..... " << db.countRangesByStatus(puzzle->id, RangeStatus::Completed) << "\n";
    std::cout << "Jobs RESERVED........ " << db.countJobsByState(puzzle->id, JobState::Reserved) << "\n";
    std::cout << "Jobs RUNNING......... " << db.countJobsByState(puzzle->id, JobState::Running) << "\n";
    std::cout << "Jobs COMPLETED....... " << db.countJobsByState(puzzle->id, JobState::Completed) << "\n\n";

    std::cout << "Recent ranges:\n";
    auto ranges = db.listRanges(puzzle->id);
    int shown = 0;
    for (const auto& range : ranges) {
        std::cout << "  #" << range.id << " " << range.startKey << ":" << range.endKey
                  << " " << st(range.status)
                  << " block_bits=" << range.blockBits << "\n";
        if (++shown >= 10) {
            break;
        }
    }

    auto bitcrack = ToolManager::bitcrackPath();
    std::cout << "\nBitCrack............. " << (bitcrack ? *bitcrack : "(not configured)") << "\n";
    std::cout << "Selected GPU......... " << GpuManager::selectedGpu() << "\n";

    return 0;
}

int Application::cmdAudit(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "Audit log is stored in SQLite table: audit_log\n";
    std::cout << "Detailed audit listing will be implemented in the next build.\n";
    return 0;
}

}
