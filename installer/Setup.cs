using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Reflection;
using System.Security.Cryptography;
using System.Xml.Linq;
using System.Diagnostics;
using System.Drawing;
using System.Windows.Forms;
using Microsoft.Win32;

public static class SetupCore {
    public const string ExeHash="8266b6b4a5bf25f2f4e8de068aa3720f6289c962bb1c2bb70a7b1c111ba510a1";
    public const string Mod="DXHRDC-GFX.asi";
    public static string Hash(byte[] bytes){using(var s=SHA256.Create())return BitConverter.ToString(s.ComputeHash(bytes)).Replace("-","").ToLowerInvariant();}
    public static string HashFile(string p){return Hash(File.ReadAllBytes(p));}
    public static byte[] Payload(string name){using(var s=Assembly.GetExecutingAssembly().GetManifestResourceStream("payload."+name)){if(s==null)throw new Exception("Missing embedded file: "+name);using(var m=new MemoryStream()){s.CopyTo(m);return m.ToArray();}}}
    static readonly string[] Names={Mod,"winmm.dll","StealthHUD.ini","DXHRDC-GFX.ini","LICENSE-GFX.txt","LICENSE-ModUtils.txt","LICENSE-ImGui.txt","LICENSE-ASI-Loader.txt","LICENSE-WIL.txt","README.txt"};
    static string Relative(string n){return n.StartsWith("LICENSE-")||n=="README.txt"?"StealthHUD\\"+n:n;}
    static void AtomicWrite(string path,byte[] bytes){
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        string temp=path+".stealthhud-"+Guid.NewGuid().ToString("N")+".tmp";
        try{File.WriteAllBytes(temp,bytes);if(File.Exists(path))File.Replace(temp,path,null);else File.Move(temp,path);}
        finally{if(File.Exists(temp))File.Delete(temp);}
    }
    public static void Validate(string dir){
        string exe=Path.Combine(dir,"DXHRDC.exe");
        if(!File.Exists(exe))throw new Exception("Select the game folder containing DXHRDC.exe.");
        if(HashFile(exe)!=ExeHash)throw new Exception("Unsupported game executable. This release supports the verified Steam Director's Cut build only. No game files were changed.");
    }
    public static void Install(string dir){
        dir=Path.GetFullPath(dir);Validate(dir);
        string backup=Path.Combine(dir,"StealthHUD.Backup"),manifest=Path.Combine(backup,"manifest.xml");
        if(Directory.Exists(backup))throw new Exception("An installer backup already exists. Use Restore first, or inspect StealthHUD.Backup if a previous installation was interrupted.");
        string loader=Path.Combine(dir,"winmm.dll");
        if(File.Exists(loader)&&HashFile(loader)!=Hash(Payload("winmm.dll")))throw new Exception("A different winmm.dll already exists. It was not replaced. Resolve the ASI loader compatibility before installing.");
        var entries=new List<XElement>();
        // Read and validate every payload before touching the destination.
        var payload=Names.ToDictionary(n=>n,n=>Payload(n));
        foreach(var n in Names){
            string rel=Relative(n),dest=Path.Combine(dir,rel);
            if(n!=Mod && File.Exists(dest))continue;
            entries.Add(new XElement("file",new XAttribute("name",rel),new XAttribute("hash",Hash(payload[n])),new XAttribute("existed",File.Exists(dest)),new XAttribute("originalHash",File.Exists(dest)?HashFile(dest):"")));
        }
        Directory.CreateDirectory(backup);
        try{
            foreach(var e in entries)if((bool)e.Attribute("existed")){
                string rel=(string)e.Attribute("name"),dest=Path.Combine(backup,rel);
                Directory.CreateDirectory(Path.GetDirectoryName(dest));File.Copy(Path.Combine(dir,rel),dest,false);
            }
            new XDocument(new XElement("install",new XAttribute("version","0.3.1"),entries)).Save(manifest);
            foreach(var e in entries){string rel=(string)e.Attribute("name");AtomicWrite(Path.Combine(dir,rel),payload[Path.GetFileName(rel)]);}
        }catch{
            // Roll back exactly this transaction, including earlier successful writes.
            foreach(var e in entries.AsEnumerable().Reverse()){
                string rel=(string)e.Attribute("name"),dest=Path.Combine(dir,rel),old=Path.Combine(backup,rel);
                if((bool)e.Attribute("existed")){if(File.Exists(old))AtomicWrite(dest,File.ReadAllBytes(old));}
                else if(File.Exists(dest)&&HashFile(dest)==(string)e.Attribute("hash"))File.Delete(dest);
            }
            if(File.Exists(manifest))File.Move(manifest,Path.Combine(backup,"failed-install.xml"));
            throw;
        }
    }
    public static string Restore(string dir){
        dir=Path.GetFullPath(dir);
        if(!File.Exists(Path.Combine(dir,"DXHRDC.exe")))throw new Exception("Select the original game folder.");
        string backup=Path.Combine(dir,"StealthHUD.Backup"),manifest=Path.Combine(backup,"manifest.xml");
        if(!File.Exists(manifest))throw new Exception("No active installer backup found in this folder.");
        var entries=XDocument.Load(manifest).Root.Elements("file").ToArray();
        var allowed=new HashSet<string>(Names.Select(Relative),StringComparer.OrdinalIgnoreCase);
        foreach(var e in entries){
            string rel=(string)e.Attribute("name");
            if(!allowed.Contains(rel))throw new Exception("Unexpected backup entry. Restore stopped.");
            string dest=Path.Combine(dir,rel),old=Path.Combine(backup,rel);
            // User-edited configuration and shared loader are intentionally retained.
            if(rel.EndsWith(".ini")||rel=="winmm.dll")continue;
            if(File.Exists(dest)&&HashFile(dest)!=(string)e.Attribute("hash"))throw new Exception("File changed since installation: "+rel+". Restore stopped to preserve your changes.");
            if((bool)e.Attribute("existed")&&(!File.Exists(old)||HashFile(old)!=(string)e.Attribute("originalHash")))throw new Exception("Backup missing or changed: "+rel);
        }
        foreach(var e in entries.Reverse()){
            string rel=(string)e.Attribute("name");
            if(rel.EndsWith(".ini")||rel=="winmm.dll")continue;
            string dest=Path.Combine(dir,rel);
            if((bool)e.Attribute("existed"))AtomicWrite(dest,File.ReadAllBytes(Path.Combine(backup,rel)));
            else if(File.Exists(dest))File.Delete(dest);
        }
        // Preserve the audit trail and originals; allow a later fresh install.
        string archive=backup+".restored-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss-ffff");
        Directory.Move(backup,archive);
        return "Previous graphics plugin restored (or removed if none existed). Configuration files and the shared ASI loader were retained. Backup archived at: "+archive;
    }
}

