#include "openpuzzle/ui/UiTranslations.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace openpuzzle::ui;
void require(bool value,const char* detail){if(!value)throw std::runtime_error(detail);}
std::set<char> placeholders(std::string_view value){std::set<char> result;for(std::size_t i=1;i<value.size();++i)if(value[i-1]=='%'&&value[i]>='1'&&value[i]<='9')result.insert(value[i]);return result;}
int main(){try{
 std::set<std::string_view> keys;
 for(const auto& row:uiTranslations){
  require(keys.insert(row.key).second,"Duplicate translation key");
  for(const auto value:row.text){require(!value.empty(),"Empty translation");require(placeholders(value)==placeholders(row.text[0]),"Translation lost a placeholder");}
 }
 for(unsigned i=0;i<4;++i){const auto language=static_cast<UiLanguage>(i);require(uiLanguageFromCode(uiLanguageCode(language))==language,"Language code did not round-trip");
  for(const auto key:{"start","language","theme","thermal_safety","recover","close_busy","private_key_action","key_not_displayed"})require(uiText(language,key)!=key,"Missing user-facing translation");
 }
 require(uiText(UiLanguage::Portuguese,"start")=="Iniciar"&&uiText(UiLanguage::French,"start")=="Démarrer"&&uiText(UiLanguage::Spanish,"start")=="Iniciar","Linux translations changed");
 require(uiLanguageFromCode("invalid")==UiLanguage::English&&uiText(static_cast<UiLanguage>(42),"start")=="Start","Invalid language fallback");
 require(uiFormat(UiLanguage::French,"start_question","71","cuda-0")=="Démarrer le Puzzle 71 dans cuda-0 ?","Confirmation placeholders lost");
 require(uiFormat(UiLanguage::English,"start_question","%2","cuda-0")=="Start Puzzle %2 in cuda-0?","Substitution interpreted an argument");
 const std::string raw="Thermal mode: disabled\r\nWarning C: 75\r\nSlot: cuda-0\r\nRuntime: idle\r\nWallet file........ C:\\Users\\Claudio\\Power W: 13.txt\r\n";
 const auto localized=uiDisplayText(UiLanguage::Portuguese,raw);
 require(localized.find("Modo térmico: desativado")!=std::string::npos&&localized.find("Aviso °C: 75")!=std::string::npos,"Thermal output not translated");
 require(localized.find("Slot: cuda-0")!=std::string::npos&&localized.find("C:\\Users\\Claudio\\Power W: 13.txt")!=std::string::npos,"Identifier/path was rewritten");
 require(uiDisplayText(UiLanguage::French,"not displayed or uploaded")=="ni affichée ni envoyée","Private-key notice not translated");
 require(uiDisplayText(UiLanguage::English,"custom engine payload %PATH% αβ 123")=="custom engine payload %PATH% αβ 123","Unknown engine payload changed");
 require(raw.find("Thermal mode: disabled")!=std::string::npos,"Raw protocol mutated");
 std::cout<<"WindowsUiLanguageTests passed: four shared languages, placeholders, thermal display and preserved protocol/path data\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
