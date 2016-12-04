#include "shapetranslator.h"
#include "nodetranslator.h"
#include "plugin.h"

#define MNoVersionString
#define MNoPluginEntry
#include <maya/MFnPlugin.h>
#include <maya/MSceneMessage.h>

std::map<std::string, CScriptedTranslator> gTranslators;
MCallbackId gPluginLoadedCallbackId = 0;


static const char* gModuleSetup =
"import os, sys, glob\n\
for d in os.environ.get(\"MTOA_EXTENSIONS_PATH\", \"\").split(os.pathsep):\n\
   path = os.path.join(d, \"scriptedTranslatorUtils.py\")\n\
   if os.path.exists(path):\n\
      if not d in sys.path:\n\
         sys.path.append(d)\n\
      break\n";


void MayaPluginLoadedCallback(const MStringArray &strs, void *)
{
   // 0 = pluginPath, 1 = pluginName
   MString pluginName = strs[1];
   
   // Start the arnold universe so that attribute helpers can query arnold nodes
   bool AiUniverseCreated = false;
   if (!AiUniverseIsActive())
   {
      AiMsgSetConsoleFlags(AI_LOG_NONE);
      AiMsgSetLogFileFlags(AI_LOG_NONE);
      
      AiBegin();
      
      MString pluginPath = MString("$ARNOLD_PLUGIN_PATH").expandEnvironmentVariablesAndTilde();
      if (pluginPath.length() > 0)
      {
         AiLoadPlugins(pluginPath.asChar());
      }
      
      AiUniverseCreated = true;
   }
   
   std::map<std::string, CScriptedTranslator>::iterator it = gTranslators.begin();
   while (it != gTranslators.end())
   {
      if (!it->second.attrsAdded)
      {
         if (it->second.requiredPlugin == pluginName)
         {
            // Provider name ?
            CAbTranslator context("scriptedTranslators", "", MString(it->first.c_str()));
            NodeInitializer(context);
         }
      }
      ++it;
   }
   
   if (AiUniverseCreated)
   {
      AiEnd();
   }
}

MCallbackId AddPluginLoadedCallback()
{
   if (gPluginLoadedCallbackId != 0)
   {
      return gPluginLoadedCallbackId;
   }
   
   MStatus status;
   
   // Create plugin loaded callback
   gPluginLoadedCallbackId = MSceneMessage::addStringArrayCallback(MSceneMessage::kAfterPluginLoad, MayaPluginLoadedCallback, NULL, &status);
   CHECK_MSTATUS(status);
   
   return gPluginLoadedCallbackId;
}

MStatus RemovePluginLoadedCallback()
{
   MStatus status;
   
   // Delete plugin loaded callback
   if (gPluginLoadedCallbackId != 0)
   {
      status = MMessage::removeCallback(gPluginLoadedCallbackId);
      if (MStatus::kSuccess == status)
      {
         gPluginLoadedCallbackId = 0;
      }
   }
   return status;
}

#ifdef OLD_API
float GetSampleFrame(CArnoldSession *session, unsigned int step)
{
   MFnDependencyNode opts(session->GetArnoldRenderOptions());
   
   int steps = opts.findPlug("motion_steps").asInt();
   
   if (steps <= 1)
   {
      return session->GetExportFrame();
   }
   else
   {
      int mbtype = opts.findPlug("range_type").asInt();
      float start = session->GetExportFrame();
      float end = start;
      
      if (mbtype == MTOA_MBLUR_TYPE_CUSTOM)
      {
         start += opts.findPlug("motion_start").asFloat();
         end += opts.findPlug("motion_end").asFloat();
      }
      else
      {
         float frames = opts.findPlug("motion_frames").asFloat();
         
         if (mbtype == MTOA_MBLUR_TYPE_START)
         {
            end += frames;
         }
         else if (mbtype == MTOA_MBLUR_TYPE_END)
         {
            start -= frames;
         }
         else
         {
            float half_frames = 0.5f * frames;
            start -= half_frames;
            end += half_frames;
         }
      }
      
      float incr = (end - start) / (steps - 1);
      
      return start + step * incr;
   }
}
#endif

