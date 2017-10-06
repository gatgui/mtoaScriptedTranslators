#ifndef __shapetranslator_h__
#define __shapetranslator_h__

#include "common.h"
#include "translators/shape/ShapeTranslator.h"
#include "extension/Extension.h"
#include <set>

class CScriptedShapeTranslator : public CShapeTranslator
{
public:
   
   friend void NodeInitializer(CAbTranslator);
   
   CScriptedShapeTranslator();
   virtual ~CScriptedShapeTranslator();
   
   virtual AtNode* CreateArnoldNodes();
   virtual void Export(AtNode* atNode);
   virtual bool RequiresMotionData();
   virtual void Init();
   virtual void ExportMotion(AtNode *atNode);
   virtual void RequestUpdate();
   
   static void* creator();
   
private:
   
   void RunScripts(AtNode *atNode, unsigned int step, bool update=false);
   void GetShapeInstanceShader(MDagPath &dagPath, MFnDependencyNode &shadingEngineNode);
   
private:
   
   bool m_motionBlur;
   AtNode *m_masterNode;
   std::set<unsigned int> m_exportedSteps;
   AtString m_procType;
};

#endif
