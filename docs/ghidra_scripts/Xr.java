import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.io.*;
import java.util.*;
// args: out addr... : lista funciones que referencian cada direccion
public class Xr extends GhidraScript {
  public void run() throws Exception {
    String[] a=getScriptArgs(); PrintWriter pw=new PrintWriter(new FileWriter(a[0],true));
    for(int i=1;i<a.length;i++){ Address ad=toAddr(a[i]); TreeMap<String,Integer> m=new TreeMap<>();
      for(Reference r: getReferencesTo(ad)){ Function f=getFunctionContaining(r.getFromAddress());
        String k=f==null? "nofunc@"+r.getFromAddress() : f.getEntryPoint()+" "+f.getName(); m.merge(k,1,Integer::sum);}
      pw.println("## "+a[i]+" ("+m.size()+" funcs)"); for(String k: m.keySet()) pw.println(k+" x"+m.get(k));}
    pw.close();}
}