bool StringToValue(const std::string &sval, CAttrData &data, AtParamValue *val)
{
   if (data.isArray)
   {
      // Temporarily reset isArray so we can call StringToValue for each element
      data.isArray = false;
      
      std::string elt;
      std::vector<AtParamValue> values;
      AtParamValue eval;
      size_t p0 = 0, p1 = sval.find(';', p0);
      while (p1 != std::string::npos)
      {
         elt = sval.substr(p0, p1-p0);
         if (StringToValue(elt, data, &eval))
         {
            values.push_back(eval);
         }
         p0 = p1 + 1;
         p1 = sval.find(';', p0);
      }
      elt = sval.substr(p0);
      if (StringToValue(elt, data, &eval))
      {
         values.push_back(eval);
      }
      
      val->ARRAY = AiArrayAllocate(values.size(), 1, data.type);
      for (size_t i=0; i<values.size(); ++i)
      {
         switch (data.type)
         {
         case AI_TYPE_BOOLEAN:
            AiArraySetBool(val->ARRAY, i, values[i].BOOL);
            break;
         case AI_TYPE_BYTE:
            AiArraySetByte(val->ARRAY, i, values[i].BYTE);
            break;
         case AI_TYPE_INT:
            AiArraySetInt(val->ARRAY, i, values[i].INT);
            break;
         case AI_TYPE_UINT:
            AiArraySetUInt(val->ARRAY, i, values[i].UINT);
            break;
         case AI_TYPE_ENUM:
            AiArraySetInt(val->ARRAY, i, values[i].INT);
            break;
         case AI_TYPE_FLOAT:
            AiArraySetFlt(val->ARRAY, i, values[i].FLT);
            break;
         case AI_TYPE_POINT2:
            AiArraySetPnt2(val->ARRAY, i, values[i].PNT2);
            break;
         case AI_TYPE_POINT:
            AiArraySetPnt(val->ARRAY, i, values[i].PNT);
            break;
         case AI_TYPE_VECTOR:
            AiArraySetVec(val->ARRAY, i, values[i].VEC);
            break;
         case AI_TYPE_RGB:
            AiArraySetRGB(val->ARRAY, i, values[i].RGB);
            break;
         case AI_TYPE_RGBA:
            AiArraySetRGBA(val->ARRAY, i, values[i].RGBA);
            break;
         case AI_TYPE_MATRIX:
            AiArraySetMtx(val->ARRAY, i, *(values[i].pMTX));
            break;
         case AI_TYPE_STRING:
            AiArraySetStr(val->ARRAY, i, values[i].STR);
            break;
         case AI_TYPE_NODE:
            AiArraySetPtr(val->ARRAY, i, NULL);
            break;
         default:
            break;
         }
         DestroyValue(data, &(values[i]));
      }
      
      data.isArray = true;
      
      return true;
   }
   else
   {
      switch (data.type)
      {
      case AI_TYPE_BOOLEAN:
         if (sval == "1" || sval == "on" || sval == "true" || sval == "True")
         {
            val->BOOL = true;
            return true;
         }
         else if (sval == "0" || sval == "off" || sval == "false" || sval == "False")
         {
            val->BOOL = false;
            return true;
         }
         else if (sval != "")
         {
            return false;
         }
      case AI_TYPE_BYTE:
         if (sscanf(sval.c_str(), "%c", &(val->BYTE)) != 1)
         {
            return false;
         }
         return true;
      case AI_TYPE_INT:
         if (sscanf(sval.c_str(), "%d", &(val->INT)) != 1)
         {
            return false;
         }
         return true;
      case AI_TYPE_UINT:
         if (sscanf(sval.c_str(), "%u", &(val->UINT)) != 1)
         {
            return false;
         }
         return true;
      case AI_TYPE_ENUM:
         for (unsigned int i=0; i<data.enums.length(); ++i)
         {
            if (!strcmp(sval.c_str(), data.enums[i].asChar()))
            {
               val->INT = (int) i;
               return true;
            }
         }
         {
            unsigned int index = 0;
            if (sscanf(sval.c_str(), "%u", &index) == 1 && index < data.enums.length())
            {
               val->INT = index;
               return true;
            }
         }
         return false;
      case AI_TYPE_FLOAT:
         if (sscanf(sval.c_str(), "%f", &(val->FLT)) != 1)
         {
            return false;
         }
         return true;
      case AI_TYPE_POINT2:
         if (sscanf(sval.c_str(), "%f,%f",
                    &(val->PNT2.x), &(val->PNT2.y)) != 2)
         {
            return false;
         }
         return true;
      case AI_TYPE_POINT:
         if (sscanf(sval.c_str(), "%f,%f,%f",
                    &(val->PNT.x), &(val->PNT.y), &(val->PNT.z)) != 3)
         {
            return false;
         }
         return true;
      case AI_TYPE_VECTOR:
         if (sscanf(sval.c_str(), "%f,%f,%f",
                    &(val->VEC.x), &(val->VEC.y), &(val->VEC.z)) != 3)
         {
            return false;
         }
         return true;
      case AI_TYPE_RGB:
         if (sscanf(sval.c_str(), "%f,%f,%f",
                    &(val->RGB.r), &(val->RGB.g), &(val->RGB.b)) != 3)
         {
            return false;
         }
         return true;
      case AI_TYPE_RGBA:
         if (sscanf(sval.c_str(), "%f,%f,%f,%f",
                    &(val->RGBA.r), &(val->RGBA.g), &(val->RGBA.b), &(val->RGBA.a)) != 4)
         {
            return false;
         }
         return true;
      case AI_TYPE_MATRIX:
         {
            AtMatrix *mat = (AtMatrix*) malloc(sizeof(AtMatrix));
            if (sscanf(sval.c_str(), "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
                       &((*mat)[0][0]), &((*mat)[0][1]), &((*mat)[0][2]), &((*mat)[0][3]),
                       &((*mat)[1][0]), &((*mat)[1][1]), &((*mat)[1][2]), &((*mat)[1][3]),
                       &((*mat)[2][0]), &((*mat)[2][1]), &((*mat)[2][2]), &((*mat)[2][3]),
                       &((*mat)[3][0]), &((*mat)[3][1]), &((*mat)[3][2]), &((*mat)[3][3])) != 16)
            {
               free(mat);
               return false;
            }
            val->pMTX = mat;
         }
         return true;
      case AI_TYPE_STRING:
         {  
            char *tmp = new char[sval.length()+1];
            strcpy(tmp, sval.c_str());
            val->STR = tmp;
         }
         return true;
      case AI_TYPE_NODE:
         val->PTR = NULL; 
         return true;
      default:
         return false;
      }
   }
}

