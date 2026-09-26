using System;
using System.IO;
using System.Linq;
public static class SetupTests {
 static void Check(bool ok,string message){if(!ok)throw new Exception(message);}
 static void Reject(Action action){bool rejected=false;try{action();}catch{rejected=true;}Check(rejected,"Expected rejection");}
 public static int Main(string[] args){
  string root=Path.GetFullPath(args[0]);Directory.CreateDirectory(root);
  string exe=args[1];int id=0;
  Func<string> fresh=()=>{string d=Path.Combine(root,"case-"+(++id));Directory.CreateDirectory(d);File.Copy(exe,Path.Combine(d,"DXHRDC.exe"));return d;};
  string bad=Path.Combine(root,"bad");Directory.CreateDirectory(bad);File.WriteAllText(Path.Combine(bad,"DXHRDC.exe"),"unsupported");Reject(()=>SetupCore.Install(bad));Check(!Directory.Exists(Path.Combine(bad,"StealthHUD.Backup")),"Invalid build changed files");
  string clean=fresh();SetupCore.Install(clean);Check(SetupCore.HashFile(Path.Combine(clean,SetupCore.Mod))==SetupCore.Hash(SetupCore.Payload(SetupCore.Mod)),"Wrong payload");Reject(()=>SetupCore.Install(clean));SetupCore.Restore(clean);Check(!File.Exists(Path.Combine(clean,SetupCore.Mod)),"Fresh ASI not removed");Check(File.Exists(Path.Combine(clean,"winmm.dll")),"Shared loader removed");Check(SetupCore.HashFile(Path.Combine(clean,"DXHRDC.exe"))==SetupCore.ExeHash,"EXE changed");
  string existing=fresh();File.WriteAllText(Path.Combine(existing,SetupCore.Mod),"original plugin");File.WriteAllText(Path.Combine(existing,"DXHRDC-GFX.ini"),"custom settings");SetupCore.Install(existing);Check(File.ReadAllText(Path.Combine(existing,"DXHRDC-GFX.ini"))=="custom settings","Config overwritten");SetupCore.Restore(existing);Check(File.ReadAllText(Path.Combine(existing,SetupCore.Mod))=="original plugin","Original not restored");
  string conflict=fresh();File.WriteAllText(Path.Combine(conflict,"winmm.dll"),"another loader");Reject(()=>SetupCore.Install(conflict));Check(!File.Exists(Path.Combine(conflict,SetupCore.Mod)),"Loader conflict modified ASI");
  string edited=fresh();SetupCore.Install(edited);File.WriteAllText(Path.Combine(edited,SetupCore.Mod),"user change");Reject(()=>SetupCore.Restore(edited));Check(File.ReadAllText(Path.Combine(edited,SetupCore.Mod))=="user change","Changed mod overwritten");
  Console.WriteLine("PASS: clean install, executable hash guard, reinstall guard, original restoration, settings preservation, loader conflict rejection, changed-file protection and shared-loader retention.");return 0;
 }
}
