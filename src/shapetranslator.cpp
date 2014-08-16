#include "shapetranslator.h"
#include "plugin.h"

#include <maya/MBoundingBox.h>


void* CScriptedShapeTranslator::creator()
{
   return new CScriptedShapeTranslator();
}

CScriptedShapeTranslator::CScriptedShapeTranslator()
   : CShapeTranslator(), m_motionBlur(false)
{
}

CScriptedShapeTranslator::~CScriptedShapeTranslator()
{
}

AtNode* CScriptedShapeTranslator::Init(CArnoldSession *session, MDagPath& dagPath, MString outputAttr)
{
   AtNode *rv = CShapeTranslator::Init(session, dagPath, outputAttr);
   m_motionBlur = (IsMotionBlurEnabled(MTOA_MBLUR_DEFORM|MTOA_MBLUR_OBJECT) && IsLocalMotionBlurEnabled());
   return rv;
}

AtNode* CScriptedShapeTranslator::Init(CArnoldSession* session, MObject& object, MString outputAttr)
{
   AtNode *rv = CDagTranslator::Init(session, object, outputAttr);
   m_motionBlur = (IsMotionBlurEnabled(MTOA_MBLUR_DEFORM|MTOA_MBLUR_OBJECT) && IsLocalMotionBlurEnabled());
   return rv;
}

AtNode* CScriptedShapeTranslator::CreateArnoldNodes()
{
   std::map<std::string, CScriptedTranslator>::iterator translatorIt;
   MFnDependencyNode fnNode(GetMayaObject());
   
   translatorIt = gTranslators.find(fnNode.typeName().asChar());
   if (translatorIt == gTranslators.end())
   {
      AiMsgError("[mtoa.scriptedTranslators] No command to export node \"%s\" of type %s.", fnNode.name().asChar(), fnNode.typeName().asChar());
      return NULL;
   }
   
   float step = FindMayaPlug("aiStepSize").asFloat();
   bool asVolume =  (step > AI_EPSILON);
   
   MDagPath masterDag;
   if (DoIsMasterInstance(m_dagPath, masterDag))
   {
      if (asVolume && !translatorIt->second.supportVolumes)
      {
         return AddArnoldNode("box");
      }
      else
      {
         return AddArnoldNode("procedural");
      }
   }
   else
   {
      if (asVolume && !translatorIt->second.supportVolumes)
      {
         return AddArnoldNode("ginstance");
      }
      else
      {
         return AddArnoldNode(translatorIt->second.supportInstances ? "ginstance" : "procedural");
      }
   }
}

void CScriptedShapeTranslator::SetArnoldNodeName(AtNode* arnoldNode, const char* tag)
{
   MDagPath path = GetMayaDagPath();
   MString name = path.partialPathName();
   
   if (m_session && StripNamespaces(m_session))
   {
      RemoveNamespacesIn(name);
   }
   
   if (tag && strlen(tag) > 0)
   {
      name += "_" + MString(tag);
   }
   
   AiNodeSetStr(arnoldNode, "name", name.asChar());
}

