import os
import pymel.core as pm

for item in os.environ.get("MTOA_SCRIPTED_TRANSLATORS", "").split(os.pathsep):
   
   spl = map(lambda x: x.strip(), item.split(","))
   n = len(spl)
   
   if n <= 2:
      nodeType = spl[0]
      plugin = (nodeType if n == 1 else spl[1])
      
      try:
         translatorMod = __import__("mtoa_%s" % nodeType)
      except:
         continue
      
      if hasattr(translatorMod, "SetupAE"):
         translatorMod.SetupAE("scriptedTranslators")
         
      else:
         try:
            import scriptedTranslatorUtils
            
            isShape = True
            if hasattr(translatorMod, "IsShape"):
               isShape = translatorMod.IsShape()
            
            scriptedTranslatorUtils.DefaultSetupAE(plugin, nodeType, "scriptedTranslators", isShape)
            
         except Exception, e:
            pass
