#ifndef __shapetranslator_h__
#define __shapetranslator_h__

#include "translators/shape/ShapeTranslator.h"
#include "extension/Extension.h"

class CScriptedShapeTranslator : public CShapeTranslator
{
public:
   
   friend void NodeInitializer(CAbTranslator);
   
   CScriptedShapeTranslator();
   virtual ~CScriptedShapeTranslator();
   
   virtual AtNode* CreateArnoldNodes();
   virtual AtNode* Init(CArnoldSession* session, MDagPath &dagPath, MString outputAttr="");
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
   void GetShapeInstanceShader(MDagPath &dagPath, MFnDependencyNode &shadingEngineNode);
   
private:
   
   bool m_motionBlur;
   MDagPath m_masterPath;
   AtNode *m_masterNode;
};

#endif
