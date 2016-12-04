#ifndef __shapetranslator_h__
#define __shapetranslator_h__

#include "common.h"
#include "translators/shape/ShapeTranslator.h"
#include "extension/Extension.h"

class CScriptedShapeTranslator : public CShapeTranslator
{
public:
   
   friend void NodeInitializer(CAbTranslator);
   
   CScriptedShapeTranslator();
   virtual ~CScriptedShapeTranslator();
   
   virtual AtNode* CreateArnoldNodes();
#ifdef OLD_API
   virtual AtNode* Init(CArnoldSession* session, MDagPath &dagPath, MString outputAttr="");
   virtual AtNode* Init(CArnoldSession* session, MObject &object, MString outputAttr="");
#else
   virtual void Init();
#endif
   virtual void Export(AtNode* atNode);
   virtual void Update(AtNode *atNode);
   virtual bool RequiresMotionData();
   virtual void Delete();
   
#ifdef OLD_API
   virtual void ExportMotion(AtNode* atNode, unsigned int step);
   virtual void UpdateMotion(AtNode* atNode, unsigned int step);
#endif
   
   static void* creator();
   
private:
   
   void RunScripts(AtNode *atNode, unsigned int step, bool update=false);
   void GetShapeInstanceShader(MDagPath &dagPath, MFnDependencyNode &shadingEngineNode);
   
private:
   
   bool m_motionBlur;
   AtNode *m_masterNode;
};

#endif