// Get shading engine associated with a custom shape
//
void CScriptedShapeTranslator::GetShapeInstanceShader(MDagPath& dagPath, MFnDependencyNode &shadingEngineNode)
{
   // Get instance shadingEngine
   shadingEngineNode.setObject(MObject::kNullObj);
   
   // First try the usual way
   MPlug shadingGroupPlug = GetNodeShadingGroup(dagPath.node(), (dagPath.isInstanced() ? dagPath.instanceNumber() : 0));
   if (!shadingGroupPlug.isNull())
   {
      shadingEngineNode.setObject(shadingGroupPlug.node());
      return;
   }
   
   char buffer[64];
   
   // Check connection from any shadingEngine on shape
   MStringArray connections;
   MGlobal::executeCommand("listConnections -s 1 -d 0 -c 1 -type shadingEngine "+dagPath.fullPathName(), connections);
   
   MSelectionList sl;
   
   if (connections.length() == 0)
   {
      // Check for direct surface shader connection
      bool found = false;
      MGlobal::executeCommand("listConnections -s 1 -d 0 -c 1 "+dagPath.fullPathName(), connections);
      for (unsigned int cidx=0; cidx<connections.length(); cidx+=2)
      {
         MString srcNode = connections[cidx+1];
         // Get node classification, if can find arnold/shader/surface -> got it
         MStringArray rv;
         MGlobal::executeCommand("getClassification `nodeType "+srcNode+"`", rv);
         if (rv.length() > 0 && rv[0].indexW("arnold/shader/surface") != -1)
         {
            connections.clear();
            MGlobal::executeCommand("listConnections -s 0 -d 1 -c 1 -type shadingEngine "+srcNode, connections);
            found = true;
            break;
         }
      }
      if (!found)
      {
         connections.clear();
      }
   }
   
   if (connections.length() == 2)
   {
      // Single connection, use same shader for all instances
      sl.add(connections[1]);
   }
   else if (connections.length() > 2)
   {
      // Many connections, expects the destination plug in shape to be an array
      // Use instance number as logical index
      sprintf(buffer, "[%d]", dagPath.instanceNumber());
      MString iidx = buffer;
      
      for (unsigned int cidx = 0; cidx < connections.length(); cidx += 2)
      {
         MString conn = connections[cidx];
         
         if (conn.length() < iidx.length())
         {
            continue;
         }
         
         if (conn.substring(conn.length() - iidx.length(), conn.length() - 1) != iidx)
         {
            continue;
         }
         
         sl.add(connections[cidx+1]);
         break;
      }
   }
   
   if (sl.length() == 1)
   {
      MObject shadingEngineObj;
      sl.getDependNode(0, shadingEngineObj);
      
      shadingEngineNode.setObject(shadingEngineObj);
   }
}

void CScriptedShapeTranslator::Export(AtNode *atNode)
{
   RunScripts(atNode, 0);
}

void CScriptedShapeTranslator::ExportMotion(AtNode *atNode, unsigned int step)
{
   RunScripts(atNode, step);
}

void CScriptedShapeTranslator::Update(AtNode *atNode)
{
   RunScripts(atNode, 0, true);
}

void CScriptedShapeTranslator::UpdateMotion(AtNode *atNode, unsigned int step)
{
   RunScripts(atNode, step, true);
}

bool CScriptedShapeTranslator::RequiresMotionData()
{
   return m_motionBlur;
}

