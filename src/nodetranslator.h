#ifndef __nodetranslator_h__
#define __nodetranslator_h__

#include "common.h"
#include "translators/NodeTranslator.h"
#include "extension/Extension.h"
#include <set>

class CScriptedNodeTranslator : public CNodeTranslator
{
public:
   
   CScriptedNodeTranslator();
   virtual ~CScriptedNodeTranslator();
   
   virtual AtNode* CreateArnoldNodes();
   virtual void Export(AtNode* atNode);
   virtual bool RequiresMotionData();
   virtual void Init();
   virtual void ExportMotion(AtNode *atNode);
   virtual void RequestUpdate();

   static void* creator();
   
private:
   
   void RunScripts(AtNode *atNode, unsigned int step, bool update=false);
   
private:
   
   bool m_motionBlur;
   std::set<unsigned int> m_exportedSteps;
};

#endif
