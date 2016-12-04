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
#ifdef OLD_API
   virtual AtNode* Init(CArnoldSession* session, const MObject &object, const MString &outputAttr="");
#else
   virtual void Init();
#endif
   virtual void Export(AtNode* atNode);
   virtual void Update(AtNode *atNode);
#ifdef OLD_API
   virtual void ExportMotion(AtNode* atNode, unsigned int step);
   virtual void UpdateMotion(AtNode* atNode, unsigned int step);
#endif
   virtual bool RequiresMotionData();
   virtual void Delete();
   
   static void* creator();
   
private:
   
   void RunScripts(AtNode *atNode, unsigned int step, bool update=false);
   
private:
   
   bool m_motionBlur;
   std::set<unsigned int> m_exportedSteps;
};

#endif