void CScriptedShapeTranslator::RunScripts(AtNode *atNode, unsigned int step, bool update)
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
   
   MFnDagNode node(m_dagPath.node());
   
   bool isMasterDag = false;
   bool transformBlur = IsMotionBlurEnabled(MTOA_MBLUR_OBJECT) && IsLocalMotionBlurEnabled();
   bool deformBlur = IsMotionBlurEnabled(MTOA_MBLUR_DEFORM) && IsLocalMotionBlurEnabled();
   
   char buffer[64];
   
   MString command = exportCmd;
   command += "(";
   
   sprintf(buffer, "%f", GetExportFrame());
   command += buffer;
   command += ", ";
   
   sprintf(buffer, "%d", step);
   command += buffer;
   command += ", ";
   
   // current sample frame
   sprintf(buffer, "%f", GetMotionStepFrame(m_session, step));
   command += buffer;
   command += ", ";
   
   // List of arnold attributes the custom shape export command has overriden
   MStringArray attrs;
   
   MDagPath masterDag;
   if (DoIsMasterInstance(m_dagPath, masterDag))
   {
      command += "\"" + m_dagPath.partialPathName() + "\", 0, \"\")";
      isMasterDag = true;
      // If IsMasterInstance return true, masterDag isn't set
      masterDag = m_dagPath;
   }
   else
   {
      command += "\"" + m_dagPath.partialPathName() + "\", 1, \"" + masterDag.partialPathName() + "\")";
   }
   
   MStatus status = MGlobal::executePythonCommand(command, attrs);
   if (!status)
   {
      AiMsgError("[mtoa.scriptedTranslators] Failed to export node \"%s\".", node.name().asChar());
      return;
   }
   
   // Should be getting displacement shader from masterDag only [arnold do not support displacement shader overrides for ginstance]
   MFnDependencyNode masterShadingEngine;
   MFnDependencyNode shadingEngine;
   float dispPadding = -AI_BIG;
   float dispHeight = 1.0f;
   float dispZeroValue = 0.0f;
   bool dispAutobump = false;
   bool outputDispPadding = false;
   bool outputDispHeight = false;
   bool outputDispZeroValue = false;
   bool outputDispAutobump = false;
   
   MString anodeName = m_dagPath.partialPathName();
   if (m_session && StripNamespaces(m_session))
   {
      RemoveNamespacesIn(anodeName);
   }
   
   AtNode *anode = AiNodeLookUpByName(anodeName.asChar());
   
   if (anode == NULL)
   {
      return;
   }
   
   const AtNodeEntry *anodeEntry = AiNodeGetNodeEntry(anode);
   
   GetShapeInstanceShader(masterDag, masterShadingEngine);
   GetShapeInstanceShader(m_dagPath, shadingEngine);
   
   AtMatrix matrix;
   MMatrix mmatrix = m_dagPath.inclusiveMatrix();
   ConvertMatrix(matrix, mmatrix);
   
   // Set transformation matrix
   if (!StringInList("matrix", attrs))
   {
      if (HasParameter(anodeEntry, "matrix"))
      {
         if (transformBlur)
         {
            if (step == 0)
            {
               AtArray* matrices = AiArrayAllocate(1, GetNumMotionSteps(), AI_TYPE_MATRIX);
               AiArraySetMtx(matrices, step, matrix);
               AiNodeSetArray(anode, "matrix", matrices);
            }
            else
            {
               AtArray* matrices = AiNodeGetArray(anode, "matrix");
               AiArraySetMtx(matrices, step, matrix);
            }
         }
         else
         {
            AiNodeSetMatrix(anode, "matrix", matrix);
         }
      }
   }
   
   // Set bounding box
   if (!StringInList("min", attrs) && !StringInList("max", attrs))
   {
      // Now check if min and max parameters are valid parameter names on arnold node
      if (HasParameter(anodeEntry, "min") != 0 && HasParameter(anodeEntry, "max") != 0)
      {
         if (step == 0)
         {
            MBoundingBox bbox = node.boundingBox();
            
            MPoint bmin = bbox.min();
            MPoint bmax = bbox.max();
            
            AiNodeSetPnt(anode, "min", static_cast<float>(bmin.x), static_cast<float>(bmin.y), static_cast<float>(bmin.z));
            AiNodeSetPnt(anode, "max", static_cast<float>(bmax.x), static_cast<float>(bmax.y), static_cast<float>(bmax.z));
         }
         else
         {
            if (transformBlur || deformBlur)
            {
               AtPoint cmin = AiNodeGetPnt(anode, "min");
               AtPoint cmax = AiNodeGetPnt(anode, "max");
               
               MBoundingBox bbox = node.boundingBox();
               
               MPoint bmin = bbox.min();
               MPoint bmax = bbox.max();
               
               if (bmin.x < cmin.x)
                  cmin.x = static_cast<float>(bmin.x);
               if (bmin.y < cmin.y)
                  cmin.y = static_cast<float>(bmin.y);
               if (bmin.z < cmin.z)
                  cmin.z = static_cast<float>(bmin.z);
               if (bmax.x > cmax.x)
                  cmax.x = static_cast<float>(bmax.x);
               if (bmax.y > cmax.y)
                  cmax.y = static_cast<float>(bmax.y);
               if (bmax.z > cmax.z)
                  cmax.z = static_cast<float>(bmax.z);
               
               AiNodeSetPnt(anode, "min", cmin.x, cmin.y, cmin.z);
               AiNodeSetPnt(anode, "max", cmax.x, cmax.y, cmax.z);
            }
         }
      }
   }
   
   if (step == 0)
   {
      // Set common attributes
      MPlug plug;
      
      if (!StringInList("subdiv_type", attrs))
      {
         plug = node.findPlug("subdiv_type");
         if (plug.isNull())
         {
            plug = node.findPlug("aiSubdivType");
         }
         if (!plug.isNull() && HasParameter(anodeEntry, "subdiv_type", anode, "constant INT"))
         {
            AiNodeSetInt(anode, "subdiv_type", plug.asInt());
         }
      }
      
      if (!StringInList("subdiv_iterations", attrs))
      {
         plug = node.findPlug("subdiv_iterations");
         if (plug.isNull())
         {
            plug = node.findPlug("aiSubdivIterations");
         }
         if (!plug.isNull() && HasParameter(anodeEntry, "subdiv_iterations", anode, "constant BYTE"))
         {
            AiNodeSetByte(anode, "subdiv_iterations", plug.asInt());
         }
      }
      
      if (!StringInList("subdiv_adaptive_metric", attrs))
      {
         plug = node.findPlug("subdiv_adaptive_metric");
         if (plug.isNull())
         {
            plug = node.findPlug("aiSubdivAdaptiveMetric");
         }
         if (!plug.isNull() && HasParameter(anodeEntry, "subdiv_adaptive_metric", anode, "constant INT"))
         {
            AiNodeSetInt(anode, "subdiv_adaptive_metric", plug.asInt());
         }
      }
      
      if (!StringInList("subdiv_pixel_error", attrs))
      {
         plug = node.findPlug("subdiv_pixel_error");
         if (plug.isNull())
         {
            plug = node.findPlug("aiSubdivPixelError");
         }
         if (!plug.isNull() && HasParameter(anodeEntry, "subdiv_pixel_error", anode, "constant FLOAT"))
         {
            AiNodeSetFlt(anode, "subdiv_pixel_error", plug.asFloat());
         }
      }
      
      if (!StringInList("subdiv_dicing_camera", attrs))
      {
         plug = node.findPlug("subdiv_dicing_camera");
         if (plug.isNull())
         {
            plug = node.findPlug("aiSubdivDicingCamera");
         }
         if (!plug.isNull() && HasParameter(anodeEntry, "subdiv_dicing_camera", anode, "constant NODE"))
         {
            MString cameraName = plug.asString();
            
            AtNode *cameraNode = NULL;
            
            if (cameraName != "" && cameraName != "Default")
            {
               if (m_session && StripNamespaces(m_session)) RemoveNamespacesIn(cameraName);
               cameraNode = AiNodeLookUpByName(cameraName.asChar());
            }
            
            AiNodeSetPtr(anode, "subdiv_dicing_camera", cameraNode);
         }
      }
      
      if (!StringInList("subdiv_uv_smoothing", attrs))
      {
         plug = node.findPlug("subdiv_uv_smoothing");
         if (plug.isNull())
         {
            plug = node.findPlug("aiSubdivUvSmoothing");
         }
         if (!plug.isNull() && HasParameter(anodeEntry, "subdiv_uv_smoothing", anode, "constant INT"))
         {
            AiNodeSetInt(anode, "subdiv_uv_smoothing", plug.asInt());
         }
      }
      
      if (!StringInList("subdiv_smooth_derivs", attrs))
      {
         plug = node.findPlug("aiSubdivSmoothDerivs");
         if (!plug.isNull() && HasParameter(anodeEntry, "subdiv_smooth_derivs", anode, "constant BOOL"))
         {
            AiNodeSetBool(anode, "subdiv_smooth_derivs", plug.asBool());
         }
      }
      
      if (!StringInList("smoothing", attrs))
      {
         // Use maya shape built-in attribute
         plug = node.findPlug("smoothShading");
         if (!plug.isNull() && HasParameter(anodeEntry, "smoothing", anode, "constant BOOL"))
         {
            AiNodeSetBool(anode, "smoothing", plug.asBool());
         }
      }
      
      if (!StringInList("disp_height", attrs))
      {
         plug = node.findPlug("aiDispHeight");
         if (!plug.isNull())
         {
            outputDispHeight = true;
            dispHeight = plug.asFloat();
         }
      }
      
      if (!StringInList("disp_zero_value", attrs))
      {
         plug = node.findPlug("aiDispZeroValue");
         if (!plug.isNull())
         {
            outputDispZeroValue = true;
            dispZeroValue = plug.asFloat();
         }
      }
      
      if (!StringInList("disp_autobump", attrs))
      {
         plug = node.findPlug("aiDispAutobump");
         if (!plug.isNull())
         {
            outputDispAutobump = true;
            dispAutobump = plug.asBool();
         }
      }
      
      if (!StringInList("disp_padding", attrs))
      {
         plug = node.findPlug("aiDispPadding");
         if (!plug.isNull())
         {
            outputDispPadding = true;
            dispPadding = MAX(dispPadding, plug.asFloat());
         }
      }
      
      // Set diplacement shader
      if (!StringInList("disp_map", attrs))
      {
         if (masterShadingEngine.object() != MObject::kNullObj)
         {
            MPlugArray shaderConns;
            
            MPlug shaderPlug = masterShadingEngine.findPlug("displacementShader");
            
            shaderPlug.connectedTo(shaderConns, true, false);
            
            if (shaderConns.length() > 0)
            {
               MFnDependencyNode dispNode(shaderConns[0].node());
               
               plug = dispNode.findPlug("aiDisplacementPadding");
               if (!plug.isNull())
               {
                  outputDispPadding = true;
                  dispPadding = MAX(dispPadding, plug.asFloat());
               }
               
               plug = dispNode.findPlug("aiDisplacementAutoBump");
               if (!plug.isNull())
               {
                  outputDispAutobump = true;
                  dispAutobump = dispAutobump || plug.asBool();
               }
               
               if (HasParameter(anodeEntry, "disp_map", anode, "constant ARRAY NODE"))
               {
                  AtNode *dispImage = ExportNode(shaderConns[0]);
                  AiNodeSetArray(anode, "disp_map", AiArrayConvert(1, 1, AI_TYPE_NODE, &dispImage));
               }
            }
         }
      }
      
      if (outputDispHeight && HasParameter(anodeEntry, "disp_height", anode, "constant FLOAT"))
      {
         AiNodeSetFlt(anode, "disp_height", dispHeight);
      }
      if (outputDispZeroValue && HasParameter(anodeEntry, "disp_zero_value", anode, "constant FLOAT"))
      {
         AiNodeSetFlt(anode, "disp_zero_value", dispZeroValue);
      }
      if (outputDispPadding && HasParameter(anodeEntry, "disp_padding", anode, "constant FLOAT"))
      {
         AiNodeSetFlt(anode, "disp_padding", dispPadding);
      }
      if (outputDispAutobump && HasParameter(anodeEntry, "disp_autobump", anode, "constant BOOL"))
      {
         AiNodeSetBool(anode, "disp_autobump", dispAutobump);
      }
      
      if (!StringInList("sidedness", attrs))
      {
         // Use maya shape built-in attribute
         plug = node.findPlug("doubleSided");
         if (!plug.isNull() && HasParameter(anodeEntry, "sidedness", anode, "constant BYTE"))
         {
            AiNodeSetByte(anode, "sidedness", plug.asBool() ? AI_RAY_ALL : 0);
            
            // Only set invert_normals if doubleSided attribute could be found
            if (!plug.asBool() && !StringInList("invert_normals", attrs))
            {
               // Use maya shape built-in attribute
               plug = node.findPlug("opposite");
               if (!plug.isNull() && HasParameter(anodeEntry, "invert_normals", anode, "constant BOOL"))
               {
                  AiNodeSetBool(anode, "invert_normals", plug.asBool());
               }
            }
         }
      }
      
      // Old point based SSS parameter
      if (!StringInList("sss_sample_distribution", attrs))
      {
         plug = node.findPlug("sss_sample_distribution");
         if (plug.isNull())
         {
            plug = node.findPlug("aiSssSampleDistribution");
         }
         if (!plug.isNull() && HasParameter(anodeEntry, "sss_sample_distribution", anode, "constant INT"))
         {
            AiNodeSetInt(anode, "sss_sample_distribution", plug.asInt());
         }
      }
      
      // Old point based SSS parameter
      if (!StringInList("sss_sample_spacing", attrs))
      {
         plug = node.findPlug("sss_sample_spacing");
         if (plug.isNull())
         {
            plug = node.findPlug("aiSssSampleSpacing");
         }
         if (!plug.isNull() && HasParameter(anodeEntry, "sss_sample_spacing", anode, "constant FLOAT"))
         {
            AiNodeSetFlt(anode, "sss_sample_spacing", plug.asFloat());
         }
      }
      
      if (!StringInList("sss_setname", attrs))
      {
         plug = node.findPlug("aiSssSetname");
         if (!plug.isNull() && plug.asString().length() > 0)
         {
            if (HasParameter(anodeEntry, "sss_setname", anode, "constant STRING"))
            {
               AiNodeSetStr(anode, "sss_setname", plug.asString().asChar());
            }
         }
      }
      
      if (!StringInList("receive_shadows", attrs))
      {
         // Use maya shape built-in attribute
         plug = node.findPlug("receiveShadows");
         if (!plug.isNull() && HasParameter(anodeEntry, "receive_shadows", anode, "constant BOOL"))
         {
            AiNodeSetBool(anode, "receive_shadows", plug.asBool());
         }
      }
      
      if (!StringInList("self_shadows", attrs))
      {
         plug = node.findPlug("self_shadows");
         if (plug.isNull())
         {
            plug = node.findPlug("aiSelfShadows");
         }
         if (!plug.isNull() && HasParameter(anodeEntry, "self_shadows", anode, "constant BOOL"))
         {
            AiNodeSetBool(anode, "self_shadows", plug.asBool());
         }
      }
      
      if (!StringInList("opaque", attrs))
      {
         plug = node.findPlug("opaque");
         if (plug.isNull())
         {
            plug = node.findPlug("aiOpaque");
         }
         if (!plug.isNull() && HasParameter(anodeEntry, "opaque", anode, "constant BOOL"))
         {
            AiNodeSetBool(anode, "opaque", plug.asBool());
         }
      }
      
      if (!StringInList("visibility", attrs))
      {
         if (HasParameter(anodeEntry, "visibility", anode, "constant BYTE"))
         {
            int visibility = AI_RAY_ALL;
            
            // Use maya shape built-in attribute
            plug = node.findPlug("castsShadows");
            if (!plug.isNull() && !plug.asBool())
            {
               visibility &= ~AI_RAY_SHADOW;
            }
            
            // Use maya shape built-in attribute
            plug = node.findPlug("primaryVisibility");
            if (!plug.isNull() && !plug.asBool())
            {
               visibility &= ~AI_RAY_CAMERA;
            }
            
            // Use maya shape built-in attribute
            plug = node.findPlug("visibleInReflections");
            if (!plug.isNull() && !plug.asBool())
            {
               visibility &= ~AI_RAY_REFLECTED;
            }
            
            // Use maya shape built-in attribute
            plug = node.findPlug("visibleInRefractions");
            if (!plug.isNull() && !plug.asBool())
            {
               visibility &= ~AI_RAY_REFRACTED;
            }
            
            plug = node.findPlug("diffuse_visibility");
            if (plug.isNull())
            {
               plug = node.findPlug("aiVisibleInDiffuse");
            }
            if (!plug.isNull() && !plug.asBool())
            {
               visibility &= ~AI_RAY_DIFFUSE;
            }
            
            plug = node.findPlug("glossy_visibility");
            if (plug.isNull())
            {
               plug = node.findPlug("aiVisibleInGlossy");
            }
            if (!plug.isNull() && !plug.asBool())
            {
               visibility &= ~AI_RAY_GLOSSY;
            }
            
            AiNodeSetByte(anode, "visibility", visibility & 0xFF);
         }
      }
      
      if (!StringInList("step_size", attrs))
      {
         plug = node.findPlug("step_size");
         if (plug.isNull())
         {
            plug = node.findPlug("aiStepSize");
         }
         if (!plug.isNull() && HasParameter(anodeEntry, "step_size", anode, "constant FLOAT"))
         {
            AiNodeSetFlt(anode, "step_size", plug.asFloat());
         }
      }
      
      if (AiNodeIs(anode, "ginstance") && !StringInList("node", attrs))
      {
         MDagPath &masterDag = GetMasterInstance();
         AtNode *masterNode = AiNodeLookUpByName(masterDag.partialPathName().asChar());
         if (masterNode)
         {
            AiNodeSetPtr(anode, "node", masterNode);
         }
      }
      
      // Set surface shader
      if (HasParameter(anodeEntry, "shader", anode, "constant NODE"))
      {
         if (!StringInList("shader", attrs))
         {
            if (shadingEngine.object() != MObject::kNullObj)
            {
               AtNode *shader = ExportNode(shadingEngine.findPlug("message"));
               if (shader != NULL)
               {
                  AiNodeSetPtr(anode, "shader", shader);
                  
                  if (AiNodeLookUpUserParameter(anode, "mtoa_shading_groups") == 0)
                  {
                     AiNodeDeclare(anode, "mtoa_shading_groups", "constant ARRAY NODE");
                     AiNodeSetArray(anode, "mtoa_shading_groups", AiArrayConvert(1, 1, AI_TYPE_NODE, &shader));
                  }
               }
            }
         }
      }
   }
   
   ExportLightLinking(anode);
   
   MPlug plug = FindMayaPlug("aiTraceSets");
   if (!plug.isNull())
   {
      ExportTraceSets(anode, plug);
   }
   
   // Call cleanup command on last export step
   
   if (!IsMotionBlurEnabled() || !IsLocalMotionBlurEnabled() || int(step) >= (int(GetNumMotionSteps()) - 1))
   {
      if (HasParameter(anodeEntry, "disp_padding", anode))
      {
         float padding = AiNodeGetFlt(anode, "disp_padding");
         
         AtPoint cmin = AiNodeGetPnt(anode, "min");
         AtPoint cmax = AiNodeGetPnt(anode, "max");
         
         cmin.x -= padding;
         cmin.y -= padding;
         cmin.z -= padding;
         cmax.x += padding;
         cmax.y += padding;
         cmax.z += padding;
         
         AiNodeSetPnt(anode, "min", cmin.x, cmin.y, cmin.z);
         AiNodeSetPnt(anode, "max", cmax.x, cmax.y, cmax.z);
      }
      
      if (cleanupCmd != "")
      {
         command = cleanupCmd + "(\"" + m_dagPath.partialPathName() + "\", ";
         
         if (isMasterDag)
         {
            command += "0, \"\")";
         }
         else
         {
            command += "1, \"" + masterDag.partialPathName() + "\")";
         }
         
         status = MGlobal::executePythonCommand(command);
         
         if (!status)
         {
            AiMsgError("[mtoa.scriptedTranslators] Failed to cleanup node \"%s\".", node.name().asChar());
         }
      }
   }
}

void CScriptedShapeTranslator::Delete()
{
}


