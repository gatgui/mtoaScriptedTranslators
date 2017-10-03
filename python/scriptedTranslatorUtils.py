import re
import arnold
import maya.cmds as cmds
import maya.OpenMaya as OpenMaya

_fps = {
  "game": 15.0,
  "film": 24.0,
  "pal": 25.0,
  "ntsc": 30.0,
  "show": 48.0,
  "palf": 50.0,
  "ntscf": 60.0,
  "millisec": 1000.0,
  "sec": 1.0,
  "min": 1.0 / 60.0,
  "hour": 1.0 / 3600.0
}

_verexp = re.compile(r"(\d+)\.(\d+).(\d+)")

_arnold5 = (int(arnold.AiGetVersion()[0]) >= 5)

def GetMtoAVersion():
    global _verexp
    
    v = cmds.pluginInfo("mtoa", query=1, version=1)
    m = _verexp.search(v)
    if m != None:
        return (int(m.group(1)), int(m.group(2)), int(m.group(3)))
    else:
        return (0, 0, 0)

def IsMtoAVersionAtLeast(majv, minv, patchv):
    M, m, p = GetMtoAVersion()
    if M < majv:
        return False
    elif M != majv:
        return True
    if m < minv:
        return False
    elif m != minv:
        return True
    if p < patchv:
        return False
    else:
        return True

def GetFPS():
    global _fps
  
    rv = OpenMaya.MGlobal.executeCommandStringResult("currentUnit -q -t")
    
    if rv in _fps:
        return _fps[rv]
      
    else:
        m = re.match(r"(\d+)fps", rv)
        
        if m:
            fps = float(m.group(1))
        else:
            fps = 1.0
        
        _fps[rv] = fps
        
        return fps

def EvaluateFrames(node, timeAttr, inRenderFrame):
    """
    timeAttr may have expression applied
    EvaluateFrames returns adequatly modified render and sample frames
    """
    timePlug = "%s.%s" % (node, timeAttr)
    outRenderFrame = inRenderFrame
    outSampleFrame = cmds.getAttr(timePlug)
    
    conns = cmds.listConnections(timePlug, s=1, d=0, sh=1, p=1)
    if conns != None:
        restoreConns = []
        srcPlug = conns[0]
        srcNode = srcPlug.split(".")[0]
        hist = cmds.listHistory(srcNode)
        if hist != None:
            for h in hist:
                tconns = cmds.listConnections(h, s=1, d=0, p=1, c=1, type="time")
                if tconns is None:
                    continue
                i = 0
                n = len(tconns)
                while i < n:
                    restoreConns.append((tconns[i+1], tconns[i]))
                    cmds.disconnectAttr(tconns[i+1], tconns[i])
                    cmds.setAttr(tconns[i], inRenderFrame)
                    i += 2
        if len(restoreConns) > 0:
            outRenderFrame = cmds.getAttr(srcPlug)
            for src, dst in restoreConns:
                cmds.connectAttr(src, dst)
   
    return (outRenderFrame, outSampleFrame)

def GetOverrideAttr(nodeName, attrName, failedValue=None, returnValueProcess=None, verbose=False):
    if not cmds.attributeQuery(attrName, node=nodeName, exists=1):
        if verbose:
            print("scriptedTranslatorUtils.GetOverrideAttr: No attribute \"%s\" on node \"%s\"" % (attrName, nodeName))
        return failedValue
    
    try:
        retval = failedValue
        found = False
        osets = cmds.ls(type="objectSet")
        if osets:
            osets = filter(lambda x: cmds.getAttr(x+".aiOverride") and cmds.attributeQuery(attrName, node=x, exists=1), osets)
            if len(osets) > 0:
                if verbose:
                    print("scriptedTranslatorUtils.GetOverrideAttr: Candidate sets %s" % osets)
                # Get node full path list
                nodeList = cmds.ls(nodeName, long=1)[0].split("|")
                # Start with node itself
                i = len(nodeList)
                # First element is empty string
                while i > 1:
                    name = "|".join(nodeList[:i])
                    if verbose:
                        print("scriptedTranslatorUtils.GetOverrideAttr: Check for %s" % name)
                    for oset in osets:
                        if cmds.sets(name, isMember=oset):
                            if verbose:
                                print("scriptedTranslatorUtils.GetOverrideAttr: Member of %s" % oset)
                            retval = cmds.getAttr("%s.%s" % (oset, attrName))
                            found = True
                            break
                        else:
                            if verbose:
                                print("scriptedTranslatorUtils.GetOverrideAttr: Not member of %s" % oset)
                    if found:
                        break
                    i -= 1
        
        if not found:
            retval = cmds.getAttr("%s.%s" % (nodeName, attrName))
        
        if returnValueProcess:
            return returnValueProcess(retval)
        else:
            return retval
    
    except Exception, e:
        print("scriptedTranslatorUtils.GetOverrideAttr: Failed to get value: %s" % e)
        return failedValue

def GetDeformationBlur(nodeName):
    if cmds.getAttr("defaultArnoldRenderOptions.motion_blur_enable") and \
       cmds.getAttr("defaultArnoldRenderOptions.mb_object_deform_enable"):
        return GetOverrideAttr(nodeName, "motionBlur", False)
    else:
        return False

def GetTransformationBlur(nodeName):
    if cmds.getAttr("defaultArnoldRenderOptions.motion_blur_enable"):
        return GetOverrideAttr(nodeName, "motionBlur", False)
    else:
        return False