public sealed class SetupForm:Form {
    TextBox path=new TextBox();Label status=new Label();Button install=new Button(),restore=new Button();
    public SetupForm(){
        Text="Deus Ex HR DC - Stealth HUD Setup 0.3.1";ClientSize=new Size(660,360);FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;StartPosition=FormStartPosition.CenterScreen;Font=new Font("Segoe UI",10);
        Controls.Add(new Label{Text="Stealth HUD",Font=new Font("Segoe UI",20,FontStyle.Bold),Location=new Point(22,18),Size=new Size(610,40)});
        Controls.Add(new Label{Text="In-game achievement and stealth-condition indicators.\nSteam Director's Cut / DirectX 11 / Windows 10 or later.",Location=new Point(24,65),Size=new Size(610,50)});
        Controls.Add(new Label{Text="Game folder (contains DXHRDC.exe)",Location=new Point(24,126),Size=new Size(600,25)});
        path.SetBounds(24,154,505,28);path.Text=FindGame();Controls.Add(path);
        var browse=new Button{Text="Browse...",Location=new Point(540,152),Size=new Size(95,30)};browse.Click+=(s,e)=>{using(var b=new FolderBrowserDialog()){b.SelectedPath=path.Text;b.Description="Select the Deus Ex Human Revolution Director's Cut folder";if(b.ShowDialog()==DialogResult.OK)path.Text=b.SelectedPath;}};Controls.Add(browse);
        Controls.Add(new Label{Text="Includes a modified DXHRDC-GFX plugin and ASI Loader. Existing graphics\nsettings are preserved. A backup is created before replacement. Close the game first.",Location=new Point(24,195),Size=new Size(610,55)});
        install.Text="Install";install.SetBounds(24,260,140,36);install.Click+=(s,e)=>Run(false);Controls.Add(install);
        restore.Text="Restore previous";restore.SetBounds(175,260,170,36);restore.Click+=(s,e)=>Run(true);Controls.Add(restore);
        status.SetBounds(24,310,610,40);status.Text="F9: expanded details   |   F10: hide/show   |   F11: graphics settings";Controls.Add(status);
    }
    void Run(bool undo){
        install.Enabled=restore.Enabled=false;
        try{
            if(Process.GetProcessesByName("DXHRDC").Length!=0)throw new Exception("Close Deus Ex before installing or restoring.");
            string message;if(undo)message=SetupCore.Restore(path.Text);else{SetupCore.Install(path.Text);message="Installed successfully. Enable DirectX 11 in the game settings, then launch through Steam. F9 opens details; F10 hides the HUD.";}
            status.Text=undo?"Previous installation restored.":"Installed successfully.";MessageBox.Show(this,message,"Stealth HUD",MessageBoxButtons.OK,MessageBoxIcon.Information);
        }catch(UnauthorizedAccessException){MessageBox.Show(this,"Windows denied write access. Close the game and run this installer as administrator if the Steam library requires it.","Stealth HUD",MessageBoxButtons.OK,MessageBoxIcon.Error);}
        catch(Exception ex){MessageBox.Show(this,ex.Message,"Stealth HUD",MessageBoxButtons.OK,MessageBoxIcon.Error);}
        finally{install.Enabled=restore.Enabled=true;}
    }
    static string FindGame(){
        var libraries=new List<string>();
        using(var key=Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam")){if(key!=null){var s=key.GetValue("SteamPath") as string;if(!String.IsNullOrEmpty(s))libraries.Add(s);}}
        libraries.Add(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),"Steam"));
        foreach(var root in libraries.ToArray()){
            string vdf=Path.Combine(root,@"steamapps\libraryfolders.vdf");
            if(!File.Exists(vdf))continue;
            foreach(System.Text.RegularExpressions.Match m in System.Text.RegularExpressions.Regex.Matches(File.ReadAllText(vdf),"\"path\"\\s+\"([^\"]+)\""))libraries.Add(m.Groups[1].Value.Replace(@"\\",@"\"));
        }
        foreach(var lib in libraries){string p=Path.Combine(lib,@"steamapps\common\Deus Ex Human Revolution Director's Cut");if(File.Exists(Path.Combine(p,"DXHRDC.exe")))return p;}
        return "";
    }
}
public static class Entry {
    [STAThread] public static void Main(){Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new SetupForm());}
}