void DestroyValue(CAttrData &data, AtParamValue *val)
{
   if (data.isArray)
   {
      AiArrayDestroy(val->ARRAY);
   }
   else
   {
      switch (data.type)
      {
      case AI_TYPE_STRING:
         delete[] val->STR;
         break;
      case AI_TYPE_MATRIX:
         free(val->pMTX);
         break;
      default:
         break;
      }
   }
}

bool HasParameter(const AtNodeEntry *anodeEntry, const char *param, AtNode *anode, const char *decl)
{
   if (AiNodeEntryLookUpParameter(anodeEntry, param) != NULL)
   {
      return true;
   }
   else if (anode)
   {
      if (AiNodeLookUpUserParameter(anode, param) != NULL)
      {
         return true;
      }
      else if (decl != NULL)
      {
         return AiNodeDeclare(anode, param, decl);
      }
      else
      {
         return false;
      }
   }
   else
   {
      return false;
   }
}

void NodeInitializer(CAbTranslator context)
{
   std::map<std::string, CScriptedTranslator>::iterator it = gTranslators.find(context.maya.asChar());
   
   if (it != gTranslators.end())
   {
      MObject plugin = MFnPlugin::findPlugin(it->second.requiredPlugin);
      if (plugin.isNull())
      {
         AiMsgWarning("[mtoa.scriptedTranslators] %s requires Maya plugin %s, registering will be deferred until plugin is loaded",
                      it->first.c_str(), it->second.requiredPlugin.asChar());
         AddPluginLoadedCallback();
         it->second.deferred = true;
         return;
      }
      
      CExtensionAttrHelper procHelper(context.maya, "procedural");
      
      if (it->second.isShape)
      {
         CScriptedShapeTranslator::MakeCommonAttributes(procHelper);
      }
      
      MString setupAttrsCmd = it->second.setupAttrsCmd + "()";
      
      if (setupAttrsCmd.length() > 0)
      {
         MStringArray rv;
         MStatus stat = MGlobal::executePythonCommand(setupAttrsCmd, rv);
         
         if (stat == MStatus::kSuccess && rv.length() > 1)
         {
            for (unsigned int i=0; i<rv.length(); ++i)
            {
               CAttrData data;
               std::string arnoldNode, arnoldAttr;
               
               std::string decl = rv[i].asChar();
               size_t p0 = 0, p1;
               
               // attribute type
               p1 = decl.find('|', p0);
               if (p1 == std::string::npos) continue;
               if (sscanf(decl.substr(p0, p1-p0).c_str(), "%d", &data.type) != 1) continue;
               
               // arnold node name
               p0 = p1 + 1;
               p1 = decl.find('|', p0);
               if (p1 == std::string::npos) continue;
               arnoldNode = decl.substr(p0, p1-p0);
               if (arnoldNode.length() == 0) arnoldNode = "procedural";
               
               // arnold attr name
               p0 = p1 + 1;
               p1 = decl.find('|', p0);
               if (p1 == std::string::npos) continue;
               arnoldAttr = decl.substr(p0, p1-p0);
               
               // name
               p0 = p1 + 1;
               p1 = decl.find('|', p0);
               if (p1 == std::string::npos) continue;
               data.name = decl.substr(p0, p1-p0).c_str();
               
               // shortName
               p0 = p1 + 1;
               p1 = decl.find('|', p0);
               if (p1 == std::string::npos) continue;
               data.shortName = decl.substr(p0, p1-p0).c_str();
               
               // isArray
               p0 = p1 + 1;
               p1 = decl.find('|', p0);
               if (p1 == std::string::npos) continue;
               data.isArray = (decl.substr(p0, p1-p0) == "1");
               
               // default
               p0 = p1 + 1;
               p1 = decl.find('|', p0);
               if (p1 == std::string::npos) continue;
               std::string sdefval = decl.substr(p0, p1-p0);
               
               // min
               p0 = p1 + 1;
               p1 = decl.find('|', p0);
               if (p1 == std::string::npos) continue;
               std::string sminval = decl.substr(p0, p1-p0);
               
               // max
               p0 = p1 + 1;
               p1 = decl.find('|', p0);
               if (p1 == std::string::npos) continue;
               std::string smaxval = decl.substr(p0, p1-p0);
               
               // softMin
               p0 = p1 + 1;
               p1 = decl.find('|', p0);
               if (p1 == std::string::npos) continue;
               std::string ssoftminval = decl.substr(p0, p1-p0);
               
               // softMax
               p0 = p1 + 1;
               p1 = decl.find('|', p0);
               if (p1 == std::string::npos) continue;
               std::string ssoftmaxval = decl.substr(p0, p1-p0);
               
               // keyable
               p0 = p1 + 1;
               p1 = decl.find('|', p0);
               if (p1 == std::string::npos) continue;
               data.keyable = (decl.substr(p0, p1-p0) == "1");
               
               // enums
               p0 = p1 + 1;
               p1 = decl.find('|', p0);
               if (p1 != std::string::npos) continue;
               std::string senumvals = decl.substr(p0);
               size_t p2 = 0, p3 = senumvals.find(',', p2);
               while (p3 != std::string::npos)
               {
                  data.enums.append(senumvals.substr(p2, p3-p2).c_str());
                  p2 = p3 + 1;
                  p3 = senumvals.find(',', p2);
               }
               data.enums.append(senumvals.substr(p2).c_str());

               CExtensionAttrHelper helper(context.maya, arnoldNode.c_str());

               // if arnold name is set, ignore any other value
               if (arnoldAttr.length() > 0)
               {
                  helper.MakeInput(arnoldAttr.c_str());
               }
               else if (data.type != AI_TYPE_UNDEFINED)
               {
                  // read values
                  bool hasDefault = StringToValue(sdefval, data, &(data.defaultValue));
                  data.hasMin = false;
                  data.hasMax = false;
                  data.hasSoftMin = false;
                  data.hasSoftMax = false;
                  
                  if (data.type == AI_TYPE_BYTE ||
                      data.type == AI_TYPE_INT ||
                      data.type == AI_TYPE_UINT ||
                      data.type == AI_TYPE_FLOAT)
                  {
                     data.hasMin = StringToValue(sminval, data, &(data.min));
                     data.hasMax = StringToValue(smaxval, data, &(data.max));
                     data.hasSoftMin = StringToValue(ssoftminval, data, &(data.softMin));
                     data.hasSoftMax = StringToValue(ssoftmaxval, data, &(data.softMax));
                  }
                  
                  // create attributes
                  helper.MakeInput(data);
               
                  // cleanup values
                  if (hasDefault) DestroyValue(data, &(data.defaultValue));
                  if (data.hasMin) DestroyValue(data, &(data.min));
                  if (data.hasMax) DestroyValue(data, &(data.max));
                  if (data.hasSoftMin) DestroyValue(data, &(data.softMin));
                  if (data.hasSoftMax) DestroyValue(data, &(data.softMax));
               }
            }
         }
      }
      
      it->second.attrsAdded = true;
      
      if (it->second.deferred)
      {
         // Was deferred, so that SetupAE should not have been called yet
         if (it->second.setupAECmd.length() > 0)
         {
            MGlobal::executePythonCommand(it->second.setupAECmd + "(\"scriptedTranslators\")");
         }
         it->second.deferred = false;
      }
   }
}