class AttrData(object):
    TrueValues = ["1", "on", "true", "True"]
    FalseValues = ["0", "off", "false", "False"]
    ListTypes = [arnold.AI_TYPE_VECTOR, arnold.AI_TYPE_RGB, arnold.AI_TYPE_RGBA, arnold.AI_TYPE_MATRIX] +
                 ([arnold.AI_TYPE_POINT2, arnold.AI_TYPE_POINT] if not _arnold5 else [arnold.AI_TYPE_VECTOR2])
    
    def __init__(self, **kwargs):
        super(AttrData, self).__init__()
        self.reset()
        for k, v in kwargs.iteritems():
            if hasattr(self, k):
                setattr(self, k, v)
    
    def reset(self):
        self.isArray = False
        self.keyable = False
        self.type = arnold.AI_TYPE_UNDEFINED
        self.name = ""
        self.shortName = ""
        self.defaultValue = None
        self.min = None
        self.max = None
        self.softMin = None
        self.softMax = None
        self.enums = []
        self.arnoldNode = ""
        self.arnoldAttr = ""
    
    def elementToString(self, v):
        if self.type in self.ListTypes:
            return ",".join(v)
        else:
            return str(v)
    
    def stringToElement(self, s):
        if self.type in self.ListTypes:
            return map(lambda x: float(x), s.split(","))
        elif self.type == arnold.AI_TYPE_BOOLEAN:
            if s in self.TrueValues:
                return True
            elif s in self.FalseValues:
                return False
            else:
                raise Exception("Invalid boolean value \"%s\"" % s)
        elif self.type in [arnold.AI_TYPE_BYTE, arnold.AI_TYPE_INT, arnold.AI_TYPE_UINT]:
            return int(s)
        elif self.type == arnold.AI_TYPE_FLOAT:
            return float(s)
        elif self.type == arnold.AI_TYPE_ENUM:
            if not s in self.enums:
                try:
                    return self.enums[int(s)]
                except:
                    pass
                raise Exception("Invalid enum value or index \"%s\" (must be in %s)" % (s, self.enums))
            return s
        elif self.type == arnold.AI_TYPE_STRING:
            return s
        elif self.type == arnold.AI_TYPE_NODE:
            return s
        else:
            raise Exception("Unsupported type %d" % self.type)
    
    def valueToString(self, v):
        if self.isArray:
            return ";".join(map(lambda x: self.elementToString(x), v))
        else:
            return self.elementToString(v)
    
    def stringToValue(self, s):
        if self.isArray:
            return map(lambda x: self.stringToElement(x.strip()), s.split(";"))
        else:
            return self.stringToElement(s)
    
    def __str__(self):
        s  = "%d" % self.type
        s += "|%s" % self.arnoldNode
        s += "|%s" % self.arnoldAttr
        s += "|%s" % self.name
        s += "|%s" % self.shortName
        s += "|%d" % (1 if self.isArray else 0)
        s += "|%s" % ("" if self.defaultValue is None else self.valueToString(self.defaultValue))
        s += "|%s" % ("" if self.min is None else self.valueToString(self.min))
        s += "|%s" % ("" if self.max is None else self.valueToString(self.max))
        s += "|%s" % ("" if self.softMin is None else self.valueToString(self.softMin))
        s += "|%s" % ("" if self.softMax is None else self.valueToString(self.softMax))
        s += "|%d" % (1 if self.keyable else 0)
        s += "|%s" % ",".join(self.enums)
        return s
    
    def parse(self, s):
        self.reset()
        spl = s.split("|")
        if len(spl) != 13:
            return False
        try:
            self.type = int(spl[0])
            self.arnoldNode = spl[1]
            self.arnoldAttr = spl[2]
            self.name = spl[3]
            self.shortName = spl[4]
            # read enums before
            self.enums = filter(lambda x: x, spl[12].split(","))
            self.isArray = (int(spl[5]) == 1)
            self.defaultValue = (self.stringToValue(spl[6]) if spl[6] else None)
            self.min = (self.stringToValue(spl[7]) if spl[7] else None)
            self.max = (self.stringToValue(spl[8]) if spl[8] else None)
            self.softMin = (self.stringToValue(spl[9]) if spl[9] else None)
            self.softMax = (self.stringToValue(spl[10]) if spl[10] else None)
            self.keyable = (int(spl[11]) == 1)
            return True
        except Exception, e:
            print(e)
            self.reset()
            return False


def DefaultSetupAE(pluginName, nodeType, translator, asShape=True):
   import pymel.core as pm
   if not pm.pluginInfo(pluginName, query=1, loaded=1):
      return
  
   try:
      import mtoa.ui.ae.templates as templates
      
      if not asShape:
         class DefaultNodeTemplate(templates.AttributeTemplate):
            def setup(self):
               self.addControl("aiUserOptions", label="User Options")
         
         templates.registerTranslatorUI(DefaultNodeTemplate, nodeType, translator)
      
      else:
         class DefaultShapeTemplate(templates.ShapeTranslatorTemplate):
            def setup(self):
               #self.renderStatsAttributes()
               self.commonShapeAttributes()
               self.addSeparator()
               #self.addControl("aiSssSampleDistribution", label="SSS Samples Distribution")
               #self.addControl("aiSssSampleSpacing", label="SSS Sample Spacing")
               self.addControl("aiSssSetname", label="SSS Set Name")
               self.addSeparator()
               self.addControl("aiUserOptions", label="User Options")
               self.addSeparator()
         
         templates.registerTranslatorUI(DefaultShapeTemplate, nodeType, translator)
      
   except Exception, e:
      print("scriptedTranslatorUtils.DefaultSetupAE: Failed\n%s" % e)


