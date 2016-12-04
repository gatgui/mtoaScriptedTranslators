#ifndef __plugin_h__
#define __plugin_h__

#include <string>
#include <map>
#include <set>
#include "common.h"
#include "extension/Extension.h"

struct CScriptedTranslator
{
   MString exportCmd;
   MString cleanupCmd;
   MString setupAECmd;
   MString setupAttrsCmd;
   MString requiredPlugin;
   bool isShape;
   bool supportInstances;
   bool supportVolumes;
   bool attrsAdded;
   bool deferred;
};


extern std::map<std::string, CScriptedTranslator> gTranslators;
extern MCallbackId gPluginLoadedCallbackId;


void NodeInitializer(CAbTranslator context);

void RegisterTranslators(CExtension& plugin);
bool RegisterTranslator(CExtension& plugin, std::string &nodeType, const std::string &providedByPlugin="");

void MayaPluginLoadedCallback(const MStringArray &strs, void *clientData);
MCallbackId AddPluginLoadedCallback();
MStatus RemovePluginLoadedCallback();

float GetSampleFrame(CArnoldSession *session, unsigned int step);

bool StringToValue(const std::string &sval, CAttrData &data, AtParamValue *val);
void DestroyValue(CAttrData &data, AtParamValue *val);

bool HasParameter(const AtNodeEntry *anodeEntry, const char *param, AtNode *anode=NULL, const char *decl=NULL);

#endif