// MTOA_SCRIPTED_TRANSLATORS contains nodeType or nodeType,pluginName pair of the form "mesh:myNode,myNodePlugin"
// if the plugin name is not specified, it default to the node type name
void RegisterTranslators(CExtension& plugin)
{
   static const char firstSeparator[] = ":;";
   static const char secondSeparator[] = ",";
   
   MString csVar = "$MTOA_SCRIPTED_TRANSLATORS";
   MString csExpVar = csVar.expandEnvironmentVariablesAndTilde();
   
   if (csExpVar != csVar)
   {
      MGlobal::executePythonCommand(gModuleSetup);
      
      std::string tmp;
      std::string nodeType;
      std::string pluginName;
      std::string nodeList = csExpVar.asChar();
      
      size_t p0 = 0;
      size_t p1 = nodeList.find_first_of(firstSeparator, p0);
      size_t t0 = 0;
      
      while (p1 != std::string::npos)
      {
         tmp = nodeList.substr(p0, p1-p0);
         
         t0 = tmp.find_first_of(secondSeparator);
         if (t0 != std::string::npos)
         {
            nodeType = tmp.substr(0, t0);
            pluginName = tmp.substr(t0+1);
         }
         else
         {
            nodeType = tmp;
            pluginName = nodeType;
         }
         
         if (nodeType.length() > 0 && RegisterTranslator(plugin, nodeType, pluginName))
         {
            MGlobal::displayInfo("[mtoa.scriptedTranslators] Registered translator for node \"" + MString(nodeType.c_str()) + "\"");
         }
         
         p0 = p1 + 1;
         p1 = nodeList.find_first_of(firstSeparator, p0);
      }
      
      t0 = nodeList.find_first_of(secondSeparator, p0);
      if (t0 != std::string::npos)
      {
         nodeType = nodeList.substr(p0, t0-p0);
         pluginName = nodeList.substr(t0+1);
      }
      else
      {
         nodeType = nodeList.substr(p0);
         pluginName = nodeType;
      }
      
      if (nodeType.length() > 0 && RegisterTranslator(plugin, nodeType, pluginName))
      {
         MGlobal::displayInfo("[mtoa.scriptedTranslators] Registered translator for node \"" + MString(nodeType.c_str()) + "\"");
      }
   }
}

