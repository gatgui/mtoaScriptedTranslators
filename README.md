mtoaScriptedTranslators
=======================

  A Maya to Arnold (MtoA) extension to allow export of new node types using python scripts.



Requirements
============
  - Arnold 4.1 or above
  - Maya 2013 or above
  - MtoA 1.0.0.1 or above


Build
=====
  scons with-mtoa=<path_to_mtoa_install_dir>
        with-arnold=<path_to_arnold_install_dir>
        with-maya=<path_to_maya_install_dir>
  
  Note:
    If maya is installed in its default system location, the 'maya-ver=<target_maya_version>' can be used instead of 'with-maya=...'
    
  Build output should appear in release/x64


Install
=======

  Add the build output folder to your MAYA_EXTENSIONS_PATH


Usage
=====

  When the extension is loaded by MtoA, it will look at the contents of the
  MTOA_SCRIPTED_TRANSLATORS environment variable.
  
  It expects a colon or semicolon separated list of '<node_type>,<plugin_name>' items. If the plugin name matches the node type, it can be omitted.
  
  For each found node type, the extension will try to import a python module named 'mtoa_<node_type>'. When it succeeds doing so, it will then look up for a function named 'Export' in the module. Only then will the node type be registered to MtoA.
  
  Other functions may be defined to control the behavior of the extension, but only the 'Export' one is required. Follows a full list of recognized functions with their arguments and expected return values:
  
  - IsShape() [default: True]
    Returns whether or not the translator should be based on CNodeTranslator or CShapeTranslator MtoA class.

  
  - SupportVolumes() [default: False]
    Returns whether or not the translated node can be used to generate volume bounds ('box' node with step_size > 0).

  
  - SupportInstances() [default: False]
    Returns whether or not ginstance should be created if shape is instanced in maya.

  
  - Export(renderFrame, mbStep, mbSampleFrame, nodeName, isInstance, masterNodeName) [required]
    Returns a list of attributes that have been explicitly set in the function 

  
  - Cleanup(nodeName, isInstance, masterNodeName)
    Called on last export step to cleanup any internally maintained data.

  
  - SetupAttrs()
    
    Returns a list of string describing the attributes that are to be added to every node of that type (use scriptedTranslatorUtils.AttrData).
    
    Example:
    
    def SetupAttrs():
      import scriptedTranslatorUtils as stu
      import arnold
      
      attrs = []
      
      # Add step_size parameter from arnold box node
      attrs.append(stu.AttrData(arnoldNode="box", arnoldAttr="step_size"))
      
      attrs.append(stu.AttrData(name="aStringAttribute", shortName="asa", type=arnold.AI_TYPE_STRING, defaultValue=""))
      
      attrs.append(stu.AttrData(name="aFloatAttribute", shortName="afa", type=arnold.AI_TYPE_FLOAT, defaultValue=0.0, softMin=0.0, softMax=1.0, max=10.0))
      
      attrs.append(stu.AttrData(name="anEnumAttribute", shortName="aea", type=arnold.AI_TYPE_ENUM, enums=["aaa", "bbb", "ccc"], defaultValue=1))
      
      return map(str, attrs)
  
  
  - SetupAE(translatorName)
    
    Attribute editor setup
    Code should look something like this for a shape translator:
    
    def SetupAE(translatorName):
      import mtoa.ui.ae.templates as templates
      
      class MyNodeTtemplate(templates.ShapeTranslatorTemplate):
        def setup(self):
          # Setup attributes automatically added by MtoA
          self.commonShapeAttributes()
          self.addSeparator()
          self.renderStatsAttributes()
          self.addSeparator()
          self.addControl("aiUserOptions", label="User Options")
          self.addSeparator()
          
          # Setup myNode specific attributes (those declared in SetupAttrs function)
          self.addControl("aiStepSize", label="Volume Step Size")
          self.addControl("aStringAttribute")
          self.addControl("aFloatAttribute")
          self.addControl("anEnumAttribute")
    
      templates.registerTranslatorUI(MyNodeTtemplate, "myNode", translatorName)


  The extension will take care of creating the arnold nodes depending on 
  IsShape/SupportVolumes/SupportInstances function return value:
  
    Only 'procedural' are generated if 'IsShape' is defined and returned false.
    For shape nodes:
      If the node aiStepSize attribute is defined and its value is greater than 0: a 'box' node is generated for the master node instance and 'ginstance' node for all secondary instances when 'SupportVolumes is not defined or returned false.
      In any other case, a 'procedural' node is always generated for the master instance. For secondary instances, a 'ginstance' is generated if 'SupportInstances' function is defined and returned true; a 'procedural' node otherwise.
