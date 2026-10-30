import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import java.io.*;
// args: out addr n : vuelca n punteros de una vtable
public class Vt extends GhidraScript {
  public void run() throws Exception {
    String[] a=getScriptArgs(); PrintWriter pw=new PrintWriter(new FileWriter(a[0],true));
    for(int j=1;j+1<a.length;j+=2){
    Address v=toAddr(a[j]); int n=Integer.parseInt(a[j+1]); pw.println("## vt "+v);
    for(int i=0;i<n;i++){ int p=getInt(v.add(i*4)); Address t=toAddr(p&0xffffffffL);
      Function f=getFunctionAt(t); pw.println(String.format("+0x%x %s %s",i*4,t,f!=null?f.getName():""));}}
    pw.close();}
}
