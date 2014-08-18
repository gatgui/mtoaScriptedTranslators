#ifndef __nodetranslator_h__
#define __nodetranslator_h__

#include "translators/NodeTranslator.h"
#include "extension/Extension.h"

class CScriptedNodeTranslator : public CNodeTranslator
{
public:
   
   CScriptedNodeTranslator();
   virtual ~CScriptedNodeTranslator();
   
   virtual AtNode* CreateArnoldNodes();
   virtual AtNode* Init(CArnoldSession* session, MObject &object, MString outputAttr="");
   virtual void Export(AtNode* atNode);
   virtual void ExportMotion(AtNode* atNode, unsigned int step);
   virtual void Update(AtNode *atNode);
   virtual void UpdateMotion(AtNode* atNode, unsigned int step);
   virtual bool RequiresMotionData();
   virtual void Delete();
   
   static void* creator();
   
private:
   
   void RunScripts(AtNode *atNode, unsigned int step, bool update=false);
   
private:
   
   bool m_motionBlur;
};

#endif