bool RegisterTranslator(CExtension& plugin, std::string &nodeType, const std::string &providedByPlugin)
{
   static const char whitespaces[] = " \t\n\v";
   
   // Strip leading and trailing spaces
   size_t w0 = nodeType.find_first_not_of(whitespaces);
   
   if (w0 != std::string::npos)
   {
      size_t w1 = nodeType.find_last_not_of(whitespaces);
      
      nodeType = nodeType.substr(w0, w1-w0+1);
      
      if (nodeType.length() > 0)
      {
         // Check if nodeType already registered
         std::map<std::string, CScriptedTranslator>::iterator it = gTranslators.find(nodeType);
         
         if (it == gTranslators.end())
         {
            // Check if Export function can be found
            std::string pymod = "mtoa_" + nodeType;
            
            if (MGlobal::executePythonCommand(MString("import ") + pymod.c_str()) != MS::kSuccess)
            {
               return false;
            }
            
            std::string exportScript = pymod + ".Export";
            
            int rv = 0;
            MString checkCmdBeg = MString("hasattr(") + pymod.c_str() + ", \"";
            MString checkCmdEnd = "\")";
            
            if (MGlobal::executePythonCommand(checkCmdBeg + "Export" + checkCmdEnd, rv) == MS::kSuccess && rv != 0)
            {
               gTranslators[nodeType].exportCmd = exportScript.c_str();
               gTranslators[nodeType].attrsAdded = false;
               gTranslators[nodeType].deferred = false;
               gTranslators[nodeType].isShape = true;
               gTranslators[nodeType].supportVolumes = false;
               gTranslators[nodeType].supportInstances = false;
               gTranslators[nodeType].requiredPlugin = providedByPlugin.c_str();
               
               std::string isShapeScript = pymod + ".IsShape";
               if (MGlobal::executePythonCommand(checkCmdBeg + "IsShape" + checkCmdEnd, rv) == MS::kSuccess && rv != 0)
               {
                  int result = 0;
                  MGlobal::executePythonCommand(MString(isShapeScript.c_str()) + "()", result);
                  gTranslators[nodeType].isShape = (result != 0);
               }
               
               if (gTranslators[nodeType].isShape)
               {
                  std::string volumeScript = pymod + ".SupportVolumes";
                  if (MGlobal::executePythonCommand(checkCmdBeg + "SupportVolumes" + checkCmdEnd, rv) == MS::kSuccess && rv != 0)
                  {
                     int result = 0;
                     MGlobal::executePythonCommand(MString(volumeScript.c_str()) + "()", result);
                     gTranslators[nodeType].supportVolumes = (result != 0);
                  }
                  else
                  {
                     gTranslators[nodeType].supportVolumes = false;
                  }
                  
                  std::string instanceScript = pymod + ".SupportInstances";
                  if (MGlobal::executePythonCommand(checkCmdBeg + "SupportInstances" + checkCmdEnd, rv) == MS::kSuccess && rv != 0)
                  {
                     int result = 0;
                     MGlobal::executePythonCommand(MString(instanceScript.c_str()) + "()", result);
                     gTranslators[nodeType].supportInstances = (result != 0);
                  }
                  else
                  {
                     gTranslators[nodeType].supportInstances = false;
                  }
               }
               
               std::string cleanupScript = pymod + ".Cleanup";
               if (MGlobal::executePythonCommand(checkCmdBeg + "Cleanup" + checkCmdEnd, rv) != MS::kSuccess || rv == 0)
               {
                  cleanupScript = "";
               }
               gTranslators[nodeType].cleanupCmd = cleanupScript.c_str();
               
               std::string attrScript = pymod + ".SetupAttrs";
               if (MGlobal::executePythonCommand(checkCmdBeg + "SetupAttrs" + checkCmdEnd, rv) != MS::kSuccess || rv == 0)
               {
                  attrScript = "";
               }
               gTranslators[nodeType].setupAttrsCmd = attrScript.c_str();
               
               std::string aeScript = pymod + ".SetupAE";
               if (MGlobal::executePythonCommand(checkCmdBeg + "SetupAE" + checkCmdEnd, rv) != MS::kSuccess || rv == 0)
               {
                  std::string tempPy = "def mtoa_" + nodeType + "_SetupAE(translator):\n";
                  tempPy += "  import scriptedTranslatorUtils\n";
                  tempPy += "  scriptedTranslatorUtils.DefaultSetupAE('" + providedByPlugin + "', '" + nodeType + "', translator, asShape=";
                  tempPy += (gTranslators[nodeType].isShape ? "True)" : "False)");
                  
                  if (MGlobal::executePythonCommand(tempPy.c_str()) != MS::kSuccess)
                  {
                     MGlobal::displayInfo("[mtoa.scriptedTranslators] Could not generate default AE template for node " + MString(nodeType.c_str()));
                     aeScript = "";
                  }
                  else
                  {
                     aeScript = "mtoa_" + nodeType + "_SetupAE";
                  }
               }
               gTranslators[nodeType].setupAECmd = aeScript.c_str();
               
               if (gTranslators[nodeType].isShape)
               {
                  MGlobal::displayInfo(MString("[mtoa.scriptedTranslators] Register \"") + nodeType.c_str() + "\" as shape");
                  plugin.RegisterTranslator(nodeType.c_str(), "scriptedTranslators", CScriptedShapeTranslator::creator, NodeInitializer);
               }
               else
               {
                  MGlobal::displayInfo(MString("[mtoa.scriptedTranslators] Register \"") + nodeType.c_str() + "\" as node");
                  plugin.RegisterTranslator(nodeType.c_str(), "scriptedTranslators", CScriptedNodeTranslator::creator, NodeInitializer);
               }
               return true;
            }
         }
      }
   }
   
   return false;
}



extern "C"
{

DLLEXPORT void initializeExtension(CExtension &extension)
{
   RegisterTranslators(extension);
}

DLLEXPORT void deinitializeExtension(CExtension &)
{
   RemovePluginLoadedCallback();
}

}

