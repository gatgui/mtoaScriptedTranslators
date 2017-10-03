import os
import re
import sys
import glob
import excons
from excons.tools import arnold
from excons.tools import maya

# Enfore the right visual studio version on windows
maya.SetupMscver()

env = excons.MakeBaseEnv()

defs = []
ext = ""
if sys.platform == "darwin":
   defs.append("_DARWIN")
   ext = ".dylib"
   env.Append(CPPFLAGS=" -Wno-unused-parameter")
elif sys.platform == "win32":
   defs.append("_WIN32")
   ext = ".dll"
else:
   defs.append("_LINUX")
   ext = ".so"
   env.Append(CPPFLAGS=" -Wno-unused-parameter")

mtoa_inc = excons.GetArgument("with-mtoa-inc")
mtoa_lib = excons.GetArgument("with-mtoa-lib")
mtoa_base = excons.GetArgument("with-mtoa")

if (mtoa_inc is None or mtoa_lib is None) and mtoa_base is None:
   print("Please provide mtoa SDK directory using with-mtoa or with-mtoa-inc/with-mtoa-lib flags")
   sys.exit(1)

if mtoa_inc is None:
   mtoa_inc = mtoa_base + "/include"

if mtoa_lib is None:
   mtoa_lib = mtoa_base + ("/lib" if sys.platform == "win32" else "/bin")

def GetMtoAVersion():
  version_header = mtoa_inc + "/utils/Version.h"
  if os.path.isfile(version_header):
    arch = None
    major = None
    numexp = re.compile("^\s*#define\s+MTOA_(ARCH|MAJOR)_VERSION_NUM\s+(\d+)")
    with open(version_header, "r") as h:
      for l in h.readlines():
        m = numexp.match(l.strip())
        if m:
          if m.group(1) == "ARCH":
            arch = int(m.group(2))
          elif m.group(1) == "MAJOR":
            major = int(m.group(2))
    if arch is not None and major is not None:
      return "%d.%d" % (arch, major)
  return ""

maya_ver = maya.Version(asString=False, nice=True)
if maya_ver < 2015:
  print("Maya 2015 at least is required")
  sys.exit(1)

arnold_ver = arnold.Version(asString=False)
if arnold_ver[0] < 4 or (arnold_ver[0] == 4 and (arnold_ver[1] < 2 or (arnold_ver[1] == 2 and arnold_ver[2] < 12))):
  print("Arnold 4.2.12.0 at least is required")
  sys.exit(1)

mtoa_ver = GetMtoAVersion()
if float(mtoa_ver) <= 1.3:
  print("MtoA 1.4 at least is required.")
  sys.exit(1)

prefix = "maya/%s/mtoa-%s" % (maya.Version(nice=True), mtoa_ver)
if maya_ver < 2017:
  print("Don't use c++11")
else:
  print("Maya version = %s" % maya.Version(asString=False))

prj = {"name": "scriptedTranslators",
       "type": "dynamicmodule",
       "prefix": prefix,
       "ext": ext,
       "defs": defs,
       "srcs": glob.glob("src/*.cpp"),
       "incdirs": [mtoa_inc],
       "libdirs": [mtoa_lib],
       "libs": ["mtoa_api"],
       "install": {"maya/python": glob.glob("python/*.py")},
       "custom": [arnold.Require, maya.Require]}

excons.DeclareTargets(env, [prj])
