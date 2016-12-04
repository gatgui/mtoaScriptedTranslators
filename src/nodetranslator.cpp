#include "nodetranslator.h"
#include "plugin.h"

void* CScriptedNodeTranslator::creator()
{
   return new CScriptedNodeTranslator();
}

CScriptedNodeTranslator::CScriptedNodeTranslator()
   : CNodeTranslator(), m_motionBlur(false)
{
}

CScriptedNodeTranslator::~CScriptedNodeTranslator()
{
}

#ifdef OLD_API
AtNode* CScriptedNodeTranslator::Init(CArnoldSession* session, const MObject& object, const MString &outputAttr)
{
   AtNode *rv = CNodeTranslator::Init(session, object, outputAttr);
   m_motionBlur = (IsMotionBlurEnabled(MTOA_MBLUR_DEFORM|MTOA_MBLUR_OBJECT) && IsLocalMotionBlurEnabled());
   return rv;
}
#else
void CScriptedNodeTranslator::Init()
{
   CNodeTranslator::Init();
   m_motionBlur = (IsMotionBlurEnabled(MTOA_MBLUR_DEFORM|MTOA_MBLUR_OBJECT) && IsLocalMotionBlurEnabled());
}
#endif

AtNode* CScriptedNodeTranslator::CreateArnoldNodes()
{
   std::map<std::string, CScriptedTranslator>::iterator translatorIt;
   MFnDependencyNode fnNode(GetMayaObject());
   
   translatorIt = gTranslators.find(fnNode.typeName().asChar());
   if (translatorIt == gTranslators.end())
   {
      AiMsgError("[mtoa.scriptedTranslators] No command to export node \"%s\" of type %s.", fnNode.name().asChar(), fnNode.typeName().asChar());
      return NULL;
   }

   return AddArnoldNode("procedural");
}

void CScriptedNodeTranslator::Export(AtNode *atNode)
{
#ifdef OLD_API
   RunScripts(atNode, 0);
#else
   RunScripts(atNode, GetMotionStep());
#endif
}

#ifdef OLD_API
void CScriptedNodeTranslator::ExportMotion(AtNode *atNode, unsigned int step)
{
   RunScripts(atNode, step);
}
#endif

void CScriptedNodeTranslator::Update(AtNode *atNode)
{
#ifdef OLD_API
   RunScripts(atNode, 0, true);
#else
   RunScripts(atNode, GetMotionStep(), true);
#endif
}

#ifdef OLD_API
void CScriptedNodeTranslator::UpdateMotion(AtNode *atNode, unsigned int step)
{
   RunScripts(atNode, step, true);
}
#endif

bool CScriptedNodeTranslator::RequiresMotionData()
{
   return m_motionBlur;
}

void CScriptedNodeTranslator::RunScripts(AtNode *atNode, unsigned int step, bool /* update */)
{
   std::map<std::string, CScriptedTranslator>::iterator translatorIt;
   MFnDependencyNode fnNode(GetMayaObject());
   
   translatorIt = gTranslators.find(fnNode.typeName().asChar());
   if (translatorIt == gTranslators.end())
   {
      AiMsgError("[mtoa.scriptedTranslators] No command to export node \"%s\" of type %s.", fnNode.name().asChar(), fnNode.typeName().asChar());
      return;
   }
   
   MString exportCmd = translatorIt->second.exportCmd;
   MString cleanupCmd = translatorIt->second.cleanupCmd;
   
   MFnDependencyNode node(GetMayaObject());
   
   char buffer[64];
   
   MString command = exportCmd;
   command += "(";
   
   sprintf(buffer, "%f", GetExportFrame());
   command += buffer;
   command += ", ";
   
   sprintf(buffer, "%d", step);
   command += buffer;
   command += ", ";
   
#ifdef OLD_API
   sprintf(buffer, "%f", GetSampleFrame(m_session, step));
#else
   unsigned int nsteps = 0;
   const double *mframes = GetMotionFrames(nsteps);
   sprintf(buffer, "%f", (step < nsteps ? mframes[step] : GetExportFrame()));
#endif
   command += buffer;
   
   command += ", \"(" + node.name() + "\", \"";
   command += AiNodeGetName(atNode);
   command += "\"), None)";
   
   MStringArray attrs;
   MStatus status = MGlobal::executePythonCommand(command, attrs);
   if (!status)
   {
      AiMsgError("[mtoa.scriptedTranslators] Failed to export node \"%s\".", node.name().asChar());
      return;
   }
   
   std::set<std::string> attrsSet;
   for (unsigned int i=0; i<attrs.length(); ++i)
   {
      attrsSet.insert(attrs[i].asChar());
   }
   std::set<std::string>::iterator attrsEnd = attrsSet.end();
   
   if (attrsSet.find("min") == attrsEnd || attrsSet.find("max") == attrsEnd)
   {
      // Either min or max is missing, force load_at_init
      AiNodeSetBool(atNode, "load_at_init", true);
   }
   
   // Will this method be called more than once for the same motion step?
   if (m_exportedSteps.find(step) != m_exportedSteps.end())
   {
      char numstr[16];
      sprintf(numstr, "%u", step);
      MGlobal::displayWarning(MString("[mtoaScriptedTranslator] Motion step already processed: ") + numstr);
   }
   m_exportedSteps.insert(step);
   
   if (!m_motionBlur || m_exportedSteps.size() == GetNumMotionSteps())
   {
      if (cleanupCmd != "")
      {
         command = cleanupCmd + "((\"" + node.name() + "\", \"";
         command += AiNodeGetName(atNode);
         command += "\"), None)";
         
         status = MGlobal::executePythonCommand(command);
         
         if (!status)
         {
            AiMsgError("[mtoa.scriptedTranslators] Failed to cleanup node \"%s\".", node.name().asChar());
         }
      }
   }
}

void CScriptedNodeTranslator::Delete()
{
}


