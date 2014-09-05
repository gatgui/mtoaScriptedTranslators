import sys
import glob
import excons
from excons.tools import arnold
from excons.tools import maya

env = excons.MakeBaseEnv()

defs = []
ext = ""
if sys.platform == "darwin":
   defs.append("_DARWIN")
   ext = ".dylib"
elif sys.platform == "win32":
   defs.append("_WIN32")
   ext = ".dll"
else:
   defs.append("_LINUX")
   ext = ".so"

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


prj = {"name": "scriptedTranslators",
       "type": "dynamicmodule",
       "ext": ext,
       "defs": defs,
       "srcs": glob.glob("src/*.cpp"),
       "incdirs": [mtoa_inc],
       "libdirs": [mtoa_lib],
       "libs": ["mtoa_api"],
       "install": {"": glob.glob("python/*.py")},
       "custom": [arnold.Require, maya.Require]}

excons.DeclareTargets(env, [prj])
