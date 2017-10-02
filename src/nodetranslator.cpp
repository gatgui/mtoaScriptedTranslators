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

   return AddArnoldNode("pyproc");
}

#ifdef OLD_API

AtNode* CScriptedNodeTranslator::Init(CArnoldSession* session, const MObject& object, const MString &outputAttr)
{
   AtNode *rv = CNodeTranslator::Init(session, object, outputAttr);
   m_motionBlur = (IsMotionBlurEnabled(MTOA_MBLUR_DEFORM|MTOA_MBLUR_OBJECT) && IsLocalMotionBlurEnabled());
   return rv;
}

void CScriptedNodeTranslator::Export(AtNode *atNode)
{
   RunScripts(atNode, 0);
}

void CScriptedNodeTranslator::ExportMotion(AtNode *atNode, unsigned int step)
{
   RunScripts(atNode, step);
}

void CScriptedNodeTranslator::Update(AtNode *atNode)
{
   RunScripts(atNode, 0, true);
}

void CScriptedNodeTranslator::UpdateMotion(AtNode *atNode, unsigned int step)
{
   RunScripts(atNode, step, true);
}

void CScriptedNodeTranslator::Delete()
{
}

#else

void CScriptedNodeTranslator::Init()
{
   CNodeTranslator::Init();
   m_motionBlur = (IsMotionBlurEnabled(MTOA_MBLUR_DEFORM|MTOA_MBLUR_OBJECT) && IsLocalMotionBlurEnabled());
}

void CScriptedNodeTranslator::Export(AtNode *atNode)
{
   RunScripts(atNode, GetMotionStep(), IsExported());
}

void CScriptedNodeTranslator::ExportMotion(AtNode *atNode)
{
   RunScripts(atNode, GetMotionStep(), IsExported());
}

void CScriptedNodeTranslator::RequestUpdate()
{
   SetUpdateMode(AI_RECREATE_NODE);
   CNodeTranslator::RequestUpdate();
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

   #if AI_VERSION_ARCH_NUM < 5
   if (attrsSet.find("min") == attrsEnd || attrsSet.find("max") == attrsEnd)
   {
      // Either min or max is missing, force load_at_init
      AiNodeSetBool(atNode, "load_at_init", true);
   }
   #endif // AI_VERSION_ARCH_NUM < 5

   if (!IsExportingMotion())
   {
      m_exportedSteps.clear();
   }

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
