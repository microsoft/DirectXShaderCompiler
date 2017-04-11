import urllib
import os
import zipfile

url = "https://github.com/Microsoft/WinObjC/raw/develop/deps/prebuilt/nuget/taef.redist.wlk.1.0.170206001-nativetargets.nupkg"
zipfile_name = os.path.join(os.environ['TEMP'], "taef.redist.wlk.1.0.170206001-nativetargets.nupkg.zip")
src_dir = os.environ['HLSL_SRC_DIR']
taef_dir = os.path.join(src_dir, "external", "taef")

if not os.path.isdir(taef_dir):
  os.makedirs(taef_dir)

try:
  urllib.urlretrieve(url, zipfile_name)
except:
  print("Unable to read file with urllib, trying via powershell...")
  from subprocess import check_call, check_output
  cmd = "(new-object System.Net.WebClient).DownloadFile('" + url + "', '" + zipfile_name + "')"
  check_call(['powershell.exe', '-Command', cmd])

z = zipfile.ZipFile(zipfile_name)
z.extractall(taef_dir)
z.close()
